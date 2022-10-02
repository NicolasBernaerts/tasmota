#!/bin/sh
# Tasmota tools installation script

# install tools
sudo apt -y install python3-pip
pip3 install --upgrade esptool

# tasmota-discover
sudo wget -O /usr/local/bin/tasmota-discover https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/tools/tasmota-discover
sudo chmod +x /usr/local/bin/tasmota-discover

# tasmota-flash
sudo wget -O /usr/local/bin/tasmota-flash https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/tools/tasmota-flash
sudo chmod +x /usr/local/bin/tasmota-flash
sudo wget -O /etc/bash_completion.d/tasmota-flash_completion https://raw.githubusercontent.com/NicolasBernaerts/tasmota/master/tools/tasmota-flash_completion
