# ACZModLoader

A simple, naive mod loader implementation for the Windows PC port of ACE COMBAT ZERO: THE BELKAN WAR.

Place `ACZModLoader.exe` beside `acz.exe`. Steam launch options:

```text
"H:\path\to\ACZero\ACZModLoader.exe" %command%
```

Or rename `acz.exe` to `acz_game.exe` and `ACZModLoader.exe` to `acz.exe`.

## Build

MinGW-w64 (`x86_64-w64-mingw32-g++`).

```bash
make
```

```text
build/ACZModLoader.exe
```

```bash
make clean
```

## Mods

```text
ACZero\
  acz.exe
  ACZModLoader.exe
  mods\
    example_mod\
      mod.ini
      example_mod.dll
```

```ini
[mod]
name=Example Mod
version=1.0
author=Your Name
type=dll
dll=example_mod.dll
enabled=true
priority=100
```

| Key | Meaning |
| --- | --- |
| `name` | Display name. Defaults to the folder name. |
| `version` | Version string. |
| `author` | Author string. |
| `type` | `dll` |
| `dll` | DLL filename relative to the mod folder. Auto-detected when the folder has exactly one DLL. |
| `enabled` | `true` / `false`. Default `true`. |
| `priority` | Lower loads first. Default `100`. |

A folder name ending in `.disabled` is skipped.

Example: [`examples/skip_startup`](examples/skip_startup)

Log: `mods\loader.log`

GNU GPLv3 or later. See [`LICENSE`](LICENSE).
