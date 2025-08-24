Place holder

## Sensors ##

### Environmental Sensor Summary

This table provides a high-level comparison of the sensors discussed, highlighting their primary function and measurement technology.

| Sensor Model      | Primary Measurement(s)            | Measurement Principle       | Key Feature / Distinction                                       |
| :---------------- | :-------------------------------- | :-------------------------- | :-------------------------------------------------------------- |
| **PMS5003S** | Particulate Matter (PM1.0, 2.5, 10) | Laser Scattering            | Measures physical particle/dust concentration in the air.       |
| **BME680** | VOCs, Pressure, Temp, Humidity    | Metal-Oxide (MOx) Gas Sensor | All-in-one; provides a raw gas resistance value for IAQ calculation. |
| **ENS160 + AHT21**| eCO₂, TVOC, Temp, Humidity        | Metal-Oxide (MOx) Gas Sensor | Provides a calculated **equivalent CO₂** (eCO₂) value.          |
| **SCD40** | Carbon Dioxide (CO₂), Temp, Humidity | NDIR (Photoacoustic)        | Provides a **direct CO₂ measurement**; baseline model.            |
| **SCD41** | Carbon Dioxide (CO₂), Temp, Humidity | NDIR (Photoacoustic)        | Direct CO₂ measurement with **higher accuracy** and a wider range than the SCD40. |

### Sensor Interface & Pinout Summary

This table details the communication protocols and essential pins for the sensors discussed. Note that pin names and operating voltages can vary slightly depending on the specific breakout board manufacturer.

| Sensor           | Interface(s) | Key Pins                                                                         | Typical Voltage* |
| :--------------- | :----------- | :------------------------------------------------------------------------------- | :--------------- |
| **PMS5003S** | UART         | `VCC`, `GND`, `TX`, `RX`, `SET`, `RESET`                                         | 5V               |
| **BME680** | I²C & SPI    | **I²C:** `VIN`, `GND`, `SCL`, `SDA`<br>**SPI:** `VIN`, `GND`, `SCK`, `SDI`, `SDO`, `CS` | 3.3V - 5V        |
| **ENS160 + AHT21** | I²C          | `VIN`, `GND`, `SCL`, `SDA`                                                       | 3.3V - 5V        |
| **SCD40 / SCD41** | I²C          | `VDD`, `GND`, `SCL`, `SDA`                                                       | 3.3V - 5V        |

***

