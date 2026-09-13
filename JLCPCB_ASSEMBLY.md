# JLCPCB SMD assembly — how to use `jlcpcb_bom.csv` / `jlcpcb_cpl.csv`

Hybrid sourcing plan: let JLCPCB machine-place the 0.5mm-pitch ICs and 0805
passives (the fiddly stuff), hand-solder the rest yourself (THT connectors,
switches, the Pico module, and 4 SMD parts JLCPCB doesn't have a confirmed
part number for — see below). See [`BOM.md`](BOM.md) for full-board Mouser
sourcing if you'd rather hand-solder everything.

`jlcpcb_cpl.csv` positions/rotations came straight from the actual
`board/digifant2-pruefstand.kicad_pcb` (via `pcbnew`, not estimated) — 42
SMD parts, all on the top side. `jlcpcb_bom.csv` LCSC part numbers were
researched against lcsc.com; every number in the file was individually
confirmed there, nothing pattern-guessed made it into these two files
(unconfirmed items were excluded instead — see below).

## Before you submit: check placement rotation in JLCPCB's preview

JLCPCB's pick-and-place rotation convention doesn't always match KiCad's raw
`0°` orientation, especially for parts with a clear pin-1/polarity marking.
**Use JLCPCB's online assembly preview (it renders every part's actual
orientation) and visually check these before ordering** — a wrong rotation
here means a part soldered in backwards:

- **D1** (SS54 diode) — cathode band orientation
- **D2** (SK6812 LED) — has a required orientation (pin 1 / data-in corner)
- **Q1, Q2, Q3, Q5** (MOSFETs, SOT-23 / DPAK) — gate/drain/source pinout
- **U1–U4, U6–U8** (MSOP-10 / SOT-23-6 ICs) — pin 1 orientation

Everything else (0805 resistors/caps) is symmetric and rotation-safe.

## 4 SMD parts excluded — hand-solder these separately

No individually-confirmed LCSC part number turned up for these, so they're
**not** in `jlcpcb_bom.csv`/`jlcpcb_cpl.csv` — order them via
[`BOM.md`](BOM.md)'s Mouser numbers (or find your own LCSC listing) and
hand-solder after the assembled board arrives:

- **F1** (3A/1206 fuse) — only a Mouser/LCSC category was confirmed, no single SKU
- **RS1** (0.02Ω 1% shunt, 2512) — Mouser SKU confirmed (`CRA2512-FZ-R020ELF`), not independently found on LCSC
- **RS2** (0.033Ω 1% shunt, 2512) — value unconfirmed at *any* distributor; either track down `CRA2512-FZ-R033ELF` yourself, or substitute a confirmed 0.030Ω/0.025Ω part and adjust the INA226 calibration constant in firmware
- **Y1** (25MHz XO91-style oscillator) — exact EuroQuartz part confirmed to exist elsewhere, not pinned to a Mouser or LCSC SKU this pass

These 4, plus the always-hand-soldered THT parts (J0–J3, TP1–TP14, SW1–SW3, C1,
U5/Pico), are the same set flagged in `BOM.md`.

## Notable per-part caveats (already reflected in the BOM, just so you know why)

- **D1**: LCSC part `C16103` is the **SS54C** variant (DO-214AB/SMC — matches
  this board's footprint). Plain "SS54" on LCSC (`C22452`) is a *different,
  smaller* DO-214AC/SMA package — don't substitute it.
- **D2**: `C5378720` (OPSCO SK6812) is confirmed in stock and the right
  family/package class, but the exact PLCC-4 5.0×5.0mm footprint wasn't
  independently re-verified pin-for-pin against this board's footprint —
  worth a quick visual check against the datasheet before ordering qty 1.
- **U6/U7/U8** (TS5A3159ADBVR): flagged as possibly EOL at TI in one search
  pass, not corroborated in a second — check TI's lifecycle page before
  ordering qty 3, in case a last-time-buy applies.
- **100k resistor** (`C17407`): two LCSC listings for the same
  `0805W8F1003T5E` part number turned up (`C17407` and `C149504`) — either
  should work, `C17407` is what's in the BOM.
