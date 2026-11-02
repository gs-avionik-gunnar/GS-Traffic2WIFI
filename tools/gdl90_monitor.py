#!/usr/bin/env python3
"""
GDL90 monitor for Windows / Linux / macOS (Python 3.6+, no extra packages).

Listens for GDL90 UDP packets (default port 4000), checks every frame (framing,
byte stuffing, CRC) and prints one status line per second plus warnings.
Use it on a laptop that is connected to the WiFi of the GS-Traffic2WIFI module
(close SkyDemon / other apps on that laptop first, they may occupy the port).

    python gdl90_monitor.py              # port 4000
    python gdl90_monitor.py --port 4000 --verbose   # also print every decoded message
"""
import argparse
import socket
import struct
import sys
import time

NAMES = {0x00: "Heartbeat", 0x0A: "Ownship", 0x0B: "OwnshipGeoAlt", 0x14: "Traffic",
         0x65: "ForeFlightID", 0xCC: "StratuxHeartbeat", 0x07: "UplinkData", 0x09: "HAT"}


def crc16(data):
    """CRC-16 as defined by the GDL90 ICD: crc = Table[crc >> 8] ^ (crc << 8) ^ byte.
    Not CRC-16/XMODEM (ICD example heartbeat 00 81 41 DB D0 08 02 -> 0x8BB3)."""
    crc = 0
    for b in data:
        t = crc & 0xFF00
        for _ in range(8):
            t = ((t << 1) ^ 0x1021) & 0xFFFF if t & 0x8000 else (t << 1) & 0xFFFF
        crc = (t ^ ((crc << 8) & 0xFFFF) ^ b) & 0xFFFF
    return crc


def unstuff(raw):
    out = bytearray()
    i = 0
    while i < len(raw):
        if raw[i] == 0x7D:
            i += 1
            if i >= len(raw):
                return None
            out.append(raw[i] ^ 0x20)
        else:
            out.append(raw[i])
        i += 1
    return bytes(out)


def split_frames(pkt):
    """Returns (frames, problems). A frame is the unstuffed content between two 0x7E flags."""
    frames, problems = [], []
    parts = pkt.split(b"\x7e")
    # b"" at both ends of a well formed packet "7E ... 7E"
    inner = parts[1:-1] if len(parts) >= 3 else []
    if pkt[:1] != b"\x7e" or pkt[-1:] != b"\x7e":
        problems.append("packet does not start/end with 0x7E")
    if len(inner) > 1:
        # several frames in one packet are legal only if each is complete; show it
        nonempty = [p for p in inner if p]
        if len(nonempty) > 1:
            problems.append("packet contains %d pieces between 0x7E flags (a 0x7E inside a frame is not escaped?)" % len(nonempty))
    for p in inner:
        if not p:
            continue
        u = unstuff(p)
        if u is None or len(u) < 3:
            problems.append("piece too short (%d bytes)" % len(p))
            continue
        body, crc = u[:-2], u[-2] | (u[-1] << 8)
        if crc16(body) != crc:
            problems.append("CRC error in piece starting %s" % body[:3].hex())
            continue
        frames.append(body)
    return frames, problems


def lat_lon(b):
    def conv(x):
        v = int.from_bytes(x, "big")
        if v & 0x800000:
            v -= 1 << 24
        return v * 180.0 / (1 << 23)
    return conv(b[0:3]), conv(b[3:6])


def decode_report(b):
    """Ownship (0x0A) / Traffic (0x14) report, b = message incl. id byte."""
    if len(b) < 28:
        return None
    lat, lon = lat_lon(b[5:11])
    alt12 = (b[11] << 4) | (b[12] >> 4)
    misc = b[12] & 0x0F
    hv = (b[14] << 4) | (b[15] >> 4)
    vv = ((b[15] & 0x0F) << 8) | b[16]
    return {
        "addr": b[2:5].hex().upper(), "atype": b[1] & 0x0F, "alert": b[1] >> 4,
        "lat": lat, "lon": lon,
        "alt": None if alt12 == 0xFFF else alt12 * 25 - 1000,
        "misc": misc, "airborne": bool(misc & 0x08),
        "nic": b[13] >> 4, "nacp": b[13] & 0x0F,
        "gs": None if hv == 0xFFF else hv,
        "vv": None if vv == 0x800 else vv,
        "trk": b[17] * 360.0 / 256.0, "emitter": b[18],
        "call": bytes(b[19:27]).decode("ascii", "replace").strip("\x00 "),
    }


