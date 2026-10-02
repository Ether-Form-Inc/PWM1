# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ether Form Inc.
"""Talks to a can_to_pwm node through the serial USB-CAN adapter, using the reference DroneCAN library
(pip install dronecan pyserial) to build the requests and decode the replies. One CanLink holds the serial port for its
lifetime, so use a single one per process: two programs sending at once interleave their frames on the bus.

    link = CanLink("/dev/ttyUSB0", node=10)
    link.get("PWM_PRESCALE")          -> Param(name, value, default, min, max)
    link.set("PWM_PRESCALE", 130)
    link.node_info()                  -> NodeInfo(...)
"""
import collections
import sys
import time

import dronecan
import dronecan.transport as transport
import serial

SELF = 50  # our own node ID on the bus

Param = collections.namedtuple("Param", "name value default min max")
NodeInfo = collections.namedtuple(
    "NodeInfo", "health uptime_s vendor_status sw_major sw_minor commit hw_major hw_minor unique_id name")


class LinkError(Exception):
    pass


class CanLink:
    def __init__(self, device="/dev/ttyUSB0", node=10, attempt_timeout=0.5):
        self.node = node
        self.attempt_timeout = attempt_timeout
        self.port = serial.Serial(None, 2000000, timeout=0.02, stopbits=serial.STOPBITS_TWO)
        self.port.port = device
        self.port.dtr = True   # the adapter only streams while DTR and RTS are asserted
        self.port.rts = True
        self.port.open()
        cfg = bytearray([0xAA, 0x55, 0x12, 0x01, 0x01] + [0] * 8 + [0x00, 0x01, 0, 0, 0, 0])  # 1 Mbit/s, normal
        cfg.append(sum(cfg[2:]) & 0xFF)
        self.port.write(cfg)
        time.sleep(0.3)
        self.port.reset_input_buffer()
        self.manager = transport.TransferManager()
        self.buf = bytearray()
        self.transfer_id = int(time.time() * 7) & 31

    def close(self):
        self.port.close()

    def _send(self, payload, timeout):
        """Sends one service request; returns the reply payload or None."""
        tr = transport.Transfer(transfer_id=self.transfer_id, source_node_id=SELF, dest_node_id=self.node,
                                payload=payload, transfer_priority=20,
                                request_not_response=True, service_not_message=True)
        # A node discards a repeat of the previous transfer ID, so use a fresh one for every attempt
        self.transfer_id = (self.transfer_id + 1) & 31
        for frame in tr.to_frames():
            self.port.write(bytes([0xAA, 0xE0 | len(frame.bytes)]) + frame.message_id.to_bytes(4, "little")
                            + bytes(frame.bytes) + b"\x55")
            time.sleep(0.003)   # the adapter drops frames sent back to back
        deadline = time.time() + timeout
        while time.time() < deadline:
            self.buf += self.port.read(4096)
            i = 0
            while i < len(self.buf):
                if self.buf[i] == 0xAA and i + 1 < len(self.buf) and (self.buf[i + 1] & 0xC0) == 0xC0:
                    end = i + 6 + (self.buf[i + 1] & 15)
                    if end >= len(self.buf):
                        break
                    if self.buf[end] == 0x55:
                        frame = transport.Frame(int.from_bytes(self.buf[i + 2:i + 6], "little"),
                                                bytes(self.buf[i + 6:end]))
                        frames = self.manager.receive_frame(frame)
                        if frames:
                            try:
                                done = transport.Transfer()
                                done.from_frames(frames)
                            except transport.TransferError:
                                done = None
                            if done is not None and done.source_node_id == self.node and done.dest_node_id == SELF \
                                    and not done.request_not_response:
                                del self.buf[:end + 1]
                                return done.payload
                        i = end + 1
                        continue
                i += 1
            del self.buf[:i]
        return None

    def request(self, payload, tries=12, timeout=None):
        """Repeats a request until it is answered: the USB-CAN adapter drops some frames."""
        for _ in range(tries):
            reply = self._send(payload, timeout or self.attempt_timeout)
            if reply is not None:
                return reply
        raise LinkError("no reply from node %d" % self.node)

    # ---- parameters ----
    @staticmethod
    def _param(res):
        name = res.name.decode() if isinstance(res.name, (bytes, bytearray)) else str(res.name)
        if not name:
            return None
        return Param(name, res.value.integer_value, res.default_value.integer_value,
                     res.min_value.integer_value, res.max_value.integer_value)

    def getset(self, name="", index=0, value=None, tries=12):
        req = dronecan.uavcan.protocol.param.GetSet.Request(index=index, name=name)
        if value is not None:
            req.value = dronecan.uavcan.protocol.param.Value(integer_value=value)
        return self._param(self.request(req, tries=tries))

    def get(self, name, tries=12):
        p = self.getset(name=name, tries=tries)
        if p is None:
            raise LinkError("no such parameter: " + name)
        return p

    def set(self, name, value, tries=12):
        """Writes a parameter and returns the Param as the node reports it afterwards (it keeps its old value if the
        new one was refused)."""
        p = self.getset(name=name, value=value, tries=tries)
        if p is None:
            raise LinkError("no such parameter: " + name)
        return p

    def params(self):
        out, index = [], 0
        while True:
            p = self.getset(index=index)
            if p is None:
                return out
            out.append(p)
            index += 1

    # ---- services ----
    def erase_all(self):
        res = self.request(dronecan.uavcan.protocol.param.ExecuteOpcode.Request(opcode=1))
        return bool(res.ok)

    def restart(self):
        res = self.request(dronecan.uavcan.protocol.RestartNode.Request(
            magic_number=dronecan.uavcan.protocol.RestartNode.Request().MAGIC_NUMBER))
        return bool(res.ok)

    def node_info(self):
        res = self.request(dronecan.uavcan.protocol.GetNodeInfo.Request())
        sw, hw = res.software_version, res.hardware_version
        commit = sw.vcs_commit if sw.optional_field_flags & 1 else None
        uid = bytes(hw.unique_id).hex() if hasattr(hw.unique_id, "__iter__") else str(hw.unique_id)
        name = res.name.decode() if isinstance(res.name, (bytes, bytearray)) else str(res.name)
        return NodeInfo(res.status.health, res.status.uptime_sec, res.status.vendor_specific_status_code,
                        sw.major, sw.minor, commit, hw.major, hw.minor, uid, name)

    # ---- the PWM bench measurement (board/Stm_pwm_meter.h) ----
    def measure(self, output, wait=6.0):
        """Times PWM output 0-3 with the node's timer. Returns (period_us, high_us). Raises LinkError if the node
        refuses (it has received commands) or sees no pulses."""
        self.set("PWM_MEASURE", output + 1)
        time.sleep(1.2)   # the node is busy for about a second and does not answer meanwhile
        deadline = time.time() + wait
        while time.time() < deadline:
            try:
                state = self.get("PWM_MEASURE", tries=3).value
            except LinkError:
                continue
            if state == 2:
                return self.get("PWM_PERIOD_US").value, self.get("PWM_HIGH_US").value
            if state == 3:
                raise LinkError("the node refused or failed the measurement of output %d (it refuses once it has "
                                "received a command since it started, or if the PCA9685 is not answering)" % output)
            time.sleep(0.3)
        raise LinkError("measurement of output %d did not finish" % output)
