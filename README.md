# Digifant-2 Prüfstand

Hardware for a fully-automated test bench for the VW Golf 1 Cabrio **Digifant-2** ECU:
signal simulator + capture front-end, driven by a Raspberry Pi Pico, with a
high-side load switch so the bench can cold-boot the ECU and cut power on
overcurrent. Scope here is the **Signal-Simulator + Erfassungseinheit** only — the
EPROM emulator is a separate future board.

The schematic is generated from Python with
[circuit-synth](https://github.com/circuit-synth/circuit-synth); the PCB is placed
and autorouted headless (`pcbnew` + Freerouting).

**[`DESIGN.md`](DESIGN.md) — what every block does, part-selection rationale, the
GPIO / I²C / VW-harness maps.** [`USE_CASES.md`](USE_CASES.md) traces every real
bench use case through the schematic to concrete numbers (voltage/current/timing
headroom) — a requirements/test matrix for the hardware, and where the AD9833
decoupling-cap gap turned up before it got fixed. `article-draft.md` has the
wider project background.

## Layout

| path | what |
|------|------|
| `digifant2_pruefstand.py` | the circuit — one `@circuit` function, flat single sheet |
| `gen_custom_symbols.py` | builds `kicad-symbols/Custom_Digifant2.kicad_sym` (INA226, AD9833, 2N7002, DPAK P-FET, VW connector — pinouts verified against datasheets) |
| `gen_project.py` | `python3 gen_project.py <name>` → full KiCad project + netlist + lib tables, runs ERC/DRC via `kicad-cli`, auto-applies no-connect flags |
| `place_and_route.py` | `pcbnew` functional-zone placement → Specctra DSN → Freerouting → SES import (run inside the KiCad flatpak; `java` runs outside it) |
| `add_nc_flags.py` | stamps `no_connect` flags from an ERC report (circuit-synth has no NC API) |
| `fix_layout.py` | post-route layout surgery (real netclasses, GND/Kelvin re-routing) — see the header comment for the 2-phase, 2-process invocation it needs |
| `reroute_missing.py` | re-exports a Specctra DSN from a partially-ripped-up board so Freerouting only has to fill the reopened ratsnest |
| `board/` | the current KiCad project (144×150 mm, 2-layer, 4× M3 corner holes, isolated OLED + USB clearance zones) |
| [`board/bom_output/bom.html`](board/bom_output/bom.html) | interactive BOM ([InteractiveHtmlBom](https://github.com/openscopeproject/InteractiveHtmlBom)) — click a part to highlight it on the board, no KiCad needed |
| [`BOM.md`](BOM.md) | Mouser sourcing: part numbers for every line item, with confidence notes — 6 of 30 need manual confirmation, see the file |
| [`mouser_cart.csv`](mouser_cart.csv) | only the Mouser-verified part numbers, ready to paste into Mouser's bulk order tool |
| [`JLCPCB_ASSEMBLY.md`](JLCPCB_ASSEMBLY.md) | hybrid plan: JLCPCB machine-places the SMD parts, you hand-solder THT + 4 unconfirmed SMD parts — read this before submitting |
| [`jlcpcb_bom.csv`](jlcpcb_bom.csv) / [`jlcpcb_cpl.csv`](jlcpcb_cpl.csv) | JLCPCB SMT-assembly BOM + pick-and-place, 42 of 46 SMD parts (real positions from `board/`, real LCSC part numbers) |
| `UPSTREAM_BUGS.md` | circuit-synth bugs found, drafted for upstream filing |
| `kicad-symbols/`, `kicad-footprints/` | libraries — custom/community committed, stock via `./fetch_libs.sh` |

## Build

```sh
./fetch_libs.sh                        # one-time: pull stock KiCad 9.0 libs
python3 gen_custom_symbols.py          # if the custom symbols changed
python3 gen_project.py digifant2_pruefstand_cs_v1
```

Placement + routing needs KiCad's `pcbnew` Python API (KiCad ≥ 10) and a
non-headless JRE for Freerouting — see the header of `place_and_route.py`.

## Status

Schematic ERC-clean; VW harness pinout verified against the cabby-info.com
Digifant II reference and the user's own hands-on HiL notes (see `DESIGN.md`).
Board placed + autorouted (144 × 150 mm, 2-layer, 4 × M3 holes, isolated OLED + USB
clearance zones), real Power/Gnd netclasses, GND re-routed as one connected net
(was 4 islands), dedicated Kelvin sense traces for both shunts. The tight
U1/U3/RS1/RS2 pocket (2 shorting_items, 2 clearance, 5 tracks_crossing left by
the scripted Kelvin routing) was cleaned up by hand in KiCad's interactive
router, and a B.Cu GND pour was added. Added the two AD9833 decoupling caps
(C7, C8), fully routed. Fixed RS2 (idle-valve shunt) to be truly in series
with the ECU's VW-23 driver instead of a parallel path to GND that was
corrupting the current reading — this reopened one small pocket at U3 pin 9
(2 `shorting_items`, confined to that one spot after extensive
hand-routing/Freerouting attempts, same class of residual as the historical
U1/U3/RS1/RS2 items). Gave the AD9833 knock output a defined path out
(TP1/TP2 test points for VW-4/VW-5) instead of a dead end. Added 12 more
THT scope-probe test points (TP3–TP14, real drilled holes — an earlier pass
used bare SMD pads and put 10 of them physically underneath the Pico module,
both corrected) on the main GPIO signals, both buses, and both 12 V rails.
Found and fixed a real bug along the way: footprints cloned via
`pcbnew.FOOTPRINT(template)` inherit the template's UUID unless you
explicitly reset it, which had corrupted DRC's item-identity tracking for
every cloned part this session (C7/C8 and all of TP1–TP14) — several
"violations" turned out to be real defects reported against the wrong
footprint. See DESIGN.md's "Known open points" for details if you script
further additions. Remaining warnings are cosmetic (silkscreen overlap,
lib_footprint_mismatch, and one starved_thermal note on a single Pico GND
pad with only 1 pour spoke instead of 2 — connected fine, just a slightly
weaker thermal/mechanical joint). Thermal copper for D1/Q2 still optional to
add by hand. Added a 12×16 perfboard-style proto area (PROTO1, isolated THT
pads on 0.1″ pitch) in the empty space bottom-right of J0. Checked the whole
board against JLCPCB's published fab minimums (trace/spacing, via/hole sizes,
hole-to-hole, copper-to-edge, board thickness) — everything copper- and
hole-related had 1.5×–14× margin already. The one real miss was silkscreen:
KiCad's default 0.1mm line width and 0.8mm text height are both below
JLCPCB's stated minimums (0.15mm / 1.0mm), risking thin/dropped silkscreen.
Bumped the project defaults and widened/enlarged every existing silkscreen
line and text item on the board to meet those minimums.

Checked +3V3 for adequate trace width (it's on the thinnest `Default`
netclass, 0.2mm, despite feeding ~15 chips) — worked out the real numbers
(thermal capacity ~0.77A at 10°C rise, worst-case drop a few mV) and it's
fine as-is; not worth the rerouting risk in this densely-packed board.
Checked decoupling instead: per each part's own datasheet, U1 and U3 (the
two INA226 current-sense amps) had no bypass cap at all, unlike U2/U4 which
already have theirs (C6, C2/C7/C8). Added C9 (U1) and C10 (U3), 100nF 0805,
tapped directly off the existing +3V3 B.Cu trunk and the GND pour a few mm
south of the packed U1/U3/RS1/RS2 pocket (no room to place them right at the
pins) — verified DRC-clean (the only residuals are the same
new-copper-vs-stale-zone-fill noise that clears on a zone refill, same as
after the TP resort). U6/U7/U8 (the sensor-disconnect analog switches) are
still without a bypass cap; lower priority since they're not
noise-sensitive, but could use the same treatment.

Added the thermal copper for D1/Q2 that was flagged as a real (not
cosmetic) risk in USE_CASES.md — D1 dissipates ~1.25W at sustained ~2.5A
with no thermal relief otherwise. Both pours verified electrically
connected to their pads (not just overlapping) and DRC-clean. Q2's is
small since there's almost no open board space around its DPAK tab; D1's
is generous since F1/RS1 (same nets, either side) leave a wide open
corridor.

**All 14 test points (TP1–TP14) are now fully routed** — the user hand-routed
7 of the 10 remaining GPIO test points after the resort (TP3, TP10, TP11,
TP4, TP12, TP5, TP6); the last 3 (TP9, TP8, TP7 — all on U5's east side, the
tightest part of the board) were routed with a grid-based pathfinder (A* over
a 0.1mm grid, spatial-bucket-indexed for speed) purpose-built for this corridor
after several rounds of manual/heuristic routing kept snagging on real
obstacles: the Pico's own pin column has no gap wide enough for a second
trace anywhere along its length (confirmed by direct probing — routed escape
stubs from other GPIOs fill in what would otherwise be a 0.49mm theoretical
gap between pin pads), so both TP8 and TP9 had to detour around the *south*
end of the Pico footprint entirely, and — since that detour corridor is
itself only wide enough for one trace at a time — needed genuinely different
physical crossing points, not just parallel lanes. Verified via real
`kicad-cli pcb drc`, not just the pathfinder's own clearance model (which
undercounted real collisions during development in two ways worth noting for
next time: it modeled non-circular pads by their half-width instead of their
half-diagonal, and it didn't check hole-to-hole spacing against a net's own
via near its own pad — both are now fixed in the approach, see `DESIGN.md`).
GND zone refilled after all of the above (TP9/TP8 routing, C9/C10, D1/Q2
thermal pours) — DRC dropped from 136 to 101 violations, all pre-existing
cosmetic categories plus the one known `starved_thermal` note, zero
clearance/short/crossing issues. (The "whole-board refill introduces ~115
spurious violations" finding earlier this session was a testing artifact —
running `kicad-cli` against a bare copy of the `.kicad_pcb` without its
paired `.kicad_pro` silently falls back to stricter default netclass rules;
with the project file present, the refill is clean.)

