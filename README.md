# STK500
STK500 (Arduino version) protocol support for Arduino.

Useful for flashing one Arduino (target) from another (host).

Originally from BootDrive, which is BaldWisdom's port of avrdude (https://github.com/osbock/Baldwisdom/tree/master/BootDrive).  I added modifications so this works from an ESP8266 host via the Arduino platform.

This should work to flash any target that is running Optiboot, but only Arduino Uno (JeeNode, 16MHz @ 3.3v) and Arduino Pro Mini (8MHz @ 3.3v) targets have been tested.

## Usage

See included example.  This example combines the ESP8266 "FSBrowser" and "BasicOTA" examples, as well as adds support for flashing .hex files by using an HTTP GET.  The example is only for an ESP8266 host.

## Issues

This library may not work on an Arduino host using SD cards- that still needs to be tested.
