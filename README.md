# ESP8622 and ESP32 based ATEM Mini Tally display

A very simple Tally display for ATEM switchers using the video hub routing protocol.

Works on both ESP8622 and ESP32 boards. ESP8622 mode is optimized for the
very cheap and basic ESP8266 ESP-01 module.

## Setup

Create a copy of setup.h.sample to setup.h and adjust for you network and inputs.

## LED Tally modes

* Off - This input is not used
* Blinking - Preview on input
* On - Program on input / Direct output

## HTTP interface

A simple http interface is available for changing input id and seting a route.
