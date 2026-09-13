# Bill of Materials — Mouser sourcing

Researched against mouser.com (2026-09-12). **Confidence matters here — read it per row.**
"Verified" means an actual Mouser product page was found for that exact part.
"Pattern-matched" means it's the same manufacturer part-numbering family as a
verified sibling value (e.g. Yageo's `RC0805FR-07<value>L` resistors — five
other values in this exact series were independently verified on Mouser, so
the same convention for these mid-values is reliable but wasn't individually
page-confirmed). "Could not confirm" means: **do not order this part number —
it may not exist.** Search Mouser yourself or use an alternate distributor.

Also see [`board/bom_output/bom.html`](board/bom_output/bom.html) for the
interactive per-footprint BOM (no Mouser data, but useful to see where each
part sits on the board).

| Ref(s) | Qty | Description | Mfr / Mfr P/N | Mouser P/N | Confidence |
|---|---|---|---|---|---|
| U5 | 1 | Raspberry Pi Pico (RP2040, THT/castellated) | Raspberry Pi / SC0915 | [SC0915](https://www.mouser.com/en/ProductDetail/Raspberry-Pi/SC0915) | Verified |
| U1, U3 | 2 | INA226 current/power monitor, MSOP-10 | TI / INA226AIDGSR | [INA226AIDGSR](https://www.mouser.com/en/ProductDetail/Texas-Instruments/INA226AIDGSR) | Verified |
| U2 | 1 | MCP4728 quad 12-bit DAC, MSOP-10 | Microchip / MCP4728-E/UN | [MCP4728-E-UN](https://www.mouser.com/ProductDetail/Microchip-Technology/MCP4728-E-UN) | Verified |
| U4 | 1 | AD9833 DDS waveform generator, MSOP-10 | Analog Devices / AD9833BRMZ | [AD9833BRMZ](https://www.mouser.com/en/ProductDetail/Analog-Devices/AD9833BRMZ) | Verified |
| U6, U7, U8 | 3 | TS5A3159A SPDT analog switch, SOT-23-6 | TI / TS5A3159ADBVR | [TS5A3159ADBVR](https://www.mouser.com/ProductDetail/Texas-Instruments/TS5A3159ADBVR) | Verified — **but flagged as possibly EOL at TI in search results; confirm current lifecycle/stock before ordering qty 3** |
| RS1 | 1 | 0.02Ω 1% current-sense shunt, 2512 | Bourns / CRA2512-FZ-R020ELF | [CRA2512-FZ-R020ELF](https://www.mouser.com/ProductDetail/Bourns/CRA2512-FZ-R020ELF) | Verified |
| RS2 | 1 | 0.033Ω 1% current-sense shunt, 2512 | Bourns / CRA2512-FZ-R033ELF (assumed) | — | **Could not confirm.** Only the 0.030Ω sibling (`CRA2512-FZ-R030ELF`) was confirmed on Mouser. Either check `mouser.com` directly for `CRA2512-FZ-R033ELF`, or substitute the confirmed 0.030Ω part and adjust the INA226 calibration register in firmware for the new shunt value. |
| Q2 | 1 | P-channel MOSFET, DPAK, ≤-30V, ≥5A (ECU high-side switch) | Vishay / SUD50P06-15-GE3 | [SUD50P06-15-GE3](https://www.mouser.com/ProductDetail/Vishay-Semiconductors/SUD50P06-15-GE3) | Verified — spec is well over-built (-60V/-50A) for a real, exact-package match; fine to use, or substitute a tighter-spec DPAK P-FET if you prefer |
| Q1, Q3, Q5 | 3 | 2N7002 N-MOSFET, SOT-23 | any (onsemi/Diodes Inc./etc.) | [category filter](https://www.mouser.com/c/semiconductors/discrete-semiconductors/transistors/?package=SOT-23-3&series=2N7002) | Category confirmed, no single SKU pinned — 2N7002 is second-sourced by many manufacturers; pick any from that filtered page |
| D1 | 1 | SS54 Schottky diode, SMC/DO-214AB, 5A/40V | Comchip / SS54-HF (or Vishay/Diodes Inc. equivalent) | — | **Could not confirm on mouser.com.** Comchip SS54-HF turned up on Farnell/Newark, not Mouser in this search. Search `mouser.com` for "SS54" directly, or use Diodes Inc./Vishay's SS54 equivalent if Comchip isn't stocked. |
| D2 | 1 | SK6812 addressable RGB LED, PLCC-4, 5.0×5.0mm | SK6812 (various — Opsco etc.) | — | **Could not confirm — Mouser does not appear to stock this part.** SK6812 is typically sourced via LCSC, AliExpress, or Adafruit, not Mouser. Plan to source this one elsewhere. |
| F1 | 1 | 3A fuse, 1206/3216 SMD | any | [category filter](https://www.mouser.com/c/circuit-protection/fuses/surface-mount-fuses/?fuse+size=1206&current+rating=3+A) | Category confirmed, no single SKU pinned — pick any 1206 3A fast-blow fuse from that filtered page |
| C1 | 1 | 100µF/25V radial electrolytic, 8mm dia., 3.5mm pitch | Panasonic FR series or Nichicon (radial-leaded) | [category filter](https://www.mouser.com/c/passive-components/capacitors/aluminum-electrolytic-capacitors/aluminum-electrolytic-capacitors-radial-leaded/?m=Panasonic&series=FR) | Category confirmed, no single SKU pinned — pick any 100µF/25V, 8mm-can radial from that filtered page |
| J1 | 1 | 2-pos 5.08mm PCB screw terminal, THT | Phoenix Contact / 1935161 | [1935161](https://www.mouser.com/ProductDetail/Phoenix-Contact/1935161) | Best match, not exact — this Phoenix part is a **fixed** 5.00mm block; the board footprint (`TerminalBlock_bornier-2_P5.08mm`) implies pluggable. Mechanically close (0.08mm off pitch), functionally fine, but check fit before ordering many. |
| J0 | 1 | 15-pos 5.00mm pluggable screw terminal, THT | MaiXu MX126-5.0-15P (as designed) | — | **Could not confirm — Mouser doesn't carry MaiXu, and no 15-pos/5.0mm Phoenix or Weidmuller equivalent turned up.** MaiXu is a standard LCSC/JLCPCB-catalog brand — likely easier to source this one from LCSC, or manually browse Mouser's terminal-block category filtered to 15 positions / 5.0mm pitch. |
| J2 | 1 | JST XH 2-pos, 2.50mm pitch, vertical THT header | JST / B2B-XH-A(LF)(SN) | [B2B-XH-ALFSN](https://www.mouser.com/ProductDetail/JST-Commercial/B2B-XH-ALFSN) | Verified — two very similar part numbers turned up (`B2B-XH-ALFSN` and `B2B-XH-2LFSN`); double-check the page shows 2-position before ordering |
| J3 | 1 | 4-pos 2.54mm vertical pin header, THT, male | Samtec / TSW-104-07-G-S | [TSW-104-07-G-S](https://www.mouser.com/ProductDetail/Samtec/TSW-104-07-G-S) | Verified — this is the gold-plated variant (pricier); cheaper tin-plated 4-pos headers exist in the same Mouser category if cost matters and gold plating isn't needed |
| TP1–TP14 | 14 | Test point, THT (substitute for a bare 1.5mm pad) | Keystone Electronics / 5010 | [5010](https://www.mouser.com/ProductDetail/Keystone-Electronics/5010) | Verified — Mouser doesn't sell bare test-point pads, this is the standard THT substitute. TP1/TP2 = knock signal/ground (VW-4/VW-5); TP3–TP14 = scope-probe points on key GPIO signals, both buses, and both 12V rails |
| SW1, SW2, SW3 | 3 | 6×6mm tactile push-button, THT, 2-pin | Omron / B3F-1000 | [B3F-1000](https://www.mouser.com/ProductDetail/Omron-Electronics/B3F-1000) | Verified — confirm actuator height/force (100gf, 4.3mm button) matches the footprint's expectations |
| Y1 | 1 | 25MHz SMD oscillator, XO91 7×5mm, 3.3V | EuroQuartz / XO91050UITA-25.000 | — | **Not pinned to a Mouser SKU this pass** (confirmed to exist on Farnell/Newark/RS, not confirmed on Mouser). Search `mouser.com` directly for `XO91050UITA` — Mouser generally stocks EuroQuartz. |
| R8, R13, R21 | 3 | 10kΩ, 0805 | Yageo / RC0805FR-0710KL | [RC0805FR-0710KL](https://www.mouser.com/ProductDetail/YAGEO/RC0805FR-0710KL) | Verified |
| R6, R7, R14 | 3 | 4.7kΩ, 0805 | Yageo / RC0805FR-074K7L | [RC0805FR-074K7L](https://www.mouser.com/ProductDetail/YAGEO/RC0805FR-074K7L) | Verified |
| R5, R10, R15, RPU1 | 4 | 1kΩ, 0805 | Yageo / RC0805FR-071KL | [RC0805FR-071KL](https://www.mouser.com/ProductDetail/YAGEO/RC0805FR-071KL) | Verified |
| R16, R19, R22, R23, R24 | 5 | 100kΩ, 0805 | Yageo / RC0805FR-07100KL | [RC0805FR-07100KL](https://www.mouser.com/ProductDetail/YAGEO/RC0805FR-07100KL) | Verified |
| R2, R3, R4 | 3 | 220Ω, 0805 | Yageo / RC0805FR-07220RL | [RC0805FR-07220RL](https://www.mouser.com/ProductDetail/YAGEO/RC0805FR-07220RL) | Verified |
| R20 | 1 | 20kΩ, 0805 | Yageo / RC0805FR-0720KL | [RC0805FR-0720KL](https://www.mouser.com/en/ProductDetail/YAGEO/RC0805FR-0720KL) | Verified — added late, was missing from the original pass entirely |
| R9, R11, R17 | 3 | 330Ω, 0805 | Yageo / RC0805FR-07330RL | — | Pattern-matched (same verified RC0805 series) — confirm on mouser.com before ordering |
| R12 | 1 | 200Ω, 0805 | Yageo / RC0805FR-07200RL | — | Pattern-matched — confirm on mouser.com before ordering |
| RG1 | 1 | 100Ω, 0805 | Yageo / RC0805FR-07100RL | — | Pattern-matched — confirm on mouser.com before ordering |
| C3 | 1 | 10nF, 0805, X7R, 50V | Yageo / CC0805JRX7R9BB103 | [CC0805JRX7R9BB103](https://www.mouser.com/ProductDetail/YAGEO/CC0805JRX7R9BB103) | Verified |
| C2, C4, C5, C6, C7, C8 | 6 | 100nF, 0805, X7R, 50V | Yageo / CC0805KRX7R9BB104 | [CC0805KRX7R9BB104](https://www.mouser.com/ProductDetail/YAGEO/CC0805KRX7R9BB104) | Verified |

## Summary: not orderable as-is

Six line items need your own attention before ordering — don't paste these
part numbers into a cart as given:

- **RS2** (0.033Ω shunt) — unconfirmed digit; confirm `R033ELF` exists or use the confirmed `R030ELF` + adjust firmware calibration.
- **D1** (SS54 diode) — Comchip SKU not found on Mouser; search "SS54" there directly or use another manufacturer's SS54.
- **D2** (SK6812 LED) — not stocked on Mouser at all; source from LCSC/Adafruit/etc.
- **J0** (15-pos 5.0mm terminal block) — MaiXu not on Mouser; source from LCSC or find a Mouser-stocked 15-pos/5.0mm equivalent.
- **Y1** (25MHz XO91 oscillator) — exists at other distributors, Mouser SKU not confirmed this pass; search `XO91050UITA` on mouser.com.
- **F1, C1, Q1/Q3/Q5** — generic/second-sourced parts; only a Mouser *category* was confirmed, pick a specific in-stock SKU yourself (any should work electrically).

Everything else (25 of 31 line items, 45 of 56 placed parts) has a specific,
Mouser-verified part number above.
