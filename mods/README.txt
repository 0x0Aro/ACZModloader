ACZ MODS
========

Each mod is one folder under this directory. ACZModLoader.exe scans the folders
at startup, sorts them by the numeric `priority` field, and injects the DLL named
in each mod.ini.

Example layout:

  mods\
    my_mod\
      mod.ini
      my_mod.dll
      optional_assets\

Example mod.ini:

  [mod]
  name=My Mod
  version=1.0
  author=Your Name
  type=dll
  dll=my_mod.dll
  enabled=true
  priority=100

Fields:

  name       Display name written to mods\loader.log.
  version    Optional free-form version string.
  author     Optional free-form author string.
  type       dll (default) for a native mod DLL. Other types are reserved for
             future declarative/data mods.
  dll        DLL filename relative to the mod folder. Required unless the folder
             contains exactly one DLL, in which case it is auto-detected.
  enabled    true/false. Default is true.
  priority   Lower numbers load first. Default is 100.

Folder names ending in `.disabled` are skipped. Setting enabled=false also skips
a mod.

The loader does not compile anything at game startup. Mods are ordinary
precompiled DLLs and every file remains visible/editable in the mod folder.

A mod DLL should locate its own assets relative to its module path, for example
with GetModuleFileNameW(instance). All enabled mod folders are added to the child
process PATH, so sibling dependency DLLs can be loaded normally. The loader
itself only handles discovery, ordering, launching acz.exe, and injection after
the SteamStub code section is decrypted.

Logs:

  mods\loader.log       loader startup, mod discovery, order, injection result

Do not replace version.dll or winmm.dll. ACZModLoader is an external loader.
