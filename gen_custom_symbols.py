"""
Minimal custom KiCad symbol generator for two chips missing from the mainline
KiCad symbol libraries: TI INA226 (current/power monitor) and ADI AD9833 (DDS).

Pinouts verified 2026-09-04 against the primary datasheets, pin-by-pin:
- INA226: TI SBOS547C (Rev. C, Aug 2026), Table 4-1 "Pin Functions", page 3.
- AD9833: Analog Devices Rev. G, Table 4 "Pin Function Descriptions", page 6.
Both match exactly what's encoded below -- no corrections needed.
"""
from pathlib import Path

PIN_TEMPLATE = '''\t\t\t(pin {etype} line
\t\t\t\t(at {x} {y} {angle})
\t\t\t\t(length 2.54)
\t\t\t\t(name "{name}"
\t\t\t\t\t(effects (font (size 1.27 1.27)))
\t\t\t\t)
\t\t\t\t(number "{num}"
\t\t\t\t\t(effects (font (size 1.27 1.27)))
\t\t\t\t)
\t\t\t)
'''

def make_symbol(libname, ref, footprint, datasheet, description, keywords,
                 left_pins, right_pins, box_half_height=None):
    # left_pins / right_pins: list of (number, name, etype), top-to-bottom
    n = max(len(left_pins), len(right_pins))
    half_h = box_half_height or (n * 1.27 + 1.27)
    width = 15.24

    pins_sexp = ""
    for i, (num, name, etype) in enumerate(left_pins):
        y = half_h - 2.54 - i * 2.54
        pins_sexp += PIN_TEMPLATE.format(etype=etype, x=-(width + 2.54), y=y, angle=0, name=name, num=num)
    for i, (num, name, etype) in enumerate(right_pins):
        y = half_h - 2.54 - i * 2.54
        pins_sexp += PIN_TEMPLATE.format(etype=etype, x=(width + 2.54), y=y, angle=180, name=name, num=num)

    return f'''\t(symbol "{libname}"
\t\t(pin_names (offset 1.016))
\t\t(exclude_from_sim no)
\t\t(in_bom yes)
\t\t(on_board yes)
\t\t(property "Reference" "{ref}"
\t\t\t(at {-width} {half_h + 1.27} 0)
\t\t\t(effects (font (size 1.27 1.27)) (justify left))
\t\t)
\t\t(property "Value" "{libname}"
\t\t\t(at {width} {half_h + 1.27} 0)
\t\t\t(effects (font (size 1.27 1.27)) (justify right))
\t\t)
\t\t(property "Footprint" "{footprint}"
\t\t\t(at 0 {-(half_h + 2.54)} 0)
\t\t\t(effects (font (size 1.27 1.27)) (hide yes))
\t\t)
\t\t(property "Datasheet" "{datasheet}"
\t\t\t(at 0 0 0)
\t\t\t(effects (font (size 1.27 1.27)) (hide yes))
\t\t)
\t\t(property "Description" "{description}"
\t\t\t(at 0 0 0)
\t\t\t(effects (font (size 1.27 1.27)) (hide yes))
\t\t)
\t\t(property "ki_keywords" "{keywords}"
\t\t\t(at 0 0 0)
\t\t\t(effects (font (size 1.27 1.27)) (hide yes))
\t\t)
\t\t(symbol "{libname}_0_1"
\t\t\t(rectangle
\t\t\t\t(start {-width} {half_h})
\t\t\t\t(end {width} {-half_h})
\t\t\t\t(stroke (width 0.254) (type default))
\t\t\t\t(fill (type background))
\t\t\t)
\t\t)
\t\t(symbol "{libname}_1_1"
{pins_sexp}\t\t)
\t)
'''

ina226 = make_symbol(
    libname="INA226",
    ref="U",
    footprint="Package_SO:MSOP-10_3x3mm_P0.5mm",
    datasheet="https://www.ti.com/lit/ds/symlink/ina226.pdf",
    description="36V, 16-bit, I2C current/voltage/power monitor with alert (custom symbol, pinout verified against TI SBOS547C)",
    keywords="current sensor power monitor i2c shunt",
    left_pins=[
        ("10", "IN+", "input"),
        ("9", "IN-", "input"),
        ("8", "VBUS", "input"),
        ("7", "GND", "power_in"),
        ("6", "VS", "power_in"),
    ],
    right_pins=[
        ("1", "A1", "input"),
        ("2", "A0", "input"),
        ("3", "ALERT", "open_collector"),
        ("4", "SDA", "bidirectional"),
        ("5", "SCL", "input"),
    ],
)

