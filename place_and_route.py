"""
Headless placement + autoroute for the Digifant-2 board.

Full pipeline (java freerouting must run OUTSIDE the flatpak; needs a DISPLAY):
  FP="flatpak run --filesystem=host --command=python3 org.kicad.KiCad"
  D=digifant2_pruefstand_cs_v26
  $FP place_and_route.py $D            # functional placement + outline + M3 holes
  $FP place_and_route.py $D --dsn      # export Specctra .dsn + inject power class
  Xvfb :99 & DISPLAY=:99 java -jar tools/freerouting.jar -de $D/$D.dsn -do $D/$D.ses -mp 100
  $FP place_and_route.py $D --ses      # import routed .ses back into the .kicad_pcb
  cd $D && $FP-less kicad-cli pcb drc ...

Notes:
- freerouting 1.9.0 (tools/freerouting.jar) needs a real DISPLAY even in batch mode
  and a NON-headless JRE (openjdk-17-jre). 2.x needs Java 25.
- a handful of connections in the INA226/shunt 0.5mm-pitch fanout are left for
  KiCad's interactive router - too tight for the autorouter.
"""
import re
import sys
from pathlib import Path
import pcbnew

proj = Path(sys.argv[1])
mode = sys.argv[2] if len(sys.argv) > 2 else ""
pcb_path = str(proj / f"{proj.name}.kicad_pcb")
b = pcbnew.LoadBoard(pcb_path)
MM = pcbnew.FromMM
fps = {f.GetReference(): f for f in b.GetFootprints()}

W, H = 130.0, 100.0          # content area
MARGIN = 7.0                 # border reserved for the corner mounting holes
BW, BH = W + 2 * MARGIN, H + 2 * MARGIN
OX, OY = MARGIN, MARGIN
GAP = 2.0
HERE = Path(__file__).parent


def place_bbox_topleft(ref, x, y, rot):
    f = fps[ref]
    f.SetOrientationDegrees(rot)
    bb = f.GetBoundingBox(False, False)
    dx = f.GetPosition().x - bb.GetLeft()
    dy = f.GetPosition().y - bb.GetTop()
    f.SetPosition(pcbnew.VECTOR2I(MM(x + OX) + dx, MM(y + OY) + dy))


def flow(refs, x0, y0, x_max, rot=0, gap=GAP):
    """refs entries are either a ref string, or (ref, rot_override) to rotate just
    that part (e.g. 180 deg to flip pin order without changing footprint size, so
    traces don't have to route around it)."""
    x, y, row_h = x0, y0, 0.0
    for item in refs:
        ref, this_rot = item if isinstance(item, tuple) else (item, rot)
        f = fps.get(ref)
        if not f:
            print("MISSING", ref); continue
        f.SetOrientationDegrees(this_rot)
        bb = f.GetBoundingBox(False, False)
        w, h = bb.GetWidth() / 1e6, bb.GetHeight() / 1e6
        if x + w > x_max and x > x0:
            x = x0; y += row_h + gap; row_h = 0.0
        place_bbox_topleft(ref, x, y, this_rot)
        x += w + gap
        row_h = max(row_h, h)


def place():
    place_bbox_topleft("J0", W - 11, 6, 90)     # VW harness, right edge
    place_bbox_topleft("J1", 2, 6, 90)          # 12V in, left edge
    place_bbox_topleft("U5", 3, 24, 0)          # Pico
    flow(["F1", ("D1", 180), "RS1", "C1", "Q2", "R13", "C3", "R14", "Q3", "R15", "R16"],
         14, 6, W - 14)                          # power chain
    flow(["RS2", "U1", "U3", "R6", "R7"], 40, 26, W - 14)          # current sense
    flow(["U2", "C6", "R2", "R3", "R4", "R5", "R20", "R21", "C5",
          "U6", "R22", "U7", "R23", "U8", "R24"], 40, 40, W - 14)  # DAC + AFM sense + sensor disconnects
    flow(["U4", "Y1", "R12", "C2", "TP1"], 40, 54, W - 14)        # DDS / knock
    flow(["R8", "R9", "R10", "R11", "RG1", "Q1", "RPU1",
          "Q4", "R18", "Q5", "R19"], 40, 68, W - 14)             # edge capture + crank + throttle sw
    flow(["SW1", "SW2", "SW3", "R17", "C4", "D2", "J3", "J2"], 6, H - 16, W - 6)

    for d in list(b.GetDrawings()):
        if d.GetLayer() == pcbnew.Edge_Cuts:
            b.Remove(d)
    for x1, y1, x2, y2 in [(0, 0, BW, 0), (BW, 0, BW, BH), (BW, BH, 0, BH), (0, BH, 0, 0)]:
        s = pcbnew.PCB_SHAPE(b)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetStart(pcbnew.VECTOR2I(MM(x1), MM(y1)))
        s.SetEnd(pcbnew.VECTOR2I(MM(x2), MM(y2)))
        s.SetWidth(MM(0.1))
        b.Add(s)

    lib = str(HERE / "kicad-footprints" / "MountingHole.pretty")
    ins = 4.5
    for i, (hx, hy) in enumerate([(ins, ins), (BW - ins, ins),
                                  (ins, BH - ins), (BW - ins, BH - ins)]):
        mh = pcbnew.FootprintLoad(lib, "MountingHole_3.2mm_M3")
        mh.SetReference(f"H{i + 1}")
        mh.SetPosition(pcbnew.VECTOR2I(MM(hx), MM(hy)))
        mh.SetBoardOnly(True)   # not in the schematic -> don't flag as "extra footprint"
        b.Add(mh)

    b.Save(pcb_path)
    print(f"placed; board {BW}x{BH} mm, 4 M3 holes")


def export_dsn():
    dsn = proj / f"{proj.name}.dsn"
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


def import_ses():
    pcbnew.ImportSpecctraSES(b, str(proj / f"{proj.name}.ses"))
    pcbnew.SaveBoard(pcb_path, b)
    print("tracks after SES:", len(b.GetTracks()))


{"": place, "--dsn": export_dsn, "--ses": import_ses}[mode]()
