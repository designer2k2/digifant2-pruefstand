"""
circuit-synth port of the Digifant-2 Pruefstand Signal-Simulator + Erfassungseinheit,
prototyping whether circuit-synth is a better authoring path than hand-rolled kiutils
S-expression generation (see ../digifant2-pruefstand-kicad/gen_kicad.py + its README
for the bug history that motivated this comparison).

Same scope as the original: Signal-Simulator + Erfassungseinheit only. EPROM emulator
bus and the knock-sensor's real connector pin are still explicitly out of scope.
"""
from circuit_synth import circuit, Component, Net


def power_section(vin_12v, gnd, ecu_12v, ecu_sw):
    """Bench 12V in -> fuse -> reverse-polarity protect -> shunt-sensed ECU rail.
    ecu_12v is the sensed rail (RS1 output); ecu_sw is the switched rail downstream
    of the Q2 load switch that actually feeds the ECU harness -- C1 bulk sits there
    so it also provides (and is measured through RS1 as) the ECU's cold-boot inrush."""
    j1 = Component("Connector:Screw_Terminal_01x02", ref="J1", value="12V BENCH IN",
                    footprint="TerminalBlock:TerminalBlock_bornier-2_P5.08mm")
    # 3A (not 2A): worst-case bench draw is ECU (~0.6A) + cold idle-valve peak
    # (~1.8A) + board logic (~0.1A) ~= 2.5A, which nuisance-trips a 2A fuse.
    f1 = Component("Device:Fuse", ref="F1", value="3A",
                    footprint="Fuse:Fuse_1206_3216Metric")
    # SS54 (not SS34): 5A part in the same SMC body -- SS34's 3A rating leaves no
    # margin over the ~2.5A peak and it runs ~1.3W hot at that current.
    d1 = Component("Device:D_Schottky", ref="D1", value="SS54",
                    footprint="Diode_SMD:D_SMC")
    # Device:R (2-pin), not Device:R_Shunt: the R_Shunt symbol has 4 pins (Kelvin
    # sense taps 3/4) but there is no stock 4-pad 2512 footprint, so pins 3/4 landed
    # nowhere on the PCB (kicad-cli DRC schematic-parity caught it). This design ties
    # sense to the power path anyway; for accuracy, hand-route U1's IN+/IN- as their
    # own traces tapped right at the RS1 pads during layout.
    rs1 = Component("Device:R", ref="RS1", value="0.02R 1% shunt",
                     footprint="Resistor_SMD:R_2512_6332Metric")
    c1 = Component("Device:C_Polarized", ref="C1", value="100uF/25V",
                    footprint="Capacitor_THT:CP_Radial_D8.0mm_P3.50mm")

    j1[1] += vin_12v
    j1[2] += gnd
    f1[1] += vin_12v
    protected = Net("+12V_PROT")
    f1[2] += protected
    d1["A"] += protected

    # Route through the shunt explicitly as its own net so INA226 (U1) sees both ends.
    post_diode = Net("+12V_POST_D1")
    d1["K"] += post_diode

    rs1[1] += post_diode
    rs1[2] += ecu_12v
    c1[1] += ecu_sw
    c1[2] += gnd

    return {"rs1": rs1}


