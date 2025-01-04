#!/usr/bin/env bash

# See https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md
apt update
apt install \
    libasound2-dev libjack-jackd2-dev \
    ladspa-sdk \
    libfreetype-dev libfontconfig1-dev \
    libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
    libglu1-mesa-dev mesa-common-dev
