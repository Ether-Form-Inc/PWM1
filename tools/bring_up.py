#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ether Form Inc.
"""One command to bring up a new can_to_pwm board: wipe it, program it, calibrate its PWM timing and test it.

Connect an ST-LINK to J3, power the board (5 V on J1), and connect a serial USB-CAN adapter to the CAN bus (with the bus
terminated). Then run, from anywhere:

    tools/bring_up.py                     # the newest build in build*/stm_generated
    tools/bring_up.py --elf path/to/can_pwm_firmware.elf

What it does, in order:
  1. checks the ST-LINK sees a powered STM32G0B1 and reads its unique ID,
  2. mass-erases all of the chip's flash (this clears every setting) and programs and verifies the firmware,
  3. checks the firmware started and carries the version that was programmed,
  4. CAN transceiver self-test, over SWD (needs no bus): the controller drives its TX line and the transceiver must
     answer on RX. Catches a Rev A board (CAN RX and TX swapped), no 5 V supply, or a dead transceiver,
  5. asks the node over CAN for its GetNodeInfo and checks it is the chip just programmed, healthy, and the right version,
  6. calibrates PWM_PRESCALE: times output 0 with the node's own timer and sets the prescale that gives a 20 ms period,
  7. times all four outputs and checks their period and idle pulse width,
  8. checks only the calibration is stored (node ID, pulse limits and the rest stay at their defaults),
  9. appends a line to the CSV log (bring_up_log.csv in the repository folder) and prints PASS or FAIL.
The exit code is 0 only for PASS.

Accuracy: the node's timer runs from the MCU's internal 16 MHz oscillator (about +-1 %). The PCA9685's prescale
changes the period in steps of about 0.8 %, so the calibrated period is within about 1 % of 20 ms.
"""
import argparse
import csv
import datetime
import os
import re
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import flash_and_check as fc   # noqa: E402  (shares the OpenOCD helpers and the settings layout)

TARGET_PERIOD_US = 20000     # 50 Hz
DEFAULT_PRESCALE = 121       # board/user_defined_threshold.h
IDLE_PULSE_US = 1000         # the pulse PWM_MEASURE drives (Pwm::arming_pulse_us)
LOG_FIELDS = ["timestamp", "result", "chip_uid", "firmware", "hardware", "pwm_prescale", "period_default_us",
              "high_default_us", "period_calibrated_us", "period_error_pct", "out0_period_us", "out1_period_us",
              "out2_period_us", "out3_period_us", "out0_high_us", "out1_high_us", "out2_high_us", "out3_high_us",
              "j5_mode", "j5_offset", "can_selftest", "failures"]


def prescale_for(period_us, current=DEFAULT_PRESCALE, target=TARGET_PERIOD_US):
    """The PRE_SCALE register value that gives a `target` period when `current` gave `period_us`.

    The period is 4096 * (PRE_SCALE + 1) / oscillator, so it is proportional to (PRE_SCALE + 1)."""
    if period_us <= 0:
        raise ValueError("no period measured")
    value = round((current + 1) * target / period_us) - 1
    return max(3, min(255, value))


def expected_high_us(period_us, pulse_us=IDLE_PULSE_US):
    """The high time to expect for the idle pulse: the PCA9685 counts 4096 per period."""
    count = pulse_us * 4096 // TARGET_PERIOD_US
    return count * period_us / 4096.0


def parse_args():
    default_exe, default_scripts = fc.find_openocd()
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", default=fc.newest_elf(), help="firmware to program (default: the newest build)")
    ap.add_argument("--can-device", default="/dev/ttyUSB0", help="serial USB-CAN adapter")
    ap.add_argument("--log", default=os.path.join(REPO, "bring_up_log.csv"), help="CSV file to append the result to")
    ap.add_argument("--tolerance", type=float, default=1.0, help="allowed error of the calibrated period, in %%")
    ap.add_argument("--keep-flash", action="store_true",
                    help="program without the mass erase (keeps whatever settings are on the board)")
    ap.add_argument("--boot-wait", type=float, default=3.0, help="seconds to wait after each reset")
    ap.add_argument("--openocd", default=os.environ.get("OPENOCD", default_exe))
    ap.add_argument("--scripts", default=os.environ.get("OPENOCD_SCRIPTS", default_scripts))
    ap.add_argument("--interface", default="stlink-dap.cfg")
    ap.add_argument("--stock", action="store_true")
    ap.add_argument("--probe-serial")
    return ap.parse_args()


