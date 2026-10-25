#!/bin/sh
# One-time provisioning for the manual "Build self-hosted" workflow.
# Run from the repository root with:
#   sudo scripts/setup-selfhosted-linux.sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
  echo "Run with sudo: sudo $0" >&2
  exit 1
fi

apt-get update
apt-get install -y \
  g++-12 \
  cmake \
  ninja-build \
  pkg-config \
  libfreetype-dev \
  libfontconfig-dev \
  libx11-dev \
  libxi-dev \
  libxext-dev \
  libxrender-dev \
  libxrandr-dev \
  libxfixes-dev \
  libxcursor-dev \
  libxinerama-dev \
  libxtst-dev \
  libgl-dev \
  libgtk-3-dev \
  xvfb \
  libgl1-mesa-dri
