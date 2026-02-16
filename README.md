# SonosRemote

This is a very experimental project I am working on to build a custom hardware remote for my Sonos speakers. It is currently sitting on a breadboard and is definitely in the early prototype phase.

## Current State
* **Very Experimental:** Things might break, or crash, or just not work some days.
* **Breadboard Setup:** I have it wired up on a breadboard right now.
* **Batch Loading:** I recently added some logic to make discovery and UI updates feel a bit snappier by processing them in batches.
* **Dynamic UI:** The layout automatically adjusts for album art and tries to wrap long song titles so they actually fit on the screen.
brooo this is the most random list every but suuuuuuuuree.

## Parts I used
* **Microcontroller:** [XIAO ESP32S3 3PCS Pack- 2.4GHz WiFi, BLE 5.0, 8MB PSRAM, 8MB Flash, Dual-core, Battery Charge Supported, Power Efficiency and Rich Interface, Ideal for Smart Homes, IoT, Wearable Devices, Robotics.](https://www.amazon.com/dp/B0DJ6NQFKX)
* **I/O Expander:** [MCP23017 Serial Interface Module IIC I2C SPI Bidirectional 16-Bit I/O Expander Pins 10Mhz Serial Interface Module](https://www.amazon.com/gp/product/B0DSLPRKKZ)
* **Display:** [1.69 Inch 1.69" Color TFT Display Module HD IPS LCD LED Screen 240X280 SPI Interface ST7789 Controller for Arduino](https://www.amazon.com/dp/B0DQ4R95TZ) (This is a very crappy display that was like the cheapest one i could find, might be made larger later).
* A messy pile of jumper wires.

## How it works (mostly)
It uses SSDP to find Sonos players on the local network. Once it finds them, it talks to them over SOAP/UPnP to get track info, album art, and handle basic playback control.

More details about the full project (enclosure, custom PCB, etc.) will come later as it evolves. For now, it is just a fun experiment in making a dedicated physical controller for music. I would LOVE to make a custom pcb and a nice enclosure for this, but I want to get the software working well first before I invest in that. I also suck at custom pcb design and enclosure design, which will make that difficult (but learning is the point!).
