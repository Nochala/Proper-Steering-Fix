## Proper Steering Fix

**Proper Steering Fix** is an `.asi` plugin for **GTA V** that disables the game’s native wheel auto-centering behavior when exiting vehicles, with full support for both **Legacy** and **Enhanced** builds.

[![Latest Release](https://img.shields.io/github/v/release/YOURUSER/YOURREPO?include_prereleases&label=Latest%20Release)](https://github.com/Nochal/Proper-Steering-Fix/releases/latest)
![Language](https://img.shields.io/badge/LANGUAGE-C%23-239120?logo=csharp&logoColor=white)
![Platform](https://img.shields.io/badge/PLATFORM-GTA%20V-111111)
![Type](https://img.shields.io/badge/TYPE-.ASI%20Plugin-8A2BE2)
![Support](https://img.shields.io/badge/SUPPORT-Legacy%20%7C%20Enhanced-0A84FF)
![Status](https://img.shields.io/badge/STATUS-Release%20Ready-2EA043)
---

## Features

- Automatic game version detection for build-specific patching
- Prevents vehicle wheels from snapping back to center on exit
- Preserves steering angle behavior in a more natural and realistic way
- Optional startup notification, configurable through the `.ini`
- Optional logging, configurable through the `.ini`

---

## Requirements

- [Latest ScriptHookV](https://www.dev-c.com/gtav/scripthookv/)

---

## Installation

1. Install [Latest ScriptHookV](https://www.dev-c.com/gtav/scripthookv/).
2. Place `ProperSteeringFix.asi` and `ProperSteeringFix.ini` into your main **GTA V** directory.
3. Launch the game and enjoy.

---

## Changelog

### v1.1.0
- Reworked the entire Enhanced patch
- Updated Enhanced AOB patterns for improved compatibility with newer game versions
- Added fallback patching for both Legacy and Enhanced if AOB scanning fails
- Released the source code

### v1.0.1
- Renamed the script from `SteeringFix` to `ProperSteeringFix`
- Refined Enhanced patterns for improved stability
- Fixed the `.ini` option where `EnableLog=0` did not properly disable logging

### v1.0.0
- Initial release

---

## Credits & Acknowledgements

- **Alexander Blade** — for **ScriptHookV**
- **ShinyWasabi** — for **scrDbg**

### Special Thanks
- The **GTA V modding community** for shared research, documentation, and reverse-engineering efforts
- Everyone who reports bugs, tests updates, or provides feedback — your input directly helps improve the mod

---

## Mod Mirrors
- [**gta5-mods**](https://www.gta5-mods.com/scripts/proper-steering-fix#description_tab)
- [**Nexus Mods Enhanced**](https://www.nexusmods.com/gta5enhanced/mods/469?tab=files)
- [**Nexus Mods Legacy**](https://www.nexusmods.com/gta5/mods/1666)
