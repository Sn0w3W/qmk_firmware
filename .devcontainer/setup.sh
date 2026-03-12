#!/usr/bin/env bash

set -eEuo pipefail

wget https://bootstrap.pypa.io/get-pip.py
python3 get-pip.py
python3 -m pip install qmk
rm get-pip.py

python3 -m pip install --upgrade milc

userspacePath="$1"

git config --global --add safe.directory "$userspacePath"
git submodule update --init --recursive

qmk git-submodule

mkdir -p /etc/udev/rules.d
cp /workspaces/qmk_firmware/util/udev/50-qmk.rules /etc/udev/rules.d/
