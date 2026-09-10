#!/usr/bin/env bash
# Fetch the stock KiCad 9.0.0 symbol/footprint libraries this project references.
# The custom + community libs (Custom_Digifant2, MCU_RaspberryPi_and_Boards,
# RPi_Pico.pretty) are committed; everything else is pulled from KiCad's GitLab.
set -euo pipefail
cd "$(dirname "$0")"

SYM_API="https://gitlab.com/api/v4/projects/kicad%2Flibraries%2Fkicad-symbols/repository/files"
FP_API="https://gitlab.com/api/v4/projects/kicad%2Flibraries%2Fkicad-footprints/repository/files"
REF=9.0.0

SYMS=(Amplifier_Instrumentation Analog_Switch Analog_DAC Connector Connector_Generic Device
      Diode LED MCU_Module Mechanical Oscillator RF_Module Sensor_Current Switch power)
FPS=(Button_Switch_THT Capacitor_SMD Capacitor_THT Connector_JST
     Connector_PinHeader_2.54mm Diode_SMD Fuse LED_SMD LED_THT MountingHole
     Oscillator Package_SO Package_TO_SOT_SMD Resistor_SMD TerminalBlock TestPoint)
# individual footprint files (KiCad's .pretty dirs are huge; grab only what's used)
FP_MODS=(
  "Button_Switch_THT.pretty/SW_PUSH_6mm"
  "Capacitor_SMD.pretty/C_0805_2012Metric"
  "Capacitor_THT.pretty/CP_Radial_D8.0mm_P3.50mm"
  "Connector_JST.pretty/JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical"
  "Connector_PinHeader_2.54mm.pretty/PinHeader_1x04_P2.54mm_Vertical"
  "Diode_SMD.pretty/D_SMC"
  "Fuse.pretty/Fuse_1206_3216Metric"
  "LED_SMD.pretty/LED_SK6812_PLCC4_5.0x5.0mm_P3.2mm"
  "LED_THT.pretty/LED_D5.0mm-4_RGB"
  "MountingHole.pretty/MountingHole_3.2mm_M3"
  "Oscillator.pretty/Oscillator_SMD_EuroQuartz_XO91-4Pin_7.0x5.0mm"
  "Package_SO.pretty/MSOP-10_3x3mm_P0.5mm"
  "Package_TO_SOT_SMD.pretty/SOT-23"
  "Package_TO_SOT_SMD.pretty/SOT-23-6"
  "Package_TO_SOT_SMD.pretty/TO-252-2"
  "Resistor_SMD.pretty/R_0805_2012Metric"
  "Resistor_SMD.pretty/R_2512_6332Metric"
  "TerminalBlock.pretty/TerminalBlock_MaiXu_MX126-5.0-15P_1x15_P5.00mm"
  "TerminalBlock.pretty/TerminalBlock_bornier-2_P5.08mm"
  "TestPoint.pretty/TestPoint_Pad_D1.5mm"
)

mkdir -p kicad-symbols kicad-footprints
for s in "${SYMS[@]}"; do
  echo "sym $s"
  curl -sf "$SYM_API/${s}.kicad_sym/raw?ref=$REF" -o "kicad-symbols/${s}.kicad_sym"
done
for m in "${FP_MODS[@]}"; do
  dir="kicad-footprints/${m%/*}"; mkdir -p "$dir"
  enc=$(python3 -c "import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1],safe=''))" "$m.kicad_mod")
  echo "fp  $m"
  curl -sf "$FP_API/$enc/raw?ref=$REF" -o "$dir/${m##*/}.kicad_mod"
done
echo "done. committed libs (Custom_Digifant2, MCU_RaspberryPi_and_Boards, RPi_Pico.pretty) stay as-is."
