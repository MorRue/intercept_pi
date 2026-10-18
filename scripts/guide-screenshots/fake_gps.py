#!/usr/bin/env python3
"""Feeds OpenCPN a fixed GPS fix: one NMEA $GPRMC sentence per second over UDP.

Usage: fake_gps.py LAT LON SOG_KT COG_DEG [PORT]
"""
import datetime
import socket
import sys
import time


def nmea(body):
    checksum = 0
    for ch in body:
        checksum ^= ord(ch)
    return "$%s*%02X\r\n" % (body, checksum)


def ddmm(value, width, pos, neg):
    a = abs(value)
    degrees = int(a)
    minutes = (a - degrees) * 60
    return "%0*d%07.4f" % (width, degrees, minutes), pos if value >= 0 else neg


def main():
    lat, lon, sog, cog = (float(v) for v in sys.argv[1:5])
    port = int(sys.argv[5]) if len(sys.argv) > 5 else 10110
    out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    la, ns = ddmm(lat, 2, "N", "S")
    lo, ew = ddmm(lon, 3, "E", "W")
    while True:
        now = datetime.datetime.now(datetime.timezone.utc)
        body = "GPRMC,%s,A,%s,%s,%s,%s,%.1f,%.1f,%s,,," % (
            now.strftime("%H%M%S.00"), la, ns, lo, ew, sog, cog,
            now.strftime("%d%m%y"))
        out.sendto(nmea(body).encode(), ("127.0.0.1", port))
        time.sleep(1)


if __name__ == "__main__":
    main()
