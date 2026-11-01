# Skip Startup

Skips the boot presentation before the title screen:

```text
CModeWakeLogo      publisher / legal logos
ModeMoviePlay      startup movies, including the opening movie
ModeGameControl    startup control-scheme confirmation
```

## Build

MinGW-w64 (`x86_64-w64-mingw32-g++`).

```bash
make
```

```text
../../build/skip_startup.dll
```

```bash
make clean
```

## Layout

```text
skip_startup\
  skip_startup.cpp
  mod_common.h
  Makefile
  mod.ini
  skip_startup.ini
```

Copy the folder under `mods\`:

```text
ACZero\mods\skip_startup\
  skip_startup.dll
  mod.ini
  skip_startup.ini
```

```ini
[skip_startup]
enabled=true
logos=true
startup_movies=true
control_scheme=true
```
