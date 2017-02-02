# Introduction
This code is a [fork](http://git.spritesserver.nl/espeink.git/) from [Sprite_tm's ESPeink project](https://spritesmods.com/?art=einkdisplay). I only made minor changes and added a script to be able to compile it against up to date [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk).

# How to install the tools
1. Clone the project `git clone https://github.com/Oliv4945/eink.git`
2. Go to the new directory `cd eink`
3. Run the setup, it will donwload and parameter all tools `./setup.sh`
4. Wait... from 30 to 60 min  

Then the firmware should be compiled (in `firmware/`), and the statics file generated in `webpages.espfs`.

# Upload
Edit the [Makefile](https://github.com/Oliv4945/eink/blob/master/Makefile#L31) according to your board.
* `None` - No specific reset procedure, done by yourself
* `ck`
* `wifio`
* `nodemcu` - NodeMCU and Wemos Mini D1 boards  

Then `make flash`, and `make htmlflash`

# TODO
The current code use an old SDK (1.0.1), I converted it to the last one (2.0.0) but the code need some cleanup before pushing it.

# Original README from Sprite_tm
This code uses esphttpd, which needs Heatshrink. To get the code for Heatshrink, do:
git submodule init
git submodule update

This code is licensed under the Beer-Ware license, as indicated in the sources.
