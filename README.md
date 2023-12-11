# ESP8266 Antenna AntennaRotator

No circuit diagram right now, but this controls some relays in an antenna rotator box (I'm using the Yaesu G450A) and it reads the rheostat to figure out the position.

At the moment it just listens on the web for manual moves or a bearing, or it'll listen on UDP port 12060 for RUMLogNG's rotator commands

## TODO:
* Calibration on the web rather than hard coded
* Make it look nicer, e.g. an amizuthal map that can be uploaded
* Support more than just RUMLogNG