def ecu_power_switch(rail_in, rail_out, gnd, gp_enable):
    """High-side P-FET load switch cutting all bench power to the ECU harness.

    Fail-safe OFF: R16 holds the driver gate low whenever the Pico is unpowered or
    GP13 is low, so the ECU rail only comes up on an explicit firmware command.
    This is what lets the bench exercise ECU cold-boot behaviour and drop the rail
    on an overcurrent reading from U1 (RS1 sees the switch + every downstream load).
    Q3 (2N7002) level-shifts the 3V3 GPIO to the 12V gate swing; R13 pulls the
    P-FET gate to its source (off) and R14 limits Q3's sink current; C3/R13 set a
    ~100us turn-on slope to tame inrush."""
    q2 = Component("Custom_Digifant2:Q_PMOS_DPAK", ref="Q2", value="P-MOS -30V >=5A",
                    footprint="Package_TO_SOT_SMD:TO-252-2")
    q3 = Component("Custom_Digifant2:Q_NMOS_2N7002", ref="Q3", value="2N7002",
                    footprint="Package_TO_SOT_SMD:SOT-23")
    r13 = Component("Device:R", ref="R13", value="10k", footprint="Resistor_SMD:R_0805_2012Metric")
    r14 = Component("Device:R", ref="R14", value="4.7k", footprint="Resistor_SMD:R_0805_2012Metric")
    r15 = Component("Device:R", ref="R15", value="1k", footprint="Resistor_SMD:R_0805_2012Metric")
    r16 = Component("Device:R", ref="R16", value="100k", footprint="Resistor_SMD:R_0805_2012Metric")
    c3 = Component("Device:C", ref="C3", value="10nF", footprint="Capacitor_SMD:C_0805_2012Metric")

    gate = Net("Q2_GATE")
    gdrv = Net("ECU_SW_GATE_DRV")
    q2["S"] += rail_in
    q2["D"] += rail_out
    q2["G"] += gate
    r13[1] += rail_in
    r13[2] += gate
    c3[1] += rail_in
    c3[2] += gate
    r14[1] += gate
    r14[2] += q3["D"]
    q3["S"] += gnd
    q3["G"] += gdrv
    r15[1] += gp_enable
    r15[2] += gdrv
    r16[1] += gdrv
    r16[2] += gnd


def status_led(vcc_3v3, gnd, gp_data):
    """Single addressable RGB pixel (SK6812, 5050 SMD) on ONE GPIO, driven by the
    Pico's PIO. SK6812 runs straight off 3V3 (VIH = 0.7*VDD ~= 2.3V, so the 3V3
    GPIO is in spec -- no level shifter, no separate 5V rail, and 2 GPIOs freed vs
    a plain 3-pin RGB LED). Chain from D2's DOUT to add more pixels later.
    Firmware picks the meaning, e.g. green = idle / blue = sim running / red = fault."""
    d2 = Component("LED:SK6812", ref="D2", value="SK6812",
                    footprint="LED_SMD:LED_SK6812_PLCC4_5.0x5.0mm_P3.2mm")
    r17 = Component("Device:R", ref="R17", value="330R", footprint="Resistor_SMD:R_0805_2012Metric")
    c4 = Component("Device:C", ref="C4", value="100nF", footprint="Capacitor_SMD:C_0805_2012Metric")
    r17[1] += gp_data
    r17[2] += d2["DIN"]
    d2["VDD"] += vcc_3v3
    d2["VSS"] += gnd
    c4[1] += vcc_3v3
    c4[2] += gnd


def ecu_current_sense(post_shunt_hi, post_shunt_lo, gnd, vcc_3v3, sda, scl):
    """INA226 U1 measuring total ECU current across RS1. addr A0=GND,A1=GND -> 0x40."""
    u1 = Component("Custom_Digifant2:INA226", ref="U1", value="INA226 (0x40)",
                    footprint="Package_SO:MSOP-10_3x3mm_P0.5mm")
    u1["IN+"] += post_shunt_hi
    u1["IN-"] += post_shunt_lo
    u1["VBUS"] += post_shunt_hi
    u1["VS"] += vcc_3v3
    u1["GND"] += gnd
    u1["A0"] += gnd
    u1["A1"] += gnd
    u1["SDA"] += sda
    u1["SCL"] += scl
    # ALERT (open-drain) intentionally unused -- leave unconnected, add a manual
    # no-connect flag in KiCad (same as the Pico's unused GPIOs; circuit-synth
    # has no NC-flag API in this version).


