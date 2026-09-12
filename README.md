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
GPIO / I²C / VW-harness maps.** `article-draft.md` has the wider project background.

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
| `board/` | the current KiCad project (144×169 mm, 2-layer, 4× M3 corner holes, isolated OLED + USB clearance zones) |
| [`board/bom_output/bom.html`](board/bom_output/bom.html) | interactive BOM ([InteractiveHtmlBom](https://github.com/openscopeproject/InteractiveHtmlBom)) — click a part to highlight it on the board, no KiCad needed |
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
Board placed + autorouted (144 × 169 mm, 2-layer, 4 × M3 holes, isolated OLED + USB
clearance zones), real Power/Gnd netclasses, GND re-routed as one connected net
(was 4 islands), dedicated Kelvin sense traces for both shunts. DRC still flags a
real cluster of violations (2 shorting_items, 2 clearance, 5 tracks_crossing, 5
unconnected) confined entirely to the tight U1/U3/RS1/RS2 pocket where the Kelvin
escapes were hand-routed — every escape direction tried there grazes something
(Freerouting's own dense GND fanout on one side, the power row on the other), so
it's left for a manual nudge in KiCad's interactive router rather than a scripted
fix. Everywhere else on the board DRC is clean (remaining warnings are cosmetic
silkscreen/lib_footprint_mismatch noise, harmless). Ground pour + thermal copper
for D1/Q2 still to add by hand in the KiCad GUI. Not yet fabbed.

## License

MIT (see `LICENSE`) for everything authored here — scripts, custom symbols, the
board design. Bundled third-party files (the Raspberry Pi Pico symbol/footprint
from [ncarandini/KiCad-RP-Pico](https://github.com/ncarandini/KiCad-RP-Pico),
CC-BY-SA-4.0) keep their own licenses — see `NOTICE`.
