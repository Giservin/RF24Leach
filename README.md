This LEACH prioritize energy to pick the next cluster head. The node that use the smallest energy will be picked to be a cluster head for the next round.

## Libraries
Install these libraries in your Arduino library folder

* https://github.com/nRF24/RF24
* https://github.com/nRF24/RF24Network
* https://github.com/adafruit/Adafruit_BusIO
* https://github.com/adafruit/Adafruit_NeoPixel
* https://github.com/adafruit/Adafruit-GFX-Library
* https://github.com/adafruit/Adafruit_SSD1306
* https://github.com/adafruit/Adafruit_INA219
* http://www.rinkydinkelectronics.com/library.php?id=73

```
Note for RF24Network Library: NEW - Nodes can now be slept while the radio is not actively transmitting. This must be manually enabled by uncommenting the #define ENABLE_SLEEP_MODE in RF24Network_config.h
```