ad9833 = make_symbol(
    libname="AD9833",
    ref="U",
    footprint="Package_SO:MSOP-10_3x3mm_P0.5mm",
    datasheet="https://www.analog.com/media/en/technical-documentation/data-sheets/ad9833.pdf",
    description="Low-power programmable waveform generator / DDS, SPI (custom symbol, pinout verified against ADI Rev. G datasheet)",
    keywords="dds waveform generator spi signal synthesizer",
    left_pins=[
        ("2", "VDD", "power_in"),
        ("4", "DGND", "power_in"),
        ("9", "AGND", "power_in"),
        ("1", "COMP", "passive"),
        ("3", "CAP_2V5", "passive"),
    ],
    right_pins=[
        ("5", "MCLK", "input"),
        ("6", "SDATA", "input"),
        ("7", "SCLK", "input"),
        ("8", "FSYNC", "input"),
        ("10", "VOUT", "output"),
    ],
)

# Device:Q_NMOS's pin *numbers* are the letters "G"/"D"/"S", not numeric -- fine at
# the schematic level (pin lookup by name works), but breaks PCB pad matching since
# the real SOT-23 footprint's pads are numbered 1/2/3. Confirmed via generated PCB:
# all 3 Q1 pads showed "unconnected" despite being correctly wired in the schematic.
# Fix: a minimal 2N7002-specific symbol with real numeric pins matching the SOT-23
# footprint, per the 2N7002 datasheet SOT-23 pinout (1=Gate, 2=Source, 3=Drain).
q_2n7002 = make_symbol(
    libname="Q_NMOS_2N7002",
    ref="Q",
    footprint="Package_TO_SOT_SMD:SOT-23",
    datasheet="https://assets.nexperia.com/documents/data-sheet/2N7002.pdf",
    description="N-channel MOSFET, SOT-23, numeric pins matching the real footprint (1=G, 2=S, 3=D)",
    keywords="mosfet nmos switch",
    left_pins=[("1", "G", "input")],
    right_pins=[("2", "S", "passive"), ("3", "D", "passive")],
)

# P-channel MOSFET for the ECU-rail high-side load switch. Same reason as the
# 2N7002 above for a part-specific symbol: the generic Device:Q_PMOS uses letter
# pin numbers that don't match the DPAK footprint's numeric pads. TO-252-2 pads:
# 1 = gate, 2 = drain (tab), 3 = source.
q_pmos_dpak = make_symbol(
    libname="Q_PMOS_DPAK",
    ref="Q",
    footprint="Package_TO_SOT_SMD:TO-252-2",
    datasheet="",
    description="P-channel MOSFET, DPAK/TO-252, numeric pins matching the footprint (1=G, 2=D, 3=S)",
    keywords="mosfet pmos load switch dpak",
    left_pins=[("1", "G", "input")],
    right_pins=[("2", "D", "passive"), ("3", "S", "passive")],
)

# 15-position terminal block for the VW Digifant II 25-pin ECU harness -- only the
# pins the bench actually uses are broken out. Pin *numbers* MUST be sequential
# 1..15 to match the footprint pads; the real harness pin number is kept as the pin
# *name* (VW-2, VW-6, ...) so the schematic and silkscreen document which ECU wire
# lands on each terminal. Access in the netlist by name.
#   2 O2 sense | 6/11 throttle idle+WOT switches | 9 intake-air NTC | 10 coolant NTC
#   12 injector (capture) | 13 gnd | 14 +12V | 17 airflow-pot ref (ADC sense)
#   18 hall-sender (see DESIGN.md: pin 8 vs 18 unresolved) | 19 sensor gnd
#   21 airflow-pot wiper | 22/23 idle valve | 25 ignition (capture)
vw_harness_pins = [2, 6, 9, 10, 11, 12, 13, 14, 17, 18, 19, 21, 22, 23, 25]
vw_pin_defs = [(str(i + 1), f"VW-{p}", "passive") for i, p in enumerate(vw_harness_pins)]
conn_vw = make_symbol(
    libname="CONN_VW",
    ref="J",
    footprint="TerminalBlock:TerminalBlock_MaiXu_MX126-5.0-15P_1x15_P5.00mm",
    datasheet="",
    description="VW Digifant II ECU harness (25-pin) broken out to the 15 pins the bench uses, on a 10A/pin 5mm screw terminal block",
    keywords="vw connector ecu harness terminal block digifant",
    left_pins=vw_pin_defs[:8],
    right_pins=vw_pin_defs[8:],
)

header = '(kicad_symbol_lib\n\t(version 20241209)\n\t(generator "custom_digifant2_prototype")\n\t(generator_version "9.0")\n'
footer = ')\n'

out = Path(__file__).parent / "kicad-symbols" / "Custom_Digifant2.kicad_sym"
out.write_text(header + ina226 + ad9833 + q_2n7002 + q_pmos_dpak + conn_vw + footer)
print(f"Wrote {out} ({out.stat().st_size} bytes)")
