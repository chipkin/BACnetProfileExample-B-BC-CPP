"""Issue #6: prove Schedule 1's REMOTE reference (device 389002, AO 1) is written on the wire.

Acts as peer device 389002 with a commandable Analog Output 1 on its own UDP port, sends a
unicast I-Am to the B-BC example so its Device Address Binding can resolve 389002 without a
BBMD, then changes Schedule 1's Schedule_Default so the schedule re-evaluates and fans out.

Usage (B-BC running on --port 47900 on this host):
    python tests/sched_e_b_remote_peer.py --target-port 47900
Prints RESULT: PASS when the peer's AO 1 Present_Value is the value Schedule 1 takes now at
priority 8: --value, or the weekly / exception value while one of those is in force (on a
Monday after 08:00 or on December 25 a new Schedule_Default changes nothing, so the check is
then that the scheduled value reached the peer).
"""
import asyncio
from bacpypes3.argparse import SimpleArgumentParser
from bacpypes3.app import Application
from bacpypes3.local.analog import AnalogOutputObject
from bacpypes3.pdu import Address
from bacpypes3.primitivedata import ObjectIdentifier, Real

import bc_setup

async def main():
    p = SimpleArgumentParser()
    p.set_defaults(address="127.0.0.1/32:47902", instance=389002, name="sched-peer-389002")
    p.add_argument("--target-port", type=int, default=47900)
    p.add_argument("--value", type=float, default=42.5)
    a = p.parse_args()
    app = Application.from_args(a)
    ao = AnalogOutputObject(
        objectIdentifier=("analog-output", 1),
        objectName="peer AO 1",
        presentValue=0.0,
        units="degrees-celsius",
        relinquishDefault=0.0,
    )
    app.add_object(ao)
    bbc = Address(f"127.0.0.1:{a.target_port}")
    try:
        for _ in range(3):
            app.i_am(address=bbc)
            await asyncio.sleep(0.5)
        sched = ObjectIdentifier("schedule,1")
        print("Schedule PV before:", await app.read_property(bbc, sched, "present-value"))
        await app.write_property(bbc, sched, "schedule-default", Real(a.value))
        print("wrote Schedule_Default =", a.value)
        expected = bc_setup.scheduled_value(a.value)
        if expected != a.value:
            print("a weekly or exception value is in force: expecting", expected)
        for _ in range(20):
            await asyncio.sleep(0.25)
            if ao.presentValue == expected:
                break
        print("Schedule PV after:", await app.read_property(bbc, sched, "present-value"))
        print("peer AO1 Present_Value:", ao.presentValue)
        print("peer AO1 Priority_Array:",
              [(i + 1, pv.real if pv.real is not None else "null")
               for i, pv in enumerate(ao.priorityArray) if pv.null is None])
        print("RESULT:", "PASS" if ao.presentValue == expected
              and ao.priorityArray[bc_setup.SCHEDULE_WRITE_PRIORITY - 1].real == expected else "FAIL")
    finally:
        app.close()


if __name__ == "__main__":
    asyncio.run(main())
