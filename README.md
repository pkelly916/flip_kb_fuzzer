# Improved Flipper Keyboard fuzzer
N92's keyboard fuzzer has been modified to include a larger keyspace and start/stop functionality through a new Flood button. Key modifiers are now applied to strings randomly so that CTRL, ALT, SHIFT, ESC, codes are now included in fuzzing. 

# To Do List

This project is a work in progress. The following list tracks the current implementation of desired features. 

- [X] refactor for later modifications
- [X] add second button 
- [X] implement start/stop functionality on flood button
- [ ] adjust keyboard keyspace and add modifiers (CTRL, ALT, ESC, etc)
- [ ] write to a log on SD card

# N92's original readme
Turn your Flipper Zero into a USB keyboard fuzzer. Works with the [Xtreme Version](https://github.com/Flipper-XFW/Xtreme-Firmware). 
For building instructions please refer to the [official guide](https://github.com/Flipper-XFW/Xtreme-Firmware#build-it-yourself).

# Other notes

This software is a modified work originally done by Gabriel Cirlig called flipper usb keyboard. Hence the original BSD-2 LICENSE is attached. For modified works, THE MIT License applies. 

