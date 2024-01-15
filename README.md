# ESPHost-EMAC

Arduino library. Mbed Core EMAC over [ESPHost library](https://github.com/JAndrassy/ESPHost).

Only WiFi STA mode for now. No SoftAP yet.

Works with Nano RP2040 Connect and can work with Nano 33 BLE and Raspberry PI Pico with esp32 wired on SPI.

## The Mbed Core WiFi library

Copy the WiFi library from Mbed Core for Giga R1.

In WiFi.h remove the `#if` around `#include "WhdSoftAPInterface.h"`.

In WiFi.cpp add this line at the end of the file.
 
```
arduino::WiFiClass WiFi(WiFiInterface::get_default_instance());
```
