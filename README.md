# Air Quality Data Logger for Splunk

This is the arduino-based firmware for an ESP8266-based device which will monitor air quality by means of a PMS7003 Air Quality Sensor and upload the resulting data to a Splunk HTTP Event Collector.  

## Configuration

On first flash, the device will create its own wifi access point "WifiManager" and wait to be configured.  Simply connect to the access point and open the [Configuration Page](http://192.168.4.1) to configure the device.  All fields are required.  These settings are stored in flash memory and will persist across power cycles.

Settings are as follows:
* Splunk Server: FQDN of the server on which Splunk is running
* Splunk Port:  TCP port on which the HEC is listening
* Token: HEC token
* Index: the Splunk index that will receive this data
* Sourcetype: the sourcetype that shall accompany this data

If a new configuration is required, reboot the device (either with the reboot button on the ESP8266 or via a power cycle), then hold the flash button down for three seconds.  Do not hold the flash button down while rebooting, as this puts the ESP8266 into another mode. 

## Debugging

The device will also log debug information to a serial port on the ESP8266's USB port at 115200,8,N,1.  Once the device is booted, this consists of a single line of data containing the current AQI, PM2.5, and PM10 readings once per second.

## Screen

The TFT_eSPI library is required to compile the firmware, but the device will work without one connected.  The code assumes a 128x128 screen, but should be easy enough to modify if desired.

## Dependencies

This firmware depends on the following libraries:
* TFT_eSPI
* ArduinoJson
* WifiManager

## Wiring

Power the PMSx003 with 5V.  Connect only the TX and RX pins to D3 and D4, respectively.

If you wish to connect an LCD screen, set up your User_Setup.h file according to the screen you are using.  This was tested with a ST7735 green tab 3 screen.