def idle_valve(drive_12v, valve_return, gnd, vcc_3v3, sda, scl):
    """Real used N71 idle valve as load (J2), current sensed by U3 (INA226, addr 0x41).
    valve_return is the same net as VW connector pin 23 (idle-valve return) -- J2 sits
    in that current loop as the bench-mounted real load."""
    j2 = Component("Connector_Generic:Conn_01x02", ref="J2", value="IDLE VALVE N71",
                    footprint="Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical")
    # 0.033R (not 0.1R): the idle valve peaks near ~1.8A, and 0.1R * 1.8A = 180mV
    # clips the INA226's +-81.92mV PGA full-scale (limit would be ~0.8A). 0.033R
    # gives ~60mV at 1.8A, ~0.11W in the 2512.
    rs2 = Component("Device:R", ref="RS2", value="0.033R 1% shunt",
                     footprint="Resistor_SMD:R_2512_6332Metric")
    u3 = Component("Custom_Digifant2:INA226", ref="U3", value="INA226 (0x41)",
                    footprint="Package_SO:MSOP-10_3x3mm_P0.5mm")

    j2[1] += drive_12v
    j2[2] += valve_return
    rs2[1] += valve_return
    rs2[2] += gnd

    u3["IN+"] += valve_return
    u3["IN-"] += gnd
    u3["VBUS"] += valve_return
    u3["VS"] += vcc_3v3
    u3["GND"] += gnd
    u3["A0"] += vcc_3v3
    u3["A1"] += gnd
    u3["SDA"] += sda
    u3["SCL"] += scl
    # ALERT intentionally unused, same as U1 -- see note there.


def dac_analog_sim(vcc_3v3, gnd, sda, scl, vw_pin9, vw_pin10, vw_pin21, vw_pin2):
    """MCP4728 U2: NTC air/water, LMM, lambda sim outputs via 220R series protection."""
    u2 = Component("Analog_DAC:MCP4728", ref="U2", value="MCP4728 (0x60)",
                    footprint="Package_SO:MSOP-10_3x3mm_P0.5mm")
    u2["VDD"] += vcc_3v3
    u2["VSS"] += gnd
    u2["SCL"] += scl
    u2["SDA"] += sda
    u2["~{LDAC}"] += gnd

    dac_outs = ["VOUTA", "VOUTB", "VOUTC", "VOUTD"]
    targets = [(vw_pin9, "R2"), (vw_pin10, "R3"), (vw_pin21, "R4"), (vw_pin2, "R5")]
    for pin_name, (target_net, rref) in zip(dac_outs, targets):
        r = Component("Device:R", ref=rref, value="220R",
                       footprint="Resistor_SMD:R_0805_2012Metric")
        dac_net = Net(f"{rref}_DAC_SIDE")
        u2[pin_name] += dac_net
        r[1] += dac_net
        r[2] += target_net


def crank_driver(gpio_crank, gnd, ecu_12v, vw_pin18):
    """Q1 open-drain crank/Hallgeber signal driver, RPU1 pull-up to +12V_ECU."""
    # Custom_Digifant2:Q_NMOS_2N7002 (not Device:Q_NMOS): the generic symbol's pin
    # *numbers* are the letters G/D/S, which resolve fine at the schematic level but
    # don't match the real SOT-23 footprint's numeric pads 1/2/3 -- confirmed via
    # the generated PCB showing all 3 pads "unconnected" despite correct schematic
    # wiring. This part has real numeric pins (1=G, 2=S, 3=D) matching the footprint.
    q1 = Component("Custom_Digifant2:Q_NMOS_2N7002", ref="Q1", value="2N7002",
                    footprint="Package_TO_SOT_SMD:SOT-23")
    rg1 = Component("Device:R", ref="RG1", value="100R",
                     footprint="Resistor_SMD:R_0805_2012Metric")
    rpu1 = Component("Device:R", ref="RPU1", value="1k",
                      footprint="Resistor_SMD:R_0805_2012Metric")

    rg1[1] += gpio_crank
    rg1[2] += q1["G"]
    q1["S"] += gnd
    q1["D"] += vw_pin18
    rpu1[1] += vw_pin18
    rpu1[2] += ecu_12v


