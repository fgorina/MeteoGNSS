# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

## Build

```bash
pio run           # build
pio run -t upload # flash
```

- Target: `m5stack-core2` (defined in `platformio.ini`)
- Partition table: `partitions.csv`
- Display driver: LovyanGFX — **avoid single-argument `setTextColor()`** (always pass background colour as second argument)
- PSRAM is required and assumed available
- Entry point: `src/main.cpp`

---

## Library Structure

```
lib/
  M5Module-GNSS/   — access to gnss, imu and pressure semsprs 🔒 CLOSED
```

**Do not modify closed/settled libraries unless explicitly instructed.**

## State (`State.h` / `State.cpp`)

Stores "truth" data to/from signalk/nmea sertver/bus
Units are SI
Computes averages/tendencies and understands SignalK paths

## Screen (`Screen.h` / `Screen.cpp` / `M5Button.h`). CLOSED

Main UI element. 
Do not touch. consider it CLOSED.
If need to mmodify first propose idea and ask.


## UI (Based in Screen)

GNSSScreen (.cpp, .h) show GNSS Information
MeteoScreen (.cpp, .h) show atmospheric pressure + wind
SummaryScreen(.cpp, .h) Polar graph of wind / pressure evolution
PressureScren(.cpp, .h) Atmopspheric pressure against time graphg

## Configuration
First time it runs gets name as "METEOGNSS_" + rtandom between 100 and 1000. Will be deviceName and initial ssid 
Defined in AppConfig.h
Stored in Preferences
Configured via web server in net_webserver (.h, .cpp)
When ssid is not configured creates a WiFi with ssid AppConfig.deviceName

## Reading data (Sensors.h, Sensors.cpp)
All device / sensor reading code should be in Sensors

## Communications
Communicates to the exterior world throug signalk
Code is in newt_signalk (.h. .cpp)
Asks for Auth code first time to be able to write. Is stored in AppConfig

---

## Key Design Decisions (do not revert without discussion)

- **Reduce dynamic allocation at runtime** — State must be sufficient.
- **SI units in State, always** — unit conversion is a display/formatter concern only.
- **LovyanGFX `setTextColor()` must always have two arguments, accessed from M5Unified/M5GFX** .
- **Config on Preferences **.
