#!/usr/bin/env python
"""Ad hoc check of Calendar 1 "Cream" (stack-held) driving Schedule 1 "Saffron"'s exception.

Reads Cream's Date_List and Present_Value, writes a Date_List that adds TODAY (the device's
Local_Date), then confirms Cream's Present_Value goes TRUE, Saffron's calendar-reference exception
takes over (Present_Value = the exception value), and Analog Output 1 "Chartreuse" follows. Finally
it writes the original Date_List back.

Usage (B-BC running on --port 47900 on this host):
    python tests/calendar_reference_check.py --target-port 47900
Prints RESULT: PASS on success.
"""
import asyncio
import datetime

from bacpypes3.app import Application
from bacpypes3.argparse import SimpleArgumentParser
from bacpypes3.basetypes import CalendarEntry
from bacpypes3.constructeddata import ListOf
from bacpypes3.pdu import Address
from bacpypes3.primitivedata import Date, ObjectIdentifier

EXCEPTION_VALUE = 5.0  # SCHEDULE_EXCEPTION_VALUE in main.cpp


async def main():
    p = SimpleArgumentParser()
    p.set_defaults(address="127.0.0.1/32:47903")
    p.add_argument("--target-port", type=int, default=47900)
    a = p.parse_args()
    app = Application.from_args(a)
    dev = Address(f"127.0.0.1:{a.target_port}")
    cream = ObjectIdentifier("calendar,1")
    saffron = ObjectIdentifier("schedule,1")
    chartreuse = ObjectIdentifier("analog-output,1")

    async def rd(obj, prop):
        return await app.read_property(dev, obj, prop)

    try:
        original = await rd(cream, "date-list")
        print("Cream Date_List before:", [str(e.date) for e in original])
        print("Cream Present_Value before:", await rd(cream, "present-value"))
        print("Saffron Present_Value before:", await rd(saffron, "present-value"))

        today = datetime.date.today()  # the device serves Local_Date from local wall-clock time
        entries = list(original) + [
            CalendarEntry(date=Date((today.year - 1900, today.month, today.day, today.isoweekday())))
        ]
        await app.write_property(dev, cream, "date-list", ListOf(CalendarEntry)(entries))
        print("wrote Cream Date_List + today", today)

        await asyncio.sleep(1.5)  # Present_Value is re-derived every tick; the Schedule re-evaluates
        cream_pv = await rd(cream, "present-value")
        saffron_pv = await rd(saffron, "present-value")
        chartreuse_pv = await rd(chartreuse, "present-value")
        print("Cream Date_List after:", [str(e.date) for e in await rd(cream, "date-list")])
        print("Cream Present_Value after:", cream_pv)
        print("Saffron Present_Value after:", saffron_pv)
        print("Chartreuse Present_Value after:", chartreuse_pv)

        ok = bool(cream_pv) and abs(float(chartreuse_pv) - EXCEPTION_VALUE) < 0.01
        await app.write_property(dev, cream, "date-list", ListOf(CalendarEntry)(list(original)))
        await asyncio.sleep(1.5)
        print("restored Date_List; Cream Present_Value:", await rd(cream, "present-value"),
              "Chartreuse:", await rd(chartreuse, "present-value"))
        print("RESULT:", "PASS" if ok else "FAIL")
    finally:
        app.close()


if __name__ == "__main__":
    asyncio.run(main())