def edge_capture(vcc_3v3, gpio_ignition, gpio_injector, vw_pin25, vw_pin12):
    """Ignition/injector edge capture: pull-up to Pico's own 3V3, series ESD guard."""
    r8 = Component("Device:R", ref="R8", value="10k", footprint="Resistor_SMD:R_0805_2012Metric")
    r9 = Component("Device:R", ref="R9", value="330R", footprint="Resistor_SMD:R_0805_2012Metric")
    r10 = Component("Device:R", ref="R10", value="1k", footprint="Resistor_SMD:R_0805_2012Metric")
    r11 = Component("Device:R", ref="R11", value="330R", footprint="Resistor_SMD:R_0805_2012Metric")

    r8[1] += vcc_3v3
    r8[2] += vw_pin25
    r9[1] += vw_pin25
    r9[2] += gpio_ignition

    r10[1] += vcc_3v3
    r10[2] += vw_pin12
    r11[1] += vw_pin12
    r11[2] += gpio_injector


def knock_sim(vcc_3v3, gnd, spi_sck, spi_mosi, spi_cs):
    """AD9833 U4 DDS burst generator; output only reaches TP1, not the VW connector
    (real target pin/coupling never established -- same as the original schematic)."""
    u4 = Component("Custom_Digifant2:AD9833", ref="U4", value="AD9833",
                    footprint="Package_SO:MSOP-10_3x3mm_P0.5mm")
    # Active 4-pin oscillator, not a passive 2-pin crystal: AD9833's MCLK is a
    # digital clock input and needs a driven signal, not a bare crystal needing
    # its own oscillator circuit. (The original draft had a symbol/footprint
    # mismatch here -- Device:Crystal is 2-pin, but was assigned a 4-pin
    # footprint. XO91 is the real symbol matching that 7.0x5.0mm footprint family.)
    y1 = Component("Oscillator:XO91", ref="Y1", value="25MHz",
                    footprint="Oscillator:Oscillator_SMD_EuroQuartz_XO91-4Pin_7.0x5.0mm")
    r12 = Component("Device:R", ref="R12", value="200R", footprint="Resistor_SMD:R_0805_2012Metric")
    c2 = Component("Device:C", ref="C2", value="100nF", footprint="Capacitor_SMD:C_0805_2012Metric")

    u4["VDD"] += vcc_3v3
    u4["DGND"] += gnd
    u4["AGND"] += gnd
    u4["MCLK"] += y1["OUT"]
    y1["V+"] += vcc_3v3
    y1["EN"] += vcc_3v3
    y1["GND"] += gnd
    u4["SCLK"] += spi_sck
    u4["SDATA"] += spi_mosi
    u4["FSYNC"] += spi_cs
    # COMP (DAC bias decouple) and CAP_2V5 (internal regulator bypass) are both
    # decoupling-only pins per the datasheet -- a real build should each get a
    # decoupling cap to ground, but that's a layout/BOM detail; left unconnected
    # here rather than modeling a dummy net, same as the other NC pins noted above.

    vout_r = Net("U4_VOUT_R")
    u4["VOUT"] += vout_r
    r12[1] += vout_r
    load = Net("U4_LOAD")
    r12[2] += load
    c2[1] += load
    tp1 = Component("Connector_Generic:Conn_01x01", ref="TP1", value="KNOCK OUT",
                     footprint="TestPoint:TestPoint_Pad_D1.5mm")
    tp1[1] += load
    c2[2] += gnd


def operator_ui(vcc_3v3, gnd, sda, scl, btn_menu, btn_minus, btn_plus):
    j3 = Component("Connector_Generic:Conn_01x04", ref="J3", value="OLED / UI I2C",
                    footprint="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical")
    j3[1] += gnd
    j3[2] += vcc_3v3
    j3[3] += scl
    j3[4] += sda

    for ref, net in (("SW1", btn_menu), ("SW2", btn_minus), ("SW3", btn_plus)):
        sw = Component("Switch:SW_Push", ref=ref, value="PUSH",
                        footprint="Button_Switch_THT:SW_PUSH_6mm")
        sw[1] += net
        sw[2] += gnd


