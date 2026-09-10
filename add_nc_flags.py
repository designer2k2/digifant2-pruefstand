"""
Stamp no-connect flags onto every pin ERC reports as unconnected.

circuit-synth 0.1.0 has no NC-flag API, so the ~28 genuinely-unused pins
(INA226 ALERT, AD9833 COMP/CAP_2V5, MCP4728 RDY, SK6812 DOUT, unused Pico
GPIOs / VBUS / VSYS / 3V3_EN / SWCLK / SWDIO) trip ERC every regen. This reads
the coordinates straight out of a KiCad ERC report and adds the flags.

Usage:
  1. open the project in KiCad, run ERC, save the report as <projdir>/ERC.rpt
  2. python3 add_nc_flags.py digifant2_pruefstand_cs_v19
  3. reload in KiCad, re-run ERC  -> the pin_not_connected errors are gone

Re-running is safe: existing no_connects at the same coordinate are skipped.
"""
import re
import sys
import uuid
from pathlib import Path

proj = Path(__file__).parent / sys.argv[1]
sch = proj / "Digifant2_Pruefstand.kicad_sch"
rpt = proj / "ERC.rpt"

coords = set()
for m in re.finditer(r"\[pin_not_connected\][^@]*@\(([\d.]+) mm, ([\d.]+) mm\)", rpt.read_text()):
    coords.add((float(m.group(1)), float(m.group(2))))

text = sch.read_text()
existing = {
    (round(float(x), 2), round(float(y), 2))
    for x, y in re.findall(r"\(no_connect\s*\(at ([\d.-]+) ([\d.-]+)\)", text)
}

blocks = []
for x, y in sorted(coords):
    if (round(x, 2), round(y, 2)) in existing:
        continue
    blocks.append(f"\t(no_connect (at {x} {y}) (uuid {uuid.uuid4()}))")

if not blocks:
    print("nothing to add")
    sys.exit(0)

# insert just before the first top-level (sheet_instances
idx = text.index("\t(sheet_instances")
text = text[:idx] + "\n".join(blocks) + "\n" + text[idx:]
sch.write_text(text)
print(f"added {len(blocks)} no-connect flags to {sch.name}")
