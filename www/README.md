# www/ — REFERENCE ONLY

**Nothing in this folder is compiled, flashed, or served.**

Editing these files has **no effect** on the device or on the web flasher. They are
design archives — earlier drafts of the HTML that now lives inside the firmware.

---

## Where the live code actually is

The CYD's built-in web config page is a **raw string literal inside the firmware
source**, not a file served from the filesystem:

| What | Where |
|---|---|
| Config page HTML | `src/main.cpp` — `generateConfigHTML()` starts at **line 840** |
| Config page title | `Poker Timer Config` (inside that raw literal) |
| Route served | `GET /` — handler at **line 1627** |
| Save route | `POST /save` — line 1641 |
| Cancel route | `GET /cancel` — line 1806 |
| Version route | `GET /version` — line 1814 |

To change the config page, edit the HTML inside `generateConfigHTML()` in
`src/main.cpp`. Nothing else.

## Why there is no filesystem serving

There is **no** `SPIFFS.begin()`, no `LittleFS`, and no upload of this folder
anywhere in the build. The `spiffs` partition is declared in `no_ota.csv` but is
**unused** — no data is ever written to it. Everything is compiled into the app
image as C++ string literals.

Consequence: the HTML costs flash space (it is part of `firmware.bin`) but cannot
be updated without a reflash.

## The two separate "web" surfaces — don't confuse them

| Surface | Source | Updates via |
|---|---|---|
| **Device config page** (served by the CYD over its own WiFi AP at `192.168.4.1`) | `src/main.cpp` → `generateConfigHTML()` | commit + push to `main` → CI rebuild |
| **Web flasher page** (GitHub Pages, `rfltools.github.io/Poker_Timer_Lite_CYD`) | `web_flasher/index.html` + `manifest.json`; `.bin` files built by CI | commit + push to `main` → CI rebuild + Pages deploy |

Neither one reads from `www/`.

## Files here (all inert)

| File | Status |
|---|---|
| `index.html` | Draft/reference copy of the config page. Superseded by `generateConfigHTML()`. |
| `blind-config.html` | Early draft. Never served. |
| `device-instructions.html` | Early draft. Never served. |
| `index_pre_batteryonly.html` | Historical snapshot. |
| `index_pre_instructions.html` | Historical snapshot. |
| `index_save.html` | Historical snapshot. |
| `images/` | Images used by the early drafts and by the project docs. Not embedded in firmware. |

> **Note:** the `images/` subfolder was added in commit `3dd45c0` ("Added WWW From Docker")
> when this project moved machines. It is not referenced by any build step.

## If you meant to change something else

- **Change the installer/landing page on GitHub Pages** → edit `web_flasher/index.html`
- **Change the flash offsets or chip family** → edit `web_flasher/manifest.json`
- **Change the device's on-screen UI or config form** → edit `src/main.cpp`
- **Change how CI builds/publishes** → edit `.github/workflows/build.yml`
- **Change the enclosure** → `case/*.stl`

None of those are in this folder.

---

*Last verified: 2026-11-11 — confirmed zero `SPIFFS`/`LittleFS`/`www` references
in `src/main.cpp` and in `.github/workflows/build.yml`.*
