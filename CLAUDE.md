# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Outflank ("The Spatial Tactician") is a planned stereo crossover utility plugin. Concept, from `README.md`:

- Splits the signal into low vs. mid/high bands
- Low frequencies are collapsed to mono ("anchored dead-center") to keep bass focus tight
- Mid/high frequencies are widened into the sides for stereo width
- Goal: maximize perceived stereo width without smearing or losing low-end punch

## Current State

This repository currently contains only `README.md` — no source code, build system, or dependencies have been added yet. There is nothing to build, lint, or test.

This is a sibling of the other C++/JUCE plugins under `AudioPlugins/` (Bastos, MixAdvice, etc. — see `/home/yvan/Projects/CLAUDE.md` for the workspace-level project list). When implementation starts, it will likely follow the same toolchain and layout those projects use:

- CMake + Ninja, JUCE fetched via `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` then `cmake --build build --parallel`
- VST3/CLAP/Standalone targets, DSP code under `Source/DSP/`, processor/editor under `Source/`
- Parameters exposed via JUCE's `AudioProcessorValueTreeState` (APVTS)

Do not assume any of this structure exists until it is actually created — check the current directory contents first.
