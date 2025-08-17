# SentinelScore — Simple Threat Prioritizer (C++17)

A tiny C2-style console tool that ranks airborne contacts by risk and suggests a basic action (Monitor / Elevated Monitor / Intercept).

---

## Why I Built This
I built this project to **practice applying C++ to a realistic defense-industry use case** without relying on any sensitive or classified data. In modern command-and-control systems, operators and algorithms must constantly assess which targets pose the highest risk. This project models a simplified version of that process:
- Ingest contacts from sensors (simulated by a CSV file).
- Assign scores based on measurable properties like range, speed, and radar cross-section.
- Rank and recommend actions (ignore, monitor, intercept).

This small project mirrors real workflows in air-defense software and shows how domain knowledge can be translated into software logic.

---

## What I Learned
Working on SentinelScore taught me several valuable lessons:
- **C++17 Practice**: Using STL features like `vector`, `optional`, `sort`, and string/stream utilities.
- **Input Validation**: Handling messy CSV input safely (headers, comments, malformed rows).
- **Domain Modeling**: Converting qualitative defense concepts (friend vs foe, proximity, closing speed) into a **quantitative risk-scoring algorithm**.
- **System Thinking**: Understanding how even a simplified prioritizer fits into a larger C2 workflow.
- **Software Engineering Habits**: Organizing code, using CMake, and documenting assumptions.

---

## Why this is “defense-industry relevant”
Command & Control and air-defense systems constantly fuse tracks and prioritize threats. This project models a **mini** version of the prioritization step: it reads contacts from a CSV, scores them with a transparent function, and sorts them for the operator.

---

## Features
- CSV ingest: `id, iff, range_km, closing_mps, altitude_m, rcs_m2`
- Tunable scoring weights for explainability
- Clean, sorted table output with an engagement suggestion

---

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

