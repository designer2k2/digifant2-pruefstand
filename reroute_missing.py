"""Export DSN for a partially-routed board (only the ripped-up nets are
unrouted; existing tracks are preserved by Freerouting) and inject the Power
class, same as place_and_route.py's export_dsn but pointed at an arbitrary
pcb filename."""
import re
import sys
from pathlib import Path
import pcbnew

proj = Path(sys.argv[1])
base = sys.argv[2]
pcb_path = str(proj / f"{base}.kicad_pcb")
b = pcbnew.LoadBoard(pcb_path)
dsn = proj / f"{base}.dsn"
print("DSN:", pcbnew.ExportSpecctraDSN(b, str(dsn)))

P = ["VIN_12V", "+12V_PROT", "+12V_POST_D1", "+12V_ECU", "+12V_ECU_SW",
     "VW_PIN23_VALVE_RETURN"]
t = dsn.read_text()
kd = re.search(r"\(class kicad_default\s+(.*?)\n      \(circuit", t, re.S)
keep = [n for n in kd.group(1).split() if n not in P]
t = t[:kd.start()] + "(class kicad_default\n      " + " ".join(keep) + \
    "\n      (circuit" + t[kd.end():]
i = t.rindex("  )\n  (wiring")
t = t[:i] + (f"    (class power {' '.join(P)}\n"
             "      (circuit (use_via \"Via[0-1]_600:300_um\"))\n"
             "      (rule (width 600) (clearance 150))\n    )\n") + t[i:]
dsn.write_text(t)
print("added power class")
