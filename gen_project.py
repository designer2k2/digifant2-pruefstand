"""
Reproducible driver for regenerating the Digifant-2 test-bench KiCad project.

Usage:  python3 gen_project.py digifant2_pruefstand_cs_v17

Handles the boilerplate that circuit-synth v0.1.0 does not do on its own:
- points the symbol/footprint resolvers at the vendored KiCad 9.0 libraries
- clears the footprint-index cache (it does not invalidate on new .kicad_mod files)
- generates the KiCad-format .net alongside the project
- forces placement_algorithm="sequential" (connection_aware overlaps big symbols)
- re-applies the 0.15mm Default-netclass clearance (does not persist across regens)
"""
import json
import os
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).parent
os.environ["KICAD_SYMBOL_DIR"] = str(HERE / "kicad-symbols")
os.environ["KICAD_FOOTPRINT_DIR"] = str(HERE / "kicad-footprints")

# stale footprint-index cache bug
cache = Path.home() / ".cache" / "circuit_synth" / "footprints"
if cache.exists():
    shutil.rmtree(cache)

from digifant2_pruefstand import main_circuit  # noqa: E402

name = sys.argv[1] if len(sys.argv) > 1 else "digifant2_pruefstand_cs"
out_dir = HERE / name
if out_dir.exists():
    shutil.rmtree(out_dir)
out_dir.mkdir(parents=True)

c = main_circuit()
# Passing <out_dir>/<name> makes the generator use <out_dir> as the base, so it
# writes <out_dir>/<name>.kicad_{pro,pcb,sch} and finds <out_dir>/<name>.net for
# net application. The hierarchical child sheet lands one level too deep, in
# <out_dir>/<name>/ -- moved up afterwards.
c.generate_kicad_netlist(str(out_dir / f"{name}.net"))
c.generate_kicad_project(str(out_dir / name), placement_algorithm="sequential")

nested = out_dir / name
if nested.is_dir():
    for f in nested.iterdir():
        shutil.move(str(f), str(out_dir / f.name))
    nested.rmdir()

# circuit-synth writes an empty net_settings; KiCad's default Default-netclass
# clearance (0.2mm) is too tight for the 0.5mm-pitch MSOP-10 parts. Inject a
# Default class at 0.15mm (fab-minimum budget) so the board is DRC-clean on load.
DEFAULT_NETCLASS = {
    "bus_width": 12, "clearance": 0.15, "diff_pair_gap": 0.25,
    "diff_pair_via_gap": 0.25, "diff_pair_width": 0.2, "line_style": 0,
    "microvia_diameter": 0.3, "microvia_drill": 0.1, "name": "Default",
    "pcb_color": "rgba(0, 0, 0, 0.000)", "priority": 2147483647,
    "schematic_color": "rgba(0, 0, 0, 0.000)", "track_width": 0.2,
    "via_diameter": 0.6, "via_drill": 0.3, "wire_width": 6,
}
# circuit-synth sizes the board outline to a tight bbox of its auto-placement, which
# leaves parts (the 60mm J0 terminal block especially) hanging over the edge and
# gives no routing room. Widen it -- the real outline gets drawn by hand at layout.
pcb = out_dir / f"{name}.kicad_pcb"
pcb.write_text(re.sub(r"(\(gr_rect\s*\(start 0 0\)\s*\(end )[\d.]+ [\d.]+(\))",
                      r"\g<1>140 140\g<2>", pcb.read_text(), count=1))

# Project-local library tables so both the user's KiCad and `kicad-cli` resolve
# the vendored 9.0 libs (otherwise every symbol/footprint throws lib_*_issues).
sym_rows = "".join(
    f'  (lib (name "{p.stem}")(type "KiCad")(uri "${{KIPRJMOD}}/../kicad-symbols/{p.name}")(options "")(descr ""))\n'
    for p in sorted((HERE / "kicad-symbols").glob("*.kicad_sym"))
)
(out_dir / "sym-lib-table").write_text(f"(sym_lib_table\n{sym_rows})\n")
fp_rows = "".join(
    f'  (lib (name "{p.stem}")(type "KiCad")(uri "${{KIPRJMOD}}/../kicad-footprints/{p.name}")(options "")(descr ""))\n'
    for p in sorted((HERE / "kicad-footprints").glob("*.pretty"))
)
(out_dir / "fp-lib-table").write_text(f"(fp_lib_table\n{fp_rows})\n")

pro = out_dir / f"{name}.kicad_pro"
data = json.loads(pro.read_text())
ns = data.setdefault("net_settings", {})
classes = [c for c in ns.get("classes", []) if c.get("name") != "Default"]
classes.insert(0, DEFAULT_NETCLASS)
ns["classes"] = classes
ns.setdefault("meta", {"version": 4})
ns.setdefault("netclass_assignments", None)
ns.setdefault("netclass_patterns", [])
pro.write_text(json.dumps(data, indent=2))
print(f"regenerated {out_dir}")

# --- ERC/DRC via the KiCad 10 flatpak (headless) --------------------------------
import subprocess  # noqa: E402

KCLI = ["flatpak", "run", "--filesystem=host",
        "--command=kicad-cli", "org.kicad.KiCad"]


def _kcli(*args):
    return subprocess.run(KCLI + list(args), capture_output=True, text=True)


sch = out_dir / "Digifant2_Pruefstand.kicad_sch"

# Known-cosmetic buckets (see [[circuit_synth_evaluation]]): circuit-synth embeds
# its own simplified symbol/footprint copies (-> *_mismatch), auto-names dead NC
# nets differently than KiCad's parity recompute (-> net_conflict on NC pins),
# and until the board is routed every ratsnest line is an "unconnected_items".
COSMETIC = ("lib_symbol_mismatch", "lib_footprint_mismatch", "net_conflict",
            "unconnected_items", "silk_over_copper", "silk_overlap",
            "silk_edge_clearance", "pin_to_pin")


def _real(rpt):
    lines = Path(rpt).read_text().splitlines() if Path(rpt).exists() else []
    return [l for l in lines if l.startswith("[") and not any(c in l for c in COSMETIC)]


if subprocess.run(["which", "flatpak"], capture_output=True).returncode == 0:
    _kcli("sch", "erc", "-o", str(out_dir / "ERC.rpt"), str(sch))
    subprocess.run([sys.executable, str(HERE / "add_nc_flags.py"), name])
    _kcli("sch", "erc", "-o", str(out_dir / "ERC.rpt"), str(sch))
    _kcli("pcb", "drc", "--schematic-parity", "-o", str(out_dir / "DRC.rpt"),
          str(out_dir / f"{name}.kicad_pcb"))
    for k in ("ERC", "DRC"):
        real = _real(out_dir / f"{k}.rpt")
        print(f"{k}: {len(real)} non-cosmetic" + (":" if real else " ✓"))
        for l in real:
            print("   ", l)
