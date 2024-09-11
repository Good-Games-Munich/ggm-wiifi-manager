# ggm-wiifi-manager
A Wii homebrew program that allows you to change the Internet Settings without going to the Wii Menu.

Heavily based on [Bazmocs](https://github.com/Bazmoc) [Wii-Network-Config-Editor ](https://github.com/Bazmoc/Wii-Network-Config-Editor/)

## Features
- Set the active connection to use a connected LAN adapter
- Read wifi configurations from files saved on usb/sd (files are read from /wifi, file extension doesn't matter)

## Warning
This program makes permanent changes to the NAND, use (and/or modify) at your own risks. It has been tested on Dolphin and 5 seperate Wiis without issues, but please make a NAND Backup before using it.
The wifi settings are changed by editing the config file /shared2/sys/net/02/config.dat on the NAND.