class Report:
    def __init__(self):
        self.failures = []
        self.row = {k: "" for k in LOG_FIELDS}

    def step(self, text):
        print("-", text, flush=True)

    def info(self, text):
        print("   " + text, flush=True)

    def fail(self, text):
        self.failures.append(text)
        print("   FAIL: " + text, flush=True)


def target_voltage(output):
    m = re.search(r"Target voltage: ([0-9.]+)", output or "")
    return float(m.group(1)) if m else None


def can_selftest(probe, boot_wait):
    """The CAN controller drives its TX line dominant and the transceiver must answer on RX (needs no bus). Returns
    (ok, detail). Resets the board afterwards.

    A CAN transceiver releases the bus if TXD stays dominant for a few milliseconds (its time-out), so RX is read
    immediately after each dominant pulse and TX is returned to recessive for several milliseconds in between."""
    commands = ["catch {halt}",
                "mww 0x40006418 0x00000001", "mww 0x40006418 0x00000003", "mww 0x40006418 0x00000083",
                "proc rxl {} {return [expr {([lindex [read_memory 0x50000010 32 1] 0] >> 11) & 1}]}",
                "proc txtest {} { set low 0; set high 0; set n 12; "
                "for {set i 0} {$i < $n} {incr i} { "
                "mww 0x40006410 0x00000040; set d [rxl]; mww 0x40006410 0x00000060; sleep 6; set r [rxl]; "
                "if {$d == 0} {incr low}; if {$r == 1} {incr high} }; "
                'echo "TXTEST dominant_low $low recessive_high $high of $n" }',
                "txtest",
                "mww 0x40006410 0x00000000", "mww 0x40006418 0x00000003", "mww 0x40006418 0x00000001",
                "reset run"]
    out = probe.run(commands)
    time.sleep(boot_wait)
    if out is None:
        return False, "the ST-LINK timed out"
    m = re.search(r"TXTEST dominant_low (\d+) recessive_high (\d+) of (\d+)", out)
    if not m:
        return False, "could not run the test"
    low, high, n = (int(x) for x in m.groups())
    # RX must go low in response to TX dominant at least sometimes (the read has to land inside the transceiver's
    # time-out) and must always be recessive when TX is
    ok = low >= 2 and high == n
    return ok, "RX followed TX dominant %d of %d times and was recessive %d of %d times" % (low, n, high, n)


