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
| `board/` | the current KiCad project (144×169 mm, 2-layer, 4× M3 corner holes, isolated OLED + USB clearance zones) |
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
Board placed + autorouted (144 × 169 mm, 2-layer, 4 × M3 holes, isolated OLED + USB
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
pads on 0.1″ pitch) in the empty space bottom-right of J0. Not yet fabbed.

## License

MIT (see `LICENSE`) for everything authored here — scripts, custom symbols, the
board design. Bundled third-party files (the Raspberry Pi Pico symbol/footprint
from [ncarandini/KiCad-RP-Pico](https://github.com/ncarandini/KiCad-RP-Pico),
CC-BY-SA-4.0) keep their own licenses — see `NOTICE`.
