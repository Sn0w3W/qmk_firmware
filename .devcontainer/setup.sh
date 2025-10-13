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

TOOLCHAIN_URL="https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-aarch64-linux.tar.bz2"
INSTALL_DIR="/opt/arm-gnu-toolchain"
BIN_DIR="$INSTALL_DIR/bin"

echo "[*] Installing ARM GNU Toolchain..."
mkdir -p "$INSTALL_DIR"
cd /tmp

wget -q --show-progress -O toolchain.tar.bz2 "$TOOLCHAIN_URL"
tar -xjf toolchain.tar.bz2 -C "$INSTALL_DIR" --strip-components=1
rm toolchain.tar.bz2

echo "export PATH=\"$BIN_DIR:\$PATH\"" >> /root/.bashrc
export PATH="$BIN_DIR:$PATH"

echo "[*] Installed version:"
arm-none-eabi-gcc --version | head -n 1

echo "[✔] ARM GCC successfully installed."

mkdir -p /etc/udev/rules.d
cp /workspaces/qmk_firmware/util/udev/50-qmk.rules /etc/udev/rules.d/
