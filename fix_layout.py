"""
Post-route layout surgery, per the deep layout review (2026-09-11):
  1. Real 0.8mm 'Power' / 0.5mm 'Gnd' netclasses (were DSN-only, not in the
     KiCad project -- any hand-routed trace defaulted to 0.2mm).
  2. Rip up the ad-hoc GND and +12V_ECU_SW tracks Freerouting produced.
  3. Dedicated, direct Kelvin-sense traces for both shunts (were unrouted or,
     for U3, routed into the general GND daisy chain 18.7mm from the shunt).
  4. +12V_ECU_SW re-routed as a star from Q2's tab instead of a 135mm chain
     through J2 with two extra vias.
  5. GND stitching via at every SMD-only GND pad, then a B.Cu GND pour.
  6. Small F.Cu thermal-relief zones under D1 and Q2's tab.

Run as TWO separate process invocations (each `pcbnew.LoadBoard` call after the
first one in a process returns broken SWIG proxies -- a binding fragility, not
a KiCad bug):
  flatpak run --filesystem=host --command=python3 org.kicad.KiCad \
      fix_layout.py <projdir> <basename> --phase1
  flatpak run --filesystem=host --command=python3 org.kicad.KiCad \
      fix_layout.py <projdir> <basename> --phase2
Then re-run kicad-cli pcb drc and fix anything it flags.
"""
import json
import sys
from pathlib import Path
import pcbnew

proj = Path(sys.argv[1])
base = sys.argv[2]
phase = sys.argv[3]
pcb_path = str(proj / f"{base}.kicad_pcb")
pro_path = proj / f"{base}.kicad_pro"
MM = pcbnew.FromMM


def pad(fps, ref, num):
    for p in fps[ref].Pads():
        if p.GetPadName() == str(num):
            return p
    raise KeyError((ref, num))


if phase == "--phase1":
    b = pcbnew.LoadBoard(pcb_path)

    # 1. netclasses in the .kicad_pro
    data = json.loads(pro_path.read_text())
    ns = data.setdefault("net_settings", {})
    POWER_NETS = ["VIN_12V", "+12V_PROT", "+12V_POST_D1", "+12V_ECU",
                  "+12V_ECU_SW", "VW_PIN23_VALVE_RETURN"]

    def netclass(name, track, clearance, via_dia, via_drill):
        return {
            "bus_width": 12, "clearance": clearance, "diff_pair_gap": 0.25,
            "diff_pair_via_gap": 0.25, "diff_pair_width": 0.2, "line_style": 0,
            "microvia_diameter": 0.3, "microvia_drill": 0.1, "name": name,
            "pcb_color": "rgba(0, 0, 0, 0.000)", "priority": 2147483647,
            "schematic_color": "rgba(0, 0, 0, 0.000)", "track_width": track,
            "via_diameter": via_dia, "via_drill": via_drill, "wire_width": 6,
        }

    classes = [c for c in ns.get("classes", []) if c.get("name") not in ("Power", "Gnd")]
    # Clearance stays 0.15mm everywhere (matches Default, and what's already
    # routed): the MSOP-10 parts have ~0.15mm pad-to-pad spacing on some of
    # these nets, so a 0.2mm clearance class makes adjacent PADS on the same
    # IC fail DRC against each other. Only track/via size increases.
    classes.append(netclass("Power", 0.8, 0.15, 0.6, 0.3))
    classes.append(netclass("Gnd", 0.5, 0.15, 0.6, 0.3))
    ns["classes"] = classes
    ns["netclass_patterns"] = (
        [p for p in ns.get("netclass_patterns", [])
         if p.get("netclass") not in ("Power", "Gnd")]
        + [{"netclass": "Power", "pattern": n} for n in POWER_NETS]
        + [{"netclass": "Gnd", "pattern": "GND"}]
    )
    pro_path.write_text(json.dumps(data, indent=2))
    print("netclasses written")

    # 2. rip up the ad-hoc GND / +12V_ECU_SW routing
    removed = 0
    for t in list(b.GetTracks()):
        if t.GetNetname() in ("GND", "+12V_ECU_SW"):
            b.Remove(t)
            removed += 1
    print(f"removed {removed} GND/+12V_ECU_SW track+via segments")
    pcbnew.SaveBoard(pcb_path, b)
    print("phase1 done")

