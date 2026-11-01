# Contributing

Contributions are welcome.

## Build

```bash
make clean all
```

The default compiler is `x86_64-w64-mingw32-g++`, producing a 64-bit Windows GUI
executable at `build/ACZModLoader.exe`. You can override it:

```bash
make CXX=/path/to/x86_64-w64-mingw32-g++
```

## Pull requests

- Keep the loader generic. It must not contain game-specific hook code or
  bundled mods.
- Add new manifest features in a backward-compatible way.
- Keep the executable GUI-subsystem and console-free.
- Do not add network, telemetry, persistence, or auto-update behavior.
- Explain any new Windows API usage in the README or code comments.
- Test the loader with at least one valid mod folder and one malformed manifest.

## Security reports

Because this project intentionally performs DLL injection, antivirus false
positives and security concerns are expected. Please report concrete issues
privately to the maintainer before public disclosure when possible.

## License

By contributing, you agree that your contribution is licensed under
GPL-3.0-or-later, the same license as the project.