def main():
    args = parse_args()
    probe = fc.Probe(args)
    rep = Report()
    row = rep.row
    row["timestamp"] = datetime.datetime.now().isoformat(timespec="seconds")
    elf_info = None
    link = None

    def finish():
        row["failures"] = "; ".join(rep.failures)
        row["result"] = "FAIL" if rep.failures else "PASS"
        new_file = not os.path.exists(args.log) or os.path.getsize(args.log) == 0
        with open(args.log, "a", newline="") as f:
            w = csv.DictWriter(f, fieldnames=LOG_FIELDS)
            if new_file:
                w.writeheader()
            w.writerow(row)
        print()
        print("%s  (logged to %s)" % (row["result"], os.path.relpath(args.log)))
        if rep.failures:
            print("Problems: " + "; ".join(rep.failures))
        sys.exit(1 if rep.failures else 0)

    # ---- 1. the programmer and the chip
    rep.step("checking the ST-LINK and the board")
    if not args.elf or not os.path.exists(args.elf):
        sys.exit("FAIL: no firmware found; build it or pass --elf")
    with open(args.elf, "rb") as f:
        elf_info = fc.parse_fw_info(f.read())
    out = probe.run(["echo preflight"])
    volts = target_voltage(out)
    state = probe.read_state()
    if state is None:
        if volts is not None and volts < 1.0:
            rep.fail("the board is not powered (the ST-LINK sees %.2f V): power it and check the J3 cable" % volts)
        else:
            rep.fail("could not read the board over SWD")
        finish()
    uid = "".join("%08x" % w for w in reversed(state["UID"]))
    row["chip_uid"] = uid
    rep.info("chip unique ID %s, supply %s V" % (uid, "%.2f" % volts if volts else "?"))
    if volts is not None and not 3.0 <= volts <= 3.6:
        rep.fail("the 3.3 V supply reads %.2f V" % volts)
    rep.info("firmware to program: %s" % (elf_info["version"] if elf_info else "no version information"))
    row["firmware"] = elf_info["version"] if elf_info else ""

    # ---- 2. wipe and program
    rep.step("%s and programming" % ("programming (flash kept)" if args.keep_flash else "erasing all flash"))
    commands = ["catch {halt}"]
    if not args.keep_flash:
        commands += ["reset halt", "stm32l4x mass_erase 0"]
    commands += ["program %s verify" % args.elf.replace("\\", "/"),
                 # a blank STM32G0 keeps its "main flash empty" flag set, and then every reset boots ST's bootloader
                 "mmw 0x40022000 0x0 0x00010000", "reset run"]
    out = probe.run(commands, timeout=300)
    if out is None or "Verified OK" not in out or (not args.keep_flash and "mass erase complete" not in out):
        print((out or "the ST-LINK timed out")[-1200:])
        rep.fail("erasing, programming or verifying failed")
        finish()
    rep.info("erased, programmed and verified")
    time.sleep(args.boot_wait)

    # ---- 3. it started
    rep.step("checking the firmware started")
    state = probe.read_state()
    if state is None:
        rep.fail("could not read the board after programming")
        finish()
    fw = state.get("fw_info")
    if not fw or (elf_info and fw["version"] != elf_info["version"]):
        rep.fail("the board does not hold the firmware that was programmed")
    else:
        rep.info("firmware on the board: " + fw["version"])
    servo, offset = fc.jumpers(state)
    row["j5_mode"], row["j5_offset"] = ("servo" if servo else "motor"), offset
    rep.info("J5: %s mode, actuator ID offset %d" % (row["j5_mode"], offset))
    if fc.stored_byte(state["ID"]) is not None or fc.stored_settings(state):
        rep.fail("settings are stored on a board that was just wiped")

    # ---- 4. the CAN transceiver, without a bus
    rep.step("CAN transceiver self-test (over SWD)")
    ok, detail = can_selftest(probe, args.boot_wait)
    row["can_selftest"] = "pass" if ok else "fail"
    rep.info(detail)
    if not ok:
        rep.fail("CAN transceiver self-test failed: this is a Rev A board (CAN RX and TX swapped), the 5 V supply on "
                 "J1 is missing, or the transceiver is faulty")
        finish()

    # ---- 5. over CAN
    rep.step("opening the CAN link and reading GetNodeInfo")
    try:
        from can_link import CanLink, LinkError
        link = CanLink(args.can_device, 10)
        info = link.node_info()
    except Exception as e:   # no adapter, no bus, or no answer
        rep.fail("no answer over CAN (%s). Check the adapter, the termination and the CAN wiring" % e)
        finish()
    row["hardware"] = "%d.%d" % (info.hw_major, info.hw_minor)
    rep.info("node 10: %s, software %d.%d, commit %s, hardware %s, health %s" % (
        info.name, info.sw_major, info.sw_minor, "%08x" % info.commit if info.commit else "-", row["hardware"],
        ["OK", "WARNING", "ERROR", "CRITICAL"][info.health]))
    if info.health != 0:
        rep.fail("the node reports a fault (health %d, vendor status %d)" % (info.health, info.vendor_status))
    expected_uid = bytes(w for word in state["UID"] for w in word.to_bytes(4, "little")).hex()
    if not info.unique_id.startswith(expected_uid):
        rep.fail("the node on the CAN bus (%s) is not the chip that was just programmed (%s): another board is on "
                 "the bus" % (info.unique_id[:24], expected_uid))
    if fw and (info.sw_major, info.sw_minor) != (fw["major"], fw["minor"]):
        rep.fail("the node reports software %d.%d but the flash holds %s" % (info.sw_major, info.sw_minor, fw["version"]))
    if fw and info.commit is not None and info.commit != fw["commit"]:
        rep.fail("the node reports commit %08x but the flash holds %s" % (info.commit, fw["version"]))
    if rep.failures:
        finish()

    # ---- 6. calibration
    rep.step("calibrating the PWM period (target %.1f ms, tolerance %.1f %%)" % (TARGET_PERIOD_US / 1000.0, args.tolerance))
    try:
        period0, high0 = link.measure(0)
        row["period_default_us"], row["high_default_us"] = period0, high0
        rep.info("at the default prescale %d: period %d us (%.2f Hz), idle pulse %d us" % (
            DEFAULT_PRESCALE, period0, 1e6 / period0, high0))
        best = None
        guess = prescale_for(period0)
        for prescale in dict.fromkeys([guess, guess - 1, guess + 1]):   # the nearest step, then its neighbours
            if not 3 <= prescale <= 255:
                continue
            if link.set("PWM_PRESCALE", prescale).value != prescale:
                raise LinkError("the node did not accept PWM_PRESCALE %d" % prescale)
            time.sleep(0.2)
            period, high = link.measure(0)
            error = 100.0 * (period - TARGET_PERIOD_US) / TARGET_PERIOD_US
            rep.info("PWM_PRESCALE %d: period %d us (%.2f Hz), error %+.2f %%" % (prescale, period, 1e6 / period, error))
            if best is None or abs(error) < abs(best[2]):
                best = (prescale, period, error, high)
            if abs(error) <= args.tolerance:
                break
        prescale, period, error, high = best
        if link.set("PWM_PRESCALE", prescale).value != prescale:
            raise LinkError("could not set the final PWM_PRESCALE")
        row["pwm_prescale"], row["period_calibrated_us"], row["period_error_pct"] = prescale, period, "%.2f" % error
        if abs(error) > args.tolerance:
            rep.fail("the calibrated period is %.2f %% off 20 ms (tolerance %.1f %%)" % (error, args.tolerance))
    except LinkError as e:
        rep.fail("calibration failed: %s" % e)
        finish()

    # ---- 7. all four outputs
    rep.step("measuring all four outputs")
    final_period = period
    for out in range(4):
        try:
            p, h = link.measure(out)
        except LinkError as e:
            rep.fail("output %d: %s" % (out, e))
            continue
        row["out%d_period_us" % out], row["out%d_high_us" % out] = p, h
        want_high = expected_high_us(p)
        bad = []
        if abs(p - final_period) > 0.01 * final_period:
            bad.append("period differs from output 0")
        if abs(h - want_high) > 0.03 * want_high:
            bad.append("idle pulse %d us, expected about %.0f us" % (h, want_high))
        rep.info("output %d: period %d us, idle pulse %d us%s" % (out, p, h, "  <-- " + "; ".join(bad) if bad else ""))
        if bad:
            rep.fail("output %d: %s" % (out, "; ".join(bad)))

    # ---- 8. only the calibration is stored
    rep.step("checking what is stored in flash")
    final = probe.read_state()
    if final is None:
        rep.fail("could not read the board at the end")
    else:
        stored = fc.stored_settings(final)
        rep.info("stored node ID: %s, stored settings: %s" % (
            fc.stored_byte(final["ID"]) or "none (default 10)", stored or "none"))
        if fc.stored_byte(final["ID"]) is not None:
            rep.fail("a node ID is stored; it should be left at the default")
        if set(stored) - {"PWM_PRESCALE"}:
            rep.fail("settings other than PWM_PRESCALE are stored: %s" % ", ".join(sorted(set(stored) - {"PWM_PRESCALE"})))
        if stored.get("PWM_PRESCALE") not in (None, int(row["pwm_prescale"] or 0)):
            rep.fail("the stored PWM_PRESCALE differs from the calibrated one")
    link.close()
    finish()


if __name__ == "__main__":
    main()
