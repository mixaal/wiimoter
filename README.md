# Wii Remote - Control Your Desktop

## Install 
```
sudo apt install xwiimote bluez blueman xserver-xorg-input-xwiimote wminput libxwiimote-dev
sudo modprobe hid-wiimote uinput
```

## Pair Wii Remote

* Run blueman
* Press scan for devices
* Press sync on wii remote (do not press 1+2 - it would ask you for PIN) 
* Finish the configuration
* Wii remote should appear as a HID in your system
* run `xwiishow` to verify the controller functionality

*NOTE*: bluez should be of 5.x version.

## Build

```
make clean; make
```

## Run

```
sudo ./wiiremote 1 /dev/input/event6
```
