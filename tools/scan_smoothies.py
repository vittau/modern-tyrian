#!/usr/bin/env python3
"""Scan Tyrian level data (tyrianN.lvl) for "smoothie" events.

The regression harness cannot use the recorded demos to reach the lava / water /
iced-blur / blur filters or the two JE_starShowVGA() special codes, because
`smoothies[]` stays zero for the whole of every demo.  Those effects are enabled
by level event type 64 ("smoothies[eventdat-1] = eventdat2", src/tyrian2.c), so
this tool parses the level files directly and reports which levels touch which
bit and at what level position (event time, which is curLoc when the event
fires).

Usage:
    tools/scan_smoothies.py /path/to/Tyrian

No dependencies beyond the Python standard library.  This is an analysis helper;
it is not part of the build or of `make regress`.

Caveat: this is a *static* scan.  It does not follow the level script's control
flow, so it lists smoothie events that an unconditional jump (event type 54) or
a conditional jump (type 61/66/70/71) may skip at runtime.  The authoritative
answer for which paths a scenario actually reaches comes from running it with a
temporary probe in the filter functions (as done for Phase 0b).  In particular,
episode 4 levels jump around a lot: several bit-6 spotlights and bit-1 lavas the
scan reports are never executed.
"""

import os
import struct
import sys

# Event type that writes smoothies[eventdat-1] = eventdat2 / smoothie_data.
EVENT_SMOOTHIE = 64

SMOOTHIE_NAMES = {
    1: "lava_filter           (processorType > 2)",
    2: "water_filter          (processorType > 2)",
    3: "iced_blur_filter #3   (processorType > 1)",
    4: "blur_filter           (processorType > 1)",
    5: "iced_blur_filter #5*  (processorType > 1)",
    6: "starShowVGA code 2    (player spotlight)",
    7: "(unused by renderer)",
    8: "(unused by renderer)",
    9: "starShowVGA code 1    (vertical flip)",
}


def read_u16(buf, off):
    return struct.unpack_from("<H", buf, off)[0]


def read_s16(buf, off):
    return struct.unpack_from("<h", buf, off)[0]


def read_u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def parse_level(buf, base):
    """Return a list of events from the level whose byte 0 is at `base`."""
    off = base
    off += 1  # char_mapFile
    off += 1  # char_shapeFile
    off += 2  # mapX
    off += 2  # mapX2
    off += 2  # mapX3
    enemy_max = read_u16(buf, off)
    off += 2
    off += 2 * enemy_max
    event_max = read_u16(buf, off)
    off += 2

    events = []
    for i in range(event_max):
        eventtime = read_u16(buf, off)
        eventtype = buf[off + 2]
        eventdat = read_s16(buf, off + 3)
        eventdat2 = read_s16(buf, off + 5)
        eventdat3 = struct.unpack_from("<b", buf, off + 7)[0]
        # eventdat5 (s8), eventdat6 (s8), eventdat4 (u8) follow; unused here.
        events.append((eventtime, eventtype, eventdat, eventdat2, eventdat3))
        off += 11

    return events


def main():
    if len(sys.argv) != 2:
        sys.stderr.write("usage: %s DATA_DIR\n" % sys.argv[0])
        return 1

    data_dir = sys.argv[1]
    if not os.path.isfile(os.path.join(data_dir, "tyrian1.lvl")):
        sys.stderr.write("no tyrian1.lvl in %s\n" % data_dir)
        return 1

    enabled_bits = {}
    barely = {}

    for episode in range(1, 5):
        path = os.path.join(data_dir, "tyrian%d.lvl" % episode)
        if not os.path.isfile(path):
            continue
        with open(path, "rb") as f:
            buf = f.read()

        lvl_num = read_u16(buf, 0)
        offsets = [read_u32(buf, 2 + 4 * i) for i in range(lvl_num)]

        # lvlPos holds two entries per level; levels use the even ones, and the
        # last entry is the episode-4 item-data block (see episodes.c).
        for level_index in range(1, (lvl_num - 1) // 2 + 1):
            base = offsets[(level_index - 1) * 2]
            events = parse_level(buf, base)
            for eventtime, eventtype, eventdat, eventdat2, eventdat3 in events:
                if eventtype != EVENT_SMOOTHIE:
                    continue
                if not (1 <= eventdat <= 9):
                    continue
                bit = eventdat
                # smoothie_data index: bit 5 is stored at index 3 (see JE_eventSystem).
                data_index = 3 if bit == 5 else bit
                action = "ON " if eventdat2 else "off"
                print("episode %d level %2d  bit %d  %s  eventtime(curLoc)=%-6d "
                      "data[%d]=%d  %s"
                      % (episode, level_index, bit, action, eventtime,
                         data_index, eventdat3, SMOOTHIE_NAMES.get(bit, "?")))
                if eventdat2:
                    enabled_bits.setdefault(bit, set()).add((episode, level_index))
                    if eventdat3:
                        barely.setdefault(bit, []).append(
                            (episode, level_index, eventtime, eventdat3))

    print()
    print("=== bits that get enabled at least once ===")
    for bit in range(1, 10):
        if bit in enabled_bits:
            levels = ", ".join("E%d/L%d" % (e, l) for e, l in sorted(enabled_bits[bit]))
            print("bit %d: %s" % (bit, levels))
    print()
    print("=== bits enabled with a non-zero smoothie_data ===")
    for bit in sorted(barely):
        for (e, l, t, d) in barely[bit]:
            print("bit %d E%d/L%d curLoc=%d data=%d" % (bit, e, l, t, d))
    return 0


if __name__ == "__main__":
    sys.exit(main())
