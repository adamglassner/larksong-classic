# Daybreak Classic

The [Daybreak](https://github.com/adamglassner/daybreak) alarm clock for older Windows: one small `Daybreak.exe` that runs on **Windows 95, 98, Me, 2000, XP, Vista, 7, 8, 8.1, 10 and 11**.

Download it from [Releases](https://github.com/adamglassner/daybreak-classic/releases). No installer: copy `Daybreak.exe` anywhere and run it.

## Features

- Analog and digital clock with seconds, plus up to 6 world clocks (with daylight-saving rules)
- Alarms with repeat days, names, snooze, your own WAV sound, and gradually rising volume
- Up before your alarm? Dismiss its next ring from the Alarms page or the tray icon
- Bedtime reminder with an optional matching wake-up alarm
- Stopwatch with laps, and a timer
- Themes: Classic, Dark, Terminal, Clock radio (LED digits) and Retro (flip clock)
- Back up and restore alarms to a `.json` file that regular Daybreak can also read
- Runs in the notification area; can start with Windows
- On Windows 7 and later: weather (Open-Meteo) and update checks

## How it stays compatible

- Plain C against the Win32 API, ANSI functions only, and only functions that exist in Windows 95
- No C runtime library, no floating point (trigonometry and the alarm tone come from small tables)
- 32-bit, i486 instructions, PE subsystem version 4.0
- WinHTTP (for weather and updates) is loaded at run time and only used on Windows 7+

## Building

With the MinGW-w64 32-bit toolchain (`gcc-mingw-w64-i686`): `./build.sh` → `build/Daybreak.exe`.

Change the version in `VERSION`; every push to `main` builds and publishes a `classic-vX.Y.Z` release.