*\*Typical Voltage often refers to the input voltage for common breakout boards, which may have onboard voltage regulators and logic level shifters. Always check the datasheet for your specific sensor or board.*
### PMS5003S
![PXL_20250824_142052766](https://github.com/user-attachments/assets/dd3514fe-fe7d-481a-b04c-3de6db1dee9a)

The PMS5003S is a laser-based particulate matter sensor. Its primary function is to measure the concentration and count of suspended particles of various sizes in the air.

| Measurement                 | Range                             | Accuracy / Consistency* | Resolution      |
| :-------------------------- | :-------------------------------- | :---------------------------------------------------------- | :-------------- |
| **PM1.0 Concentration** | 0 - 500 µg/m³                     | ±10 µg/m³ (for 0-100 µg/m³)<br>±10% reading (for 100-500 µg/m³) | 1 µg/m³         |
| **PM2.5 Concentration** | 0 - 500 µg/m³                     | ±10 µg/m³ (for 0-100 µg/m³)<br>±10% reading (for 100-500 µg/m³) | 1 µg/m³         |
| **PM10 Concentration** | 0 - 500 µg/m³                     | ±10 µg/m³ (for 0-100 µg/m³)<br>±10% reading (for 100-500 µg/m³) | 1 µg/m³         |
| **Particle Count (>0.3µm)** | > 0 particles/0.1L                | Not Specified                                               | 1 particle/0.1L |
| **Particle Count (>0.5µm)** | > 0 particles/0.1L                | Not Specified                                               | 1 particle/0.1L |
| **Particle Count (>1.0µm)** | > 0 particles/0.1L                | Not Specified                                               | 1 particle/0.1L |
| **Particle Count (>2.5µm)** | > 0 particles/0.1L                | Not Specified                                               | 1 particle/0.1L |
| **Particle Count (>5.0µm)** | > 0 particles/0.1L                | Not Specified                                               | 1 particle/0.1L |
| **Particle Count (>10µm)** | > 0 particles/0.1L                | Not Specified                                               | 1 particle/0.1L |

### BME680
![PXL_20250824_142110190](https://github.com/user-attachments/assets/2314a77e-1887-47a7-8e13-b57c832af603)

| Measurement | Range | Typical Absolute Accuracy | Resolution |
| :--- | :--- | :--- | :--- |
| **Temperature** | -40 to 85 °C | ±0.5 °C | 0.01 °C |
| **Humidity** | 0 to 100 %RH | ±3 %RH | 0.008 %RH |
| **Pressure** | 300 to 1100 hPa | ±0.12 hPa | 0.18 Pa |
| **Gas/VOCs** | N/A | Variable* | 0.083 to 3.2 kΩ (depending on measurement) |

*Note on Gas Measurement:* The gas sensor measures the change in resistance of a metal-oxide layer, which correlates to the concentration of Volatile Organic Compounds (VOCs) and other gases. The output is a raw resistance value in Ohms. To get a meaningful air quality index (IAQ), this value must be processed using the **Bosch Sensortec Environmental Cluster (BSEC) software library**. Therefore, it does not have a simple, single accuracy metric like the other sensors.

** Note: The interface is selected by the CSB pin: tie CSB to VDDIO and it’s in I²C mode; pull CSB low and it latches into SPI until the next power-on reset. 

In I²C mode the address is 0x76 (SDO=GND) or 0x77 (SDO=VDDIO); don’t leave SDO floating.

### ENS160 AHT21
![PXL_20250824_142125255 MP](https://github.com/user-attachments/assets/f89e3c8f-25b1-43af-bb54-f8081fa54aad)

| Sensor | Measurement | Range | Typical Accuracy | Resolution |
| :--- | :--- | :--- | :--- | :--- |
| **ENS160** | **Equivalent CO₂ (eCO₂)** | 400 - 65000 ppm | N/A* | 1 ppm |
|            | **Total VOCs (TVOC)** | 0 - 65000 ppb | N/A* | 1 ppb |
| **AHT21** | **Temperature** | -40 to 85 °C | ±0.3 °C | 0.01 °C |
|            | **Relative Humidity** | 0 to 100 %RH | ±2 %RH | 0.024 %RH |

###SCD40 / SCD41
![PXL_20250824_142122595 MP](https://github.com/user-attachments/assets/33b3c144-8969-4604-acb5-75a595558ca0)

This table outlines the key performance differences between the Sensirion SCD40 and SCD41 CO₂ sensors. While they share the same NDIR technology, the SCD41 offers a wider specified measurement range and higher accuracy.

| Specification             | SCD40                         | SCD41                         |
| :------------------------ | :---------------------------- | :---------------------------- |
| **CO₂ Specified Range** | 400 - 2000 ppm                | 400 - 5000 ppm                |
| **CO₂ Accuracy** | **±(50 ppm + 5% of reading)** | **±(40 ppm + 5% of reading)** |
| **Temperature Accuracy** | ±0.8 °C                       | ±0.8 °C                       |
| **Humidity Accuracy** | ±6 %RH                        | ±6 %RH                        |

***

#### Key Differences
* **Range**: The **SCD41** is specified for a wider operational range, making it more suitable for environments where CO₂ levels might exceed 2000 ppm.
* **Accuracy**: The **SCD41** is the more accurate sensor, with a lower base error in its CO₂ measurement (±40 ppm vs. ±50 ppm).
* 
## Display ##

### 2.4 TFT 240*320, ST7789
![PXL_20250824_142115152](https://github.com/user-attachments/assets/6474979e-0ac7-46bd-aea0-94cc6ba3e594)

This table outlines the specifications for a typical 2.4-inch TFT display module that uses the ST7789 controller, a common driver known for its high-resolution capabilities on small screens.

| Display Type | Controller | Resolution | Interface(s) | Key Pins                                 | Typical Voltage* |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **2.4" TFT** | **ST7789** | 240x320 pixels | SPI | `VCC`, `GND`, `SCK`, `MOSI`, `CS`, `RST`, `DC` | 3.3V - 5V        |

***

*\*Note: The ST7789 controller operates at 3.3V, but most breakout boards include a voltage regulator, allowing them to be powered by a 5V supply from popular microcontrollers like the Arduino UNO. The backlight (`LEDA` or `BL`) may also have a separate pin and require its own resistor or power source depending on the board's design.*

## Microcontroller ##
![PXL_20250824_142132725](https://github.com/user-attachments/assets/951713b7-07e2-40e1-a76e-562393f2d2a4)
ESP32-WROVER 2MB, I'm using the older version to make sure it'll fit on any of them.
