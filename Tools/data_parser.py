#!/usr/bin/env python3
"""
SEMS Data Log Parser
Usage: python data_parser.py <sems_log.csv>
Output: Statistics and optional plots
"""
import csv
import sys
import os
from datetime import datetime

def parse_log(filepath):
    records = []
    with open(filepath, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row["timestamp"].startswith("#"):
                continue  # skip event lines
            records.append(row)
    return records

def print_stats(records):
    if not records:
        print("No records found.")
        return

    temps = [float(r["temperature"]) for r in records if r["temperature"]]
    humids = [float(r["humidity"]) for r in records if r["humidity"]]
    pressures = [float(r["pressure"]) for r in records if r["pressure"]]
    aqis = [float(r["aqi"]) for r in records if r["aqi"]]

    print(f"Records: {len(records)}")
    print(f"Time range: {records[0]['timestamp']} ~ {records[-1]['timestamp']}")
    print(f"")
    print(f"Temperature:  min={min(temps):.1f}C  max={max(temps):.1f}C  avg={sum(temps)/len(temps):.1f}C")
    print(f"Humidity:     min={min(humids):.1f}%  max={max(humids):.1f}%  avg={sum(humids)/len(humids):.1f}%")
    print(f"Pressure:     min={min(pressures):.1f}hPa  max={max(pressures):.1f}hPa")
    print(f"AQI:          min={min(aqis):.0f}ppm  max={max(aqis):.0f}ppm")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python data_parser.py <sems_log.csv>")
        sys.exit(1)

    records = parse_log(sys.argv[1])
    print_stats(records)
