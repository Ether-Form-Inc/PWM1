#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ether Form Inc.
"""Programs a can_to_pwm board over SWD with OpenOCD, then checks that it works.

For one board connected to an ST-LINK (and, optionally, to a serial USB-CAN adapter) it:
  1. reads the settings pages (stored node ID, pulse width limits and motor timeout), the J5 pins and the chip's
     unique ID,
  2. programs and verifies the firmware (the settings pages are not touched),
  3. optionally stores a node ID (--set-node-id) or clears the settings, then resets the board and works out which
     node ID it should be using (the stored ID, else the default of 10),
  4. asks that node for its GetNodeInfo over CAN and checks the reply and its health,
and prints PASS or FAIL. The exit code is 0 only for PASS.

Examples:
  tools/flash_and_check.py                      # program the newest build, then check over CAN
  tools/flash_and_check.py --can-device none    # program only, no CAN check
  tools/flash_and_check.py --check-only         # do not program: just read and check the board
  tools/flash_and_check.py --set-node-id 42     # also store node ID 42 (1 to 127) in the board's flash
  tools/flash_and_check.py --clear-node-id      # also remove a stored node ID (back to the default of 10)
  tools/flash_and_check.py --clear-settings     # also erase the node ID, pulse width limits and motor timeout

OpenOCD: set OPENOCD to the executable and OPENOCD_SCRIPTS to its script directory if they are not on the PATH. If
OpenOCD is not found, the copy that ships with STM32CubeIDE (/opt/st) is used if present. That is ST's fork and needs
the default --interface stlink-dap.cfg; a stock OpenOCD wants --interface stlink.cfg --stock.
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)

NODE_ID_PAGE = (62, 0x0801F000)      # one byte, 1 to 127
PWM_PAGE = (61, 0x0801E800)          # 20 halfwords, see below
DEFAULT_NODE_ID = 10
GPIOB_IDR, GPIOA_IDR = 0x50000410, 0x50000010
UID_ADDRESS = 0x1FFF7590

# Names of the halfwords in the settings page (0xFFFF = not set, the default is used)
PWM_NAMES = ([f"SERVO_MIN_US_{i}" for i in range(4)] + [f"SERVO_MAX_US_{i}" for i in range(4)] +
             [f"MOTOR_MIN_US_{i}" for i in range(4)] + [f"MOTOR_MAX_US_{i}" for i in range(4)] +
             ["MOTOR_TIMEOUT_MS"] + ["(unused)"] * 3)


def newest_elf():
    found = glob.glob(os.path.join(REPO, "build*", "stm_generated", "can_pwm_firmware.elf"))
    return max(found, key=os.path.getmtime) if found else None


def find_openocd():
    """The OpenOCD to use when none is given: the one on the PATH, else the one in STM32CubeIDE."""
    if shutil.which("openocd"):
        return "openocd", os.environ.get("OPENOCD_SCRIPTS")
    for root in sorted(glob.glob("/opt/st/stm32cubeide_*"), reverse=True):
        exe = glob.glob(os.path.join(root, "plugins", "com.st.stm32cube.ide.mcu.externaltools.openocd.linux64_*",
                                     "tools", "bin", "openocd"))
        scripts = glob.glob(os.path.join(root, "plugins", "com.st.stm32cube.ide.mcu.debug.openocd_*", "resources",
                                         "openocd", "st_scripts"))
        if exe and scripts:
            return exe[0], scripts[0]
    return "openocd", os.environ.get("OPENOCD_SCRIPTS")


def parse_args():
    default_exe, default_scripts = find_openocd()
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", default=newest_elf(), help="firmware to program (default: the newest build)")
    ap.add_argument("--openocd", default=os.environ.get("OPENOCD", default_exe))
    ap.add_argument("--scripts", default=os.environ.get("OPENOCD_SCRIPTS", default_scripts),
                    help="OpenOCD script directory")
    ap.add_argument("--interface", default="stlink-dap.cfg", help="OpenOCD interface file (default stlink-dap.cfg)")
    ap.add_argument("--stock", action="store_true", help="stock OpenOCD (no 'transport select dapdirect_swd')")
    ap.add_argument("--probe-serial", help="select one ST-LINK when several are connected")
    ap.add_argument("--can-device", default="/dev/ttyUSB0", help="USB-CAN adapter, or 'none' to skip the CAN check")
    ap.add_argument("--check-only", action="store_true", help="do not program, only read and check")
    ap.add_argument("--set-node-id", type=int, metavar="N",
                    help="store node ID N (1 to 127) in the board's flash; 0 removes it (default node ID 10)")
    ap.add_argument("--clear-node-id", action="store_true", help="same as --set-node-id 0")
    ap.add_argument("--clear-settings", action="store_true",
                    help="erase the stored node ID, pulse width limits and motor timeout")
    ap.add_argument("--boot-wait", type=float, default=3.0, help="seconds to wait after the reset")
    return ap.parse_args()


class Probe:
    def __init__(self, args):
        self.args = args

    def run(self, commands, timeout=120):
        a = self.args
        cmd = [a.openocd]
        if a.scripts:
            cmd += ["-s", a.scripts]
        cmd += ["-f", "interface/" + a.interface]
        if not a.stock:
            cmd += ["-c", "transport select dapdirect_swd"]
        if a.probe_serial:
            cmd += ["-c", "adapter serial " + a.probe_serial]
        cmd += ["-f", "target/stm32g0x.cfg", "-c", "init"]
        for c in commands:
            cmd += ["-c", c]
        cmd += ["-c", "shutdown"]
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        except FileNotFoundError:
            sys.exit("FAIL: OpenOCD not found (%s). Set OPENOCD / OPENOCD_SCRIPTS or use --openocd." % a.openocd)
        except subprocess.TimeoutExpired:
            return None
        return r.stdout + r.stderr

    def read_state(self):
        """Halts the core, reads the settings, the J5 pins and the unique ID, then lets it run again."""
        out = self.run(["catch {halt}",
                        'echo "ID [read_memory 0x%08X 32 2]"' % NODE_ID_PAGE[1],
                        'echo "PWM [read_memory 0x%08X 32 10]"' % PWM_PAGE[1],
                        'echo "PINS [read_memory 0x%08X 32 1] [read_memory 0x%08X 32 1]"' % (GPIOB_IDR, GPIOA_IDR),
                        'echo "UID [read_memory 0x%08X 32 3]"' % UID_ADDRESS,
                        "resume"])
        if out is None:
            return None
        state = {}
        for line in out.splitlines():
            m = re.match(r"^(ID|PWM|PINS|UID) (.*)$", line.strip())
            if m:
                state[m.group(1)] = [int(x, 16) for x in m.group(2).split()]
        return state if {"ID", "PWM", "PINS", "UID"} <= set(state) else None


def stored_byte(words):
    """The one byte setting held in a flash page, or None if the page is erased."""
    raw = (words[1] << 32) | words[0]
    value = raw & 0xFF
    return None if raw == 0xFFFFFFFFFFFFFFFF or value == 0xFF else value


def pwm_values(words):
    """The 20 halfwords of the settings page."""
    out = []
    for w in words:
        out += [w & 0xFFFF, w >> 16]
    return out


def stored_settings(state):
    """{name: value} for every PWM setting that has been stored."""
    return {n: v for n, v in zip(PWM_NAMES, pwm_values(state["PWM"])) if v != 0xFFFF and n != "(unused)"}


def expected_node_id(state):
    stored = stored_byte(state["ID"])
    if stored is not None and 1 <= stored <= 127:
        return stored, "stored in flash"
    return DEFAULT_NODE_ID, "default"


def jumpers(state):
    """(servo mode?, actuator ID offset) from the J5 pins; a fitted jumper pulls its pin low."""
    gpiob, gpioa = state["PINS"]
    servo = not (gpioa >> 8) & 1
    # J5 row 2 = PB2 = bit 0, row 3 = PB1 = bit 1, row 4 = PB0 = bit 2
    offset = (1 - ((gpiob >> 2) & 1)) | (1 - ((gpiob >> 1) & 1)) << 1 | (1 - (gpiob & 1)) << 2
    return servo, offset


def describe(state):
    node = stored_byte(state["ID"])
    settings = stored_settings(state)
    servo, offset = jumpers(state)
    uid = "".join("%08x" % w for w in reversed(state["UID"]))
    text = ["stored node ID: %s" % ("none (default %d)" % DEFAULT_NODE_ID if node is None else node),
            "J5: %s mode, actuator ID offset %d" % ("servo" if servo else "motor", offset),
            "stored PWM settings: %s" % (", ".join("%s=%d" % kv for kv in settings.items()) if settings
                                         else "none (defaults)"),
            "chip unique ID: %s" % uid]
    return text


def main():
    args = parse_args()
    if args.clear_node_id:
        if args.set_node_id not in (None, 0):
            sys.exit("--clear-node-id and --set-node-id %d contradict each other" % args.set_node_id)
        args.set_node_id = 0
    if args.set_node_id is not None and not 0 <= args.set_node_id <= 127:
        sys.exit("--set-node-id must be 1 to 127 (or 0 to clear); DroneCAN does not allow other node IDs")
    if args.clear_settings and args.set_node_id not in (None, 0):
        sys.exit("--clear-settings erases the node ID, so it cannot be combined with --set-node-id %d" % args.set_node_id)
    probe = Probe(args)
    failures = []

    def step(text):
        print("-", text, flush=True)

    step("reading the board")
    before = probe.read_state()
    if before is None:
        sys.exit("FAIL: could not read the board over SWD (is the ST-LINK connected and the board powered?)")
    for line in describe(before):
        print("   " + line)

    commands = ["catch {halt}"]
    if not args.check_only:
        if not args.elf or not os.path.exists(args.elf):
            sys.exit("FAIL: no firmware found; build it or pass --elf")
        step("programming %s" % os.path.relpath(args.elf))
        commands.append("program %s verify" % args.elf.replace("\\", "/"))
    id_file = None
    if args.clear_settings:
        step("erasing the stored node ID, pulse width limits and motor timeout")
        commands.append("flash erase_sector 0 %d %d" % (PWM_PAGE[0], PWM_PAGE[0]))
        commands.append("flash erase_sector 0 %d %d" % (NODE_ID_PAGE[0], NODE_ID_PAGE[0]))
    elif args.set_node_id is not None:
        if args.set_node_id == 0:
            step("removing the stored node ID")
            commands.append("flash erase_sector 0 %d %d" % (NODE_ID_PAGE[0], NODE_ID_PAGE[0]))
        else:
            step("storing node ID %d" % args.set_node_id)
            id_file = tempfile.NamedTemporaryFile(suffix=".bin", delete=False)
            id_file.write(bytes([args.set_node_id]) + bytes(7))   # one doubleword, like the firmware writes it
            id_file.close()
            commands.append("flash write_image erase %s 0x%08X bin" % (id_file.name.replace("\\", "/"), NODE_ID_PAGE[1]))
    if len(commands) > 1:
        commands.append("reset run")
        out = probe.run(commands, timeout=300)
        if id_file:
            os.unlink(id_file.name)
        if out is None or (not args.check_only and "Verified OK" not in out):
            print((out or "OpenOCD timed out")[-1500:])
            sys.exit("FAIL: programming or verification failed")
        if not args.check_only:
            print("   programmed and verified")
        step("waiting %.0f s for the board to start" % args.boot_wait)
        time.sleep(args.boot_wait)
    else:
        time.sleep(0.2)

    step("reading the board again")
    after = probe.read_state()
    if after is None:
        sys.exit("FAIL: could not read the board after programming")
    for line in describe(after):
        print("   " + line)

    if args.clear_settings:
        if stored_byte(after["ID"]) is not None or stored_settings(after):
            failures.append("settings are still stored after --clear-settings")
    else:
        if args.set_node_id is not None:
            wanted = None if args.set_node_id == 0 else args.set_node_id
            if stored_byte(after["ID"]) != wanted:
                failures.append("the stored node ID is %s, not the requested %s" % (stored_byte(after["ID"]), wanted))
        elif stored_byte(before["ID"]) != stored_byte(after["ID"]):
            failures.append("the stored node ID changed unexpectedly")
        if pwm_values(before["PWM"]) != pwm_values(after["PWM"]):
            failures.append("the stored pulse width settings changed unexpectedly")
        if stored_byte(after["ID"]) is not None:
            print("   note: a node ID is stored in flash and overrides the default (use --clear-node-id to remove it)")

    node, why = expected_node_id(after)
    print("   the board should be node %d (%s)" % (node, why))

    if args.can_device.lower() == "none":
        step("CAN check skipped")
    elif not os.path.exists(args.can_device):
        step("CAN check skipped: %s not found" % args.can_device)
    elif subprocess.run([sys.executable, "-c", "import serial"], capture_output=True).returncode != 0:
        step("CAN check skipped: pyserial is not installed for %s (pip install pyserial)" % sys.executable)
    else:
        step("asking node %d for GetNodeInfo over CAN" % node)
        text = ""
        for attempt in range(4):   # the USB-CAN adapter drops some frames, so ask again before giving up
            r = subprocess.run([sys.executable, os.path.join(HERE, "can_getnodeinfo.py"), args.can_device, str(node)],
                               capture_output=True, text=True, timeout=30)
            text = (r.stdout + r.stderr).strip()
            if r.returncode == 0 and "CRC OK" in text:
                break
        for line in text.splitlines():
            print("   " + line)
        if "CRC OK" not in text:
            failures.append("no valid GetNodeInfo reply from node %d" % node)
        elif "health OK" not in text:
            failures.append("the node reports a fault (see the health above; ERROR with vendor code 1 means the "
                            "PCA9685 is not answering)")

    print()
    if failures:
        print("FAIL: " + "; ".join(failures))
        sys.exit(1)
    print("PASS")


if __name__ == "__main__":
    main()
