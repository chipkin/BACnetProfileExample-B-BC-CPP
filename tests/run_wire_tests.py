#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
# Public-domain example code (CC0) - see ../LICENSE.
"""Run every B-BC Wire Test, one after another, and report one result.

    python3 tests/run_wire_tests.py [--exe build/BACnetExampleBBC] [--port 47864]

Two kinds of script:

  discovery_test.py               each starts the building controller itself (wiretest harness,
  controller_test.py              Python 3 standard library only). The last two play the Peer
  failure_mode_test.py            device 389002 on an ephemeral UDP port (bc_setup.HumidityPeer).
  sched_e_b_remote_peer.py        written against bacpypes3 (pip install -r tests/requirements.txt);
  calendar_reference_check.py     each expects a running device. This runner starts a fresh one
                                  for each, on --port, and passes it as --target-port. They pass
                                  when they print "RESULT: PASS".

Exit code 0 = every check in every script passed.
"""
import argparse
import subprocess
import sys
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPOSITORY = TESTS.parent
sys.path.insert(0, str(REPOSITORY))

import wiretest  # noqa: E402  (the vendored harness at the repository root)

SCRIPTS = ["discovery_test.py", "controller_test.py", "failure_mode_test.py"]
# bacpypes3 scripts, run against a device this runner starts. Each plays its Peer on its own
# fixed UDP port (sched_e_b_remote_peer.py: 47902 as device 389002; calendar_reference_check.py:
# 47903), so --port must not be one of those.
DEVICE_SCRIPTS = ["sched_e_b_remote_peer.py", "calendar_reference_check.py"]
SCRIPT_TIMEOUT_SECONDS = 120


def run_device_script(script, exe, port):
    """Start the building controller, run a bacpypes3 script against it, stop it. Returns
    (returncode, output, summary line)."""
    device = wiretest.start(exe, ["--port", port], "building controller")
    try:
        run = subprocess.run([sys.executable, "-B", str(TESTS / script), "--target-port", str(port)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                             timeout=SCRIPT_TIMEOUT_SECONDS)
    finally:
        device.stop()
    results = [line for line in run.stdout.splitlines() if line.startswith("RESULT:")]
    passed = run.returncode == 0 and results == ["RESULT: PASS"]
    return (0 if passed else 1), run.stdout, (results[-1] if results else "no RESULT line")


def main():
    arguments = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    arguments.add_argument("--exe", default=str(wiretest.default_exe(REPOSITORY, "BACnetExampleBBC")),
                           help="path to BACnetExampleBBC (default: build/BACnetExampleBBC)")
    arguments.add_argument("--port", type=int, default=47864, help="the building controller's UDP port (default 47864)")
    args = arguments.parse_args()

    outcomes = []
    for script in SCRIPTS:
        print(f"\n===== {script} =====", flush=True)
        run = subprocess.run([sys.executable, "-B", str(TESTS / script), "--exe", args.exe, "--port", str(args.port)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        print(run.stdout, end="", flush=True)
        totals = [line for line in run.stdout.splitlines() if "checks passed" in line]
        outcomes.append((script, run.returncode, totals[-1] if totals else "no summary"))

    for script in DEVICE_SCRIPTS:
        print(f"\n===== {script} =====", flush=True)
        returncode, output, result = run_device_script(script, args.exe, args.port)
        print(output, end="", flush=True)
        outcomes.append((script, returncode, result))

    print("\n===== summary =====")
    for script, returncode, totals in outcomes:
        print(f"{'PASS' if returncode == 0 else 'FAIL'}  {script:30} {totals}")
    failed = [script for script, returncode, _ in outcomes if returncode != 0]
    print("ALL WIRE TESTS PASSED" if not failed else f"FAILED: {', '.join(failed)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