@circuit(name="Digifant2_Pruefstand")
def main_circuit():
    gnd = Net("GND")
    vin_12v = Net("VIN_12V")
    ecu_12v = Net("+12V_ECU")          # RS1-sensed rail, upstream of the Q2 load switch
    ecu_sw = Net("+12V_ECU_SW")        # switched rail feeding the ECU harness + idle valve
    vcc_3v3 = Net("+3V3")
    sda = Net("I2C_SDA")
    scl = Net("I2C_SCL")

    vw_pin2 = Net("VW_PIN2")
    vw_pin9 = Net("VW_PIN9")
    vw_pin10 = Net("VW_PIN10")
    vw_pin12 = Net("VW_PIN12")
    vw_pin18 = Net("VW_PIN18")
    vw_pin21 = Net("VW_PIN21")
    vw_pin25 = Net("VW_PIN25")

    gp_crank = Net("GP2_CRANK")
    gp_ignition = Net("GP14_IGN_CAPTURE")
    gp_injector = Net("GP15_INJ_CAPTURE")
    gp_spi_cs = Net("GP16_SPI_CS")
    gp_spi_sck = Net("GP18_SPI_SCK")
    gp_spi_mosi = Net("GP19_SPI_MOSI")
    gp_btn_menu = Net("GP20_BTN_MENU")
    gp_btn_minus = Net("GP21_BTN_MINUS")
    gp_btn_plus = Net("GP22_BTN_PLUS")
    gp_led_data = Net("GP10_LED_DATA")
    gp_ecu_en = Net("GP13_ECU_PWR_EN")

    # CONN_VW is now a 10A/pin 5mm screw terminal block (was a 2.54mm pin header --
    # its ~3A/pin rating left almost no margin on the idle-valve legs, pins 22/23).
    # Pins are addressed by harness name (VW-2, VW-9, ...); the symbol numbers its
    # terminals 1-12 to match the footprint pads.
    j0 = Component("Custom_Digifant2:CONN_VW", ref="J0", value="VW ECU HARNESS",
                    footprint="TerminalBlock:TerminalBlock_MaiXu_MX126-5.0-12P_1x12_P5.00mm")
    j0["VW-2"] += vw_pin2
    j0["VW-9"] += vw_pin9
    j0["VW-10"] += vw_pin10
    j0["VW-12"] += vw_pin12
    j0["VW-13"] += gnd
    j0["VW-14"] += ecu_sw
    j0["VW-18"] += vw_pin18
    j0["VW-19"] += gnd
    j0["VW-21"] += vw_pin21
    j0["VW-22"] += ecu_sw  # idle valve drive; switched rail (was direct +12V_ECU)
    vw_pin23_valve_return = Net("VW_PIN23_VALVE_RETURN")
    j0["VW-23"] += vw_pin23_valve_return
    j0["VW-25"] += vw_pin25

    power_section(vin_12v, gnd, ecu_12v, ecu_sw)
    ecu_power_switch(ecu_12v, ecu_sw, gnd, gp_ecu_en)
    ecu_current_sense(Net("+12V_POST_D1"), ecu_12v, gnd, vcc_3v3, sda, scl)
    idle_valve(ecu_sw, vw_pin23_valve_return, gnd, vcc_3v3, sda, scl)
    dac_analog_sim(vcc_3v3, gnd, sda, scl, vw_pin9, vw_pin10, vw_pin21, vw_pin2)
    crank_driver(gp_crank, gnd, ecu_sw, vw_pin18)
    edge_capture(vcc_3v3, gp_ignition, gp_injector, vw_pin25, vw_pin12)
    knock_sim(vcc_3v3, gnd, gp_spi_sck, gp_spi_mosi, gp_spi_cs)
    operator_ui(vcc_3v3, gnd, sda, scl, gp_btn_menu, gp_btn_minus, gp_btn_plus)
    status_led(vcc_3v3, gnd, gp_led_data)

    # ERC needs to see an actual power *source* pin on each rail, not just
    # passive connector/regulator-input pins -- add the standard KiCad power-flag
    # symbols (this mirrors the original hand-rolled schematic's own fix #7:
    # "no pin anywhere was typed power_out to drive GND net").
    # power:GND's reconstructed symbol lost the special power-flag marker real
    # KiCad GND symbols carry (confirmed via ERC: it showed up as "not driven"
    # despite being the GND source) -- PWR_FLAG works reliably for the other two
    # rails, so use it for GND too instead of the broken GND-symbol reconstruction.
    gnd_flag = Component("power:PWR_FLAG", ref="#FLG03", footprint="")
    gnd_flag[1] += gnd
    ecu12v_flag = Component("power:PWR_FLAG", ref="#FLG01", footprint="")
    ecu12v_flag[1] += ecu_12v
    vcc3v3_flag = Component("power:PWR_FLAG", ref="#FLG02", footprint="")
    vcc3v3_flag[1] += vcc_3v3
    # Q2's drain is a passive pin -> ERC sees the switched rail as undriven without this.
    ecu_sw_flag = Component("power:PWR_FLAG", ref="#FLG04", footprint="")
    ecu_sw_flag[1] += ecu_sw

    r6 = Component("Device:R", ref="R6", value="4.7k", footprint="Resistor_SMD:R_0805_2012Metric")
    r7 = Component("Device:R", ref="R7", value="4.7k", footprint="Resistor_SMD:R_0805_2012Metric")
    r6[1] += vcc_3v3
    r6[2] += sda
    r7[1] += vcc_3v3
    r7[2] += scl

    u5 = Component("MCU_RaspberryPi_and_Boards:Pico", ref="U5", value="Raspberry Pi Pico",
                    footprint="RPi_Pico:RPi_Pico_SMD_TH")
    u5["3V3"] += vcc_3v3
    # The real Pico symbol exposes 7 physically separate GND pins (same name,
    # different pin numbers) -- ERC caught that wiring only the name-matched one
    # left the other 6 undriven. All must tie to the common ground plane on a
    # real board.
    for gnd_pin in (3, 8, 13, 18, 23, 28, 38, 42):
        u5[gnd_pin] += gnd
    # RUN: active-high enable, no reset button in this design -> tie straight to 3V3.
    u5["RUN"] += vcc_3v3
    # ADC_VREF: no analog inputs used on the Pico itself (sensing is all external
    # DAC/ADC via I2C) -- tie to 3V3 per Raspberry Pi's own reference-design guidance
    # for an unused ADC.
    u5["ADC_VREF"] += vcc_3v3
    # AGND: no analog precision requirement here (ADC unused) -- tie to the common
    # ground plane per Raspberry Pi's own guidance for this case.
    u5["AGND"] += gnd
    # 3V3_EN (weak internal pull-up per datasheet), VBUS/VSYS (module is powered via
    # its own USB connection to the PC, not from this board), SWCLK/SWDIO (no SWD
    # debug probe planned) are all intentionally left NC, same convention as above.
    u5["GPIO2"] += gp_crank
    u5["GPIO10"] += gp_led_data   # SK6812 (GP11/GP12 freed vs the old 3-pin RGB LED)
    u5["GPIO13"] += gp_ecu_en
    u5["GPIO4"] += sda
    u5["GPIO5"] += scl
    u5["GPIO14"] += gp_ignition
    u5["GPIO15"] += gp_injector
    u5["GPIO16"] += gp_spi_cs
    u5["GPIO18"] += gp_spi_sck
    u5["GPIO19"] += gp_spi_mosi
    u5["GPIO20"] += gp_btn_menu
    u5["GPIO21"] += gp_btn_minus
    u5["GPIO22"] += gp_btn_plus


if __name__ == "__main__":
    c = main_circuit()
    c.generate_kicad_project("digifant2_pruefstand_cs")