Trimmed the board from 144×169mm to 144×150mm — the bottom 19mm was
completely empty (bare GND pour, nothing below PROTO1/J3 until the old
mounting holes). H3/H4 moved up to keep the same 4.5mm hole-to-edge margin
as H1/H2; DRC stayed clean (99 violations, same pre-existing cosmetic set).

Added functional silkscreen labels next to J0 (the VW harness connector) and
every test point, instead of leaving bare unlabeled pads. J0 gets each pin's
real VW harness number (VW-2, VW-9, ... or GND / +12V SW) in the clear strip
just outside its footprint. The TPs already had descriptive text (e.g.
`GP2_CRANK`) sitting on them from earlier in the session — turned out it was
on **F.Fab**, a documentation-only layer that doesn't actually get printed on
the physical board, so none of it was ever going to be visible with a probe
in hand. Moved it to F.SilkS and shortened the long ones (e.g.
`GP6_THROTTLE_IDLE_SW` → `IDLE SW`) since the west-margin TP column only has
about 10mm of clearance between the board edge and the Pico's pin column —
the original full-length strings would have run off the board edge once
actually rendered as real silkscreen (checked via real bounding-box widths,
not by eye). This did add some new `silk_overlap` hits — mostly TP labels
sitting close to the Pico module's own tiny per-pin silkscreen text in that
same tight corridor — all cosmetic, same accepted category as the rest of
this board's silkscreen warnings, verified via DRC to introduce zero
clearance/short/crossing issues.

Added fab-identification text on the back silkscreen (B.SilkS, previously
completely empty — no back components, nothing on that layer at all):
project name, revision, date, and designer, so a bare board can still be
identified later. Set via the board's title block (`${TITLE}`,
`${REVISION}`, `${ISSUE_DATE}`, `${COMPANY}`) rather than hardcoded text, so
updating the revision/date for a future respin is a one-place edit in
KiCad's Page Settings, not a silkscreen re-edit. Verified the variables
resolve correctly (checked the exported SVG's embedded text, since this
sandbox can't render the actual glyphs) and confirmed zero new DRC
violations. Not yet fabbed.

## License

MIT (see `LICENSE`) for everything authored here — scripts, custom symbols, the
board design. Bundled third-party files (the Raspberry Pi Pico symbol/footprint
from [ncarandini/KiCad-RP-Pico](https://github.com/ncarandini/KiCad-RP-Pico),
CC-BY-SA-4.0) keep their own licenses — see `NOTICE`.
