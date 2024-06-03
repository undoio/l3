#!/bin/bash
# ##############################################################################
# l3/scripts/setup-ubuntu-linux.sh:
# Simple script to setup the required s/w components to get L3 working.
# ##############################################################################

sudo apt update -y

sudo apt-get install make
sudo apt-get install gcc g++ -y

sudo apt install -y pylint pip
sudo apt-get install -y libfmt-dev/jammy
sudo apt-get install -y libspdlog-dev

pip install pytest prettytable

git submodule update --remote

export PATH="/home/ubuntu/.local/bin:$PATH"

