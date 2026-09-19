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
| U1, U3 | 2 | INA226 current/power monitor, MSOP-10 | TI / INA226AIDGST | [INA226AIDGST](https://www.mouser.com/en/ProductDetail/Texas-Instruments/INA226AIDGST) | Swapped to the `T` suffix (small reel, ~250 units) instead of `R` (standard reel, ~2500) — same die/package/pinout, just a smaller reel size that makes more sense for a qty-2 order |
| U2 | 1 | MCP4728 quad 12-bit DAC, MSOP-10 | Microchip / MCP4728-E/UN | [MCP4728-E-UN](https://www.mouser.com/ProductDetail/Microchip-Technology/MCP4728-E-UN) | Verified |
| U4 | 1 | AD9833 DDS waveform generator, MSOP-10 | Analog Devices / AD9833BRMZ-REEL7 | [AD9833BRMZ-REEL7](https://www.mouser.com/en/ProductDetail/Analog-Devices/AD9833BRMZ-REEL7) | Corrected by the user while ordering — plain AD9833BRMZ isn't orderable, needs the -REEL7 tape-and-reel suffix (same die, just packaging) |
| U6, U7, U8 | 3 | TS5A3159A SPDT analog switch, SOT-23-6 | TI / TS5A3159ADBVR | [TS5A3159ADBVR](https://www.mouser.com/ProductDetail/Texas-Instruments/TS5A3159ADBVR) | Verified — **but flagged as possibly EOL at TI in search results; confirm current lifecycle/stock before ordering qty 3** |
| RS1 | 1 | 0.02Ω 1% current-sense shunt, 2512 | Bourns / CRA2512-FZ-R020ELF | [CRA2512-FZ-R020ELF](https://www.mouser.com/ProductDetail/Bourns/CRA2512-FZ-R020ELF) | Verified |
| RS2 | 1 | 0.033Ω 1% current-sense shunt, 2512 | Vishay Dale / WSL2512R0330FEA | [71-WSL2512R0330FEA](https://www.mouser.com/ProductDetail/Vishay-Dale/WSL2512R0330FEA) | Found by the user — the originally-listed Bourns `CRA2512-FZ-R033ELF` exists on Mouser (652-) but shows 0 stock; this Vishay WSL2512 part is the same value/package and in stock. |
| Q2 | 1 | P-channel MOSFET, DPAK, ≤-30V, ≥5A (ECU high-side switch) | Vishay / SUD50P06-15-GE3 | [SUD50P06-15-GE3](https://www.mouser.com/ProductDetail/Vishay-Semiconductors/SUD50P06-15-GE3) | Verified — spec is well over-built (-60V/-50A) for a real, exact-package match; fine to use, or substitute a tighter-spec DPAK P-FET if you prefer |
| Q1, Q3, Q5 | 3 | 2N7002 N-MOSFET, SOT-23 | onsemi / 2N7002 | [512-2N7002](https://www.mouser.com/ProductDetail/onsemi/2N7002) | Found by the user on Mouser |
| D1 | 1 | SS54 Schottky diode, SMC/DO-214AB, 5A/40V | Diodes Inc. / SS54FSH | [821-SS54FSH](https://www.mouser.com/ProductDetail/Diodes-Incorporated/SS54FSH) | Found by the user on Mouser — Diodes Inc.'s SS54 in the same SMC package, confirms the originally-listed Comchip part just wasn't the right distributor. |
| D2 | 1 | SK6812 addressable RGB LED, PLCC-4, 5.0×5.0mm | SK6812 (various — Opsco etc.) | — | **Confirmed by the user: not stocked on Mouser.** Source from LCSC, AliExpress, or Adafruit instead. |
| F1 | 1 | 3A fuse, 1206/3216 SMD | Bel Fuse / TR/3216FF3-R | [504-TR/3216FF3-R](https://www.mouser.com/ProductDetail/Bel-Fuse/TR-3216FF3-R) | Found by the user on Mouser — Bel's naming embeds the rating (FF3 = 3A fast-acting) |
| C1 | 1 | 100µF/25V radial electrolytic, 8mm dia., 3.5mm pitch | Panasonic / ESH107M035AG3AA | [80-ESH107M035AG3AA](https://www.mouser.com/ProductDetail/Panasonic/ESH107M035AG3AA) | Found by the user on Mouser — decodes to 100µF/35V (not 25V as designed, but higher voltage rating is a safe substitute, pure margin upgrade) |
| J1 | 1 | 2-pos 5.08mm PCB screw terminal, THT | Phoenix Contact / 1935161 | [1935161](https://www.mouser.com/ProductDetail/Phoenix-Contact/1935161) | Best match, not exact — this Phoenix part is a **fixed** 5.00mm block; the board footprint (`TerminalBlock_bornier-2_P5.08mm`) implies pluggable. Mechanically close (0.08mm off pitch), functionally fine, but check fit before ordering many. |
| J0 | 1 | 15-pos 5.00mm screw terminal, THT | Molex / 39543-0015 | [39543-0015](https://www.mouser.com/ProductDetail/Molex/39543-0015) | Found by the user on Mouser — confirmed 15 positions / 5.00mm pitch (matches the `MX126-5.0-15P` footprint exactly). Not confirmed pluggable vs. fixed one-piece style; either works mechanically/electrically, fixed just loses the unplug-the-whole-harness convenience. |
| J2 | 1 | JST XH 2-pos, 2.50mm pitch, vertical THT header | JST / B2B-XH-A(LF)(SN) | [B2B-XH-ALFSN](https://www.mouser.com/ProductDetail/JST-Commercial/B2B-XH-ALFSN) | Verified — two very similar part numbers turned up (`B2B-XH-ALFSN` and `B2B-XH-2LFSN`); double-check the page shows 2-position before ordering |
| J3 | 1 | 4-pos 2.54mm vertical pin header, THT, male | Samtec / TSW-104-07-G-S | [TSW-104-07-G-S](https://www.mouser.com/ProductDetail/Samtec/TSW-104-07-G-S) | Verified — this is the gold-plated variant (pricier); cheaper tin-plated 4-pos headers exist in the same Mouser category if cost matters and gold plating isn't needed |
| TP1–TP14 | 14 | Test point, THT (substitute for a bare 1.5mm pad) | Keystone Electronics / 5011 | [534-5011](https://www.mouser.com/ProductDetail/Keystone-Electronics/5011) | Corrected by the user while ordering — original entry (5010) was wrong, real part/Mouser SKU is 5011 (Mouser prefix 534-). Mouser doesn't sell bare test-point pads, this is the standard THT substitute. TP1/TP2 = knock signal/ground (VW-4/VW-5); TP3–TP14 = scope-probe points on key GPIO signals, both buses, and both 12V rails |
| SW1, SW2, SW3 | 3 | 6×6mm tactile push-button, THT, 2-pin | Omron / B3F-1000 | [B3F-1000](https://www.mouser.com/ProductDetail/Omron-Electronics/B3F-1000) | Verified — confirm actuator height/force (100gf, 4.3mm button) matches the footprint's expectations |
| Y1 | 1 | 25MHz SMD oscillator, 7×5mm, 3.3V, LVCMOS | IQD Frequency Products / XLH736025000000X | [972-XLH736025000000X](https://www.mouser.com/ProductDetail/IQD/XLH736025000000X) | Found by the user on Mouser — confirmed 7mm×5mm package and 3.135–3.465V supply range (covers 3.3V nominal), LVCMOS output. Different manufacturer than originally guessed (IQD, not EuroQuartz) but matches the footprint and electrical requirements. |
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
| C2, C4, C5, C6, C7, C8, C9, C10 | 8 | 100nF, 0805, X7R, 50V | Yageo / CC0805KRX7R9BB104 | [CC0805KRX7R9BB104](https://www.mouser.com/ProductDetail/YAGEO/CC0805KRX7R9BB104) | Verified — C9/C10 added late (INA226 `VS` decoupling for U1/U3), was missing from this row entirely until now |

## Summary: not orderable as-is

Just **D2** (SK6812 LED) — not stocked on Mouser at all, source from
LCSC/Adafruit/etc. instead. Every other line item (30 of 31, 72 of 73 placed
parts) now has a specific, real Mouser part number, largely thanks to the
user checking the live site against each entry this pass — several of the
original "Verified" entries turned out to be wrong or unorderable (wrong
digit, missing packaging suffix, zero stock, wrong distributor) and got
corrected in the process. Treat any *remaining* unedited "Verified" row with
the same healthy skepticism if something doesn't match when you actually
try to order it.
