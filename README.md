# SentinelScore — Simple Threat Prioritizer (C++17)

A tiny C2-style console tool that ranks airborne contacts by risk and suggests a basic action (Monitor / Elevated Monitor / Intercept).

## Why this is “defense-industry relevant”
Command & Control and air-defense systems constantly fuse tracks and prioritize threats. This project models a **mini** version of the prioritization step: it reads contacts from a CSV, scores them with a transparent function, and sorts them for the operator.

## Features
- CSV ingest: `id, iff, range_km, closing_mps, altitude_m, rcs_m2`
- Tunable scoring weights for explainability
- Clean, sorted table output with an engagement suggestion

## Build & Run

### Prereqs
- C++17 compiler (clang++ or g++)
- CMake 3.12+

### Build
```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