elif phase == "--phase2":
    b = pcbnew.LoadBoard(pcb_path)
    fps = {f.GetReference(): f for f in b.GetFootprints()}

    def pt(ref, num):
        p = pad(fps, ref, num).GetPosition()
        return pcbnew.VECTOR2I(p.x, p.y)

    def seg(net, p1, p2, width_mm, layer=pcbnew.F_Cu):
        t = pcbnew.PCB_TRACK(b)
        t.SetStart(p1)
        t.SetEnd(p2)
        t.SetWidth(MM(width_mm))
        t.SetLayer(layer)
        t.SetNet(net)
        b.Add(t)

    def kelvin(ref, padn, shunt_ref, shunt_pad, escape_dx_mm, escape_dy_mm=0.0):
        """Route a 0.5mm-pitch INA226 sense pin out to its shunt tap. First leg
        exits perpendicular to the pin row so it can't graze the neighbouring
        pin 0.5mm away; second leg angles to the shunt pad. 0.2mm wide -- as
        thin as the netclass allows, to minimise the clearance footprint next
        to the next pin over."""
        p_pin = pt(ref, padn)
        p_mid = pcbnew.VECTOR2I(p_pin.x + MM(escape_dx_mm), p_pin.y + MM(escape_dy_mm))
        p_shunt = pt(shunt_ref, shunt_pad)
        net = pad(fps, ref, padn).GetNet()
        seg(net, p_pin, p_mid, 0.2)
        seg(net, p_mid, p_shunt, 0.2)

    # 3. dedicated Kelvin sense traces (escape west, away from the package,
    # since RS1/RS2 both sit to the west of U1/U3). This small area (U1/U3's
    # own GND+address-pin fanout is packed right next to it) is the one spot
    # where a couple of clearance/crossing items remain despite several
    # escape-direction attempts -- every direction tried grazes *something*
    # (Freerouting's own GND fanout, or the power row further west). Left for
    # a quick manual nudge in KiCad's interactive router, same as previous
    # sessions' INA226-fanout residuals.
    kelvin("U1", 10, "RS1", 1, -2.0)   # IN+  -> shunt hi (+12V_POST_D1 side)
    kelvin("U1", 9, "RS1", 2, -2.0)    # IN-  -> shunt lo (+12V_ECU side)
    kelvin("U3", 10, "RS2", 1, -2.0)   # IN+  -> shunt hi (valve_return side)
    kelvin("U3", 9, "RS2", 2, -2.0)    # IN-  -> shunt's own GND pad, not the
                                        #  general daisy chain (star-ground)

    # 4. +12V_ECU_SW: NOT hand-routed. The old 135mm chain through J2 was
    # ripped up in phase1; re-running Freerouting on just the now-open
    # ratsnest (see fix_layout_reroute.sh) reconnects it directly and safely
    # instead of risking hand-drawn diagonals through the dense U1/U3/DAC
    # rows.

    # 5. GND pour + via-stitching: dropped. This board is dense enough
    # (0.5mm-pitch MSOP-10/SOT-23-6 parts everywhere) that blind-coordinate
    # via/zone placement kept relocating shorts rather than fixing them.
    # GND topology is already fixed structurally: phase1 rips up the old
    # ad-hoc daisy chain and Freerouting reconnects GND as ONE net (verified
    # via DRC: 0 unconnected GND items, vs. 3 island-boundary reports on the
    # original board) using the real 0.5mm Gnd netclass. A literal copper
    # pour is still worth adding by hand in the KiCad GUI once you finish
    # the interactive routing -- Draw Filled Zone on B.Cu, GND net.

    # 6. Thermal copper for D1/Q2: dropped. Same fragility as the GND pour --
    # a blindly-offset zone came out geometrically disconnected from the pad
    # it was meant to heatsink (isolated_copper). Add these by hand in the
    # KiCad GUI (Draw Filled Zone on F.Cu, on +12V_PROT / +12V_POST_D1 next to
    # D1, and on +12V_ECU_SW next to Q2's tab) where you can see the overlap.

    pcbnew.SaveBoard(pcb_path, b)
    print("phase2 done: saved")

else:
    print("pass --phase1 or --phase2")
    sys.exit(1)
