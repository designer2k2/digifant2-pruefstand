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
| `board/` | the current KiCad project (144×154 mm, 2-layer, 4× M3 corner holes, isolated OLED zone) |
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
Board placed + autorouted (144 × 154 mm, 2-layer, 4 × M3 holes, isolated OLED
zone), DRC-clean except ~2-3 connections in the INA226 / shunt 0.5 mm-pitch
fanout left for KiCad's interactive router. Not yet fabbed.

## License

MIT (see `LICENSE`) for everything authored here — scripts, custom symbols, the
board design. Bundled third-party files (the Raspberry Pi Pico symbol/footprint
from [ncarandini/KiCad-RP-Pico](https://github.com/ncarandini/KiCad-RP-Pico),
CC-BY-SA-4.0) keep their own licenses — see `NOTICE`.