class Stats:
    def __init__(self):
        self.count = {}
        self.last_seen = {}
        self.hb = None
        self.own = None
        self.geo = None
        self.traffic = {}
        self.crc_err = 0
        self.packets = 0
        self.problem_counts = {}
        self.sources = {}
        self.stx = None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=4000)
    ap.add_argument("--verbose", action="store_true", help="print every decoded message")
    args = ap.parse_args()

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        s.bind(("", args.port))
    except OSError as e:
        print("Cannot bind UDP port %d: %s\nClose other GDL90 apps on this computer." % (args.port, e))
        return 1
    s.settimeout(0.2)
    print("Listening for GDL90 on UDP port %d ... (Ctrl+C to stop)" % args.port)

    st = Stats()
    t_start = time.time()
    t_report = t_start + 1.0
    last_counts = {}
    try:
        while True:
            try:
                pkt, src = s.recvfrom(2048)
            except socket.timeout:
                pkt = None
            now = time.time()
            if pkt:
                st.packets += 1
                st.sources["%s:%d" % src] = st.sources.get("%s:%d" % src, 0) + 1
                frames, problems = split_frames(pkt)
                for p in problems:
                    st.problem_counts[p] = st.problem_counts.get(p, 0) + 1
                    if p.startswith("CRC error"):
                        st.crc_err += 1
                for f in frames:
                    mid = f[0]
                    st.count[mid] = st.count.get(mid, 0) + 1
                    st.last_seen[mid] = now
                    if mid == 0x00 and len(f) >= 7:
                        ts = f[3] | (f[4] << 8) | ((f[2] >> 7) << 16)
                        st.hb = {"gps": bool(f[1] & 0x80), "utc": bool(f[2] & 0x01), "ts": ts,
                                 "st1": f[1], "st2": f[2], "cnt": f[5:7].hex()}
                    elif mid == 0x0A:
                        st.own = decode_report(f)
                    elif mid == 0x0B and len(f) >= 5:
                        alt = int.from_bytes(f[1:3], "big", signed=True) * 5
                        st.geo = {"alt": alt, "vfom": ((f[3] & 0x7F) << 8) | f[4], "warn": bool(f[3] & 0x80)}
                    elif mid == 0x14:
                        r = decode_report(f)
                        if r:
                            st.traffic[r["addr"]] = (now, r)
                    elif mid == 0xCC and len(f) >= 2:
                        st.stx = f[1]
                    if args.verbose:
                        print("  %-16s %s" % (NAMES.get(mid, "0x%02X" % mid), f.hex()))
            if now >= t_report:
                t_report += 1.0
                line = report(st, now, last_counts)
                print(line)
                last_counts = dict(st.count)
                for p, n in list(st.problem_counts.items()):
                    print("  !! %s  (x%d so far)" % (p, n))
                st.problem_counts = {}
    except KeyboardInterrupt:
        print("\nStopped.")
        summary(st)
    return 0


def rate(st, last, mid):
    return st.count.get(mid, 0) - last.get(mid, 0)


def report(st, now, last):
    if st.packets == 0:
        return "[%s] no packets yet" % time.strftime("%H:%M:%S")
    parts = ["[%s]" % time.strftime("%H:%M:%S")]
    parts.append("pkts=%d" % st.packets)
    parts.append("hb/s=%d own/s=%d geo/s=%d trf/s=%d" % (rate(st, last, 0), rate(st, last, 0x0A), rate(st, last, 0x0B), rate(st, last, 0x14)))
    if st.hb:
        h = st.hb
        parts.append("HB: gps=%s utc=%s t=%02d:%02d:%02d" % ("YES" if h["gps"] else "NO", "yes" if h["utc"] else "no",
                                                         h["ts"] // 3600, h["ts"] // 60 % 60, h["ts"] % 60))
    else:
        parts.append("HB: NONE")
    if st.own:
        o = st.own
        parts.append("OWN: %.5f %.5f alt=%s gs=%s trk=%.0f NIC/NACp=%d/%d" % (o["lat"], o["lon"], o["alt"], o["gs"], o["trk"], o["nic"], o["nacp"]))
        if o["lat"] == 0 and o["lon"] == 0:
            parts.append("<-- position is 0/0 (no fix)")
    else:
        parts.append("OWN: NONE")
    parts.append("GEO=%s" % (st.geo["alt"] if st.geo else "none"))
    if st.stx is not None:
        parts.append("0xCC=%02X" % st.stx)
    if st.crc_err:
        parts.append("CRCerr=%d" % st.crc_err)
    return " ".join(parts)


def summary(st):
    print("\nSummary: %d packets from %s" % (st.packets, ", ".join(st.sources) or "-"))
    for mid, n in sorted(st.count.items()):
        print("  %-18s %d" % (NAMES.get(mid, "0x%02X" % mid), n))
    if 0x00 not in st.count:
        print("  -> no valid heartbeat received")
    if 0x0A not in st.count:
        print("  -> no valid ownship report received")


if __name__ == "__main__":
    sys.exit(main())
