#pragma once

// GPIO map, see DESIGN.md "GPIO map".

// Q1 is an open-drain pull-down: GP2 high pulls VW-18 low (inverted).
#define PIN_CRANK           2

#define PIN_I2C_SDA         4
#define PIN_I2C_SCL         5
#define I2C_PORT            i2c0

#define PIN_IDLE_SW         6   // high = idle switch closed (VW-11 to GND)

// TS5A3159A control: high = sensor connected, low = open circuit.
#define PIN_SENSOR_AIR      8
#define PIN_SENSOR_WATER    9
#define PIN_SENSOR_LAMBDA   11

#define PIN_STATUS_LED      10  // SK6812 data, PIO

#define PIN_ECU_POWER       13  // high = ECU powered (Q2 on)

// ECU outputs are low-side switches pulled up to 3V3: active pulses are LOW.
#define PIN_IGN_CAPTURE     14
#define PIN_INJ_CAPTURE     15

// AD9833 on SPI0. GP16 is SPI0 RX in hardware, so FSYNC is driven as a plain GPIO.
#define PIN_KNOCK_FSYNC     16
#define PIN_KNOCK_SCK       18
#define PIN_KNOCK_SDATA     19
#define KNOCK_SPI_PORT      spi0

#define PIN_BTN_MENU        20
#define PIN_BTN_MINUS       21
#define PIN_BTN_PLUS        22

#define PIN_AFM_REF_ADC     26
#define AFM_REF_ADC_INPUT   0
// R20 20k / R21 10k divider: VW-17 = V_adc * 3.
#define AFM_REF_DIVIDER     3

#define I2C_ADDR_INA_ECU    0x40
#define I2C_ADDR_INA_VALVE  0x41
#define I2C_ADDR_DAC        0x60
#define I2C_ADDR_OLED       0x3C
