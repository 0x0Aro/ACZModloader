#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mod_common.h"

/*
 * Skip the boot presentation chain:
 *
 *   CModeWakeLogo         publisher/legal logos
 *   ModeMoviePlay         startup movie(s), including the opening movie
 *   ModeGameControl       startup control-scheme confirmation
 *
 * CModeWakeMemorycard stays on the stack. That mode initializes and
 * loads the PS2 save data.
 *
 * The game's mode stack is ModeHolder.  Every mode update receives a pointer to
 * the ModeHolder as its second argument.  ModeHolder vtable +0x20 is the "pop
 * current mode" operation.  Popping a startup mode performs its normal
 * destructor and resumes the previous mode.
 *
 * Once CModeTitle has been reached, startup skipping is disabled.
 */

enum SkipRva {
    kRvaWakeLogoUpdate = 0x17DC70,
    kRvaMoviePlayUpdate = 0x1735A0,
    kRvaGameControlUpdate = 0x16E8C0,
    kRvaTitleUpdate = 0x17ACB0,
};

static HMODULE g_game;
static HMODULE g_self;
static ModLogger g_log;
static wchar_t g_mod_dir[MAX_PATH];

static bool g_enabled = true;
static bool g_skip_logos = true;
static bool g_skip_movies = true;
static bool g_skip_control = true;
static bool g_startup_done;

typedef void(__fastcall* ModeUpdateFn)(void* self, void* mode_holder);
typedef void(__fastcall* ModePopFn)(void* mode_holder);

static ModeUpdateFn Orig_WakeLogoUpdate;
static ModeUpdateFn Orig_MoviePlayUpdate;
static ModeUpdateFn Orig_GameControlUpdate;
static ModeUpdateFn Orig_TitleUpdate;

static void OpenLog() {
    wchar_t path[MAX_PATH];
    ModJoinPath(path, MAX_PATH, g_mod_dir, L"skip_startup.log");
    g_log.Open(path);
}

static void ReadConfig() {
    wchar_t path[MAX_PATH];
    ModJoinPath(path, MAX_PATH, g_mod_dir, L"skip_startup.ini");
    char value[64];

    if (ModReadIniString(path, "enabled", value, sizeof(value))) {
        g_enabled = !ModIeq(value, "false") && !ModIeq(value, "0") && !ModIeq(value, "off");
    }
    if (ModReadIniString(path, "logos", value, sizeof(value))) {
        g_skip_logos = !ModIeq(value, "false") && !ModIeq(value, "0") && !ModIeq(value, "off");
    }
    if (ModReadIniString(path, "startup_movies", value, sizeof(value))) {
        g_skip_movies = !ModIeq(value, "false") && !ModIeq(value, "0") && !ModIeq(value, "off");
    }
    if (ModReadIniString(path, "control_scheme", value, sizeof(value))) {
        g_skip_control = !ModIeq(value, "false") && !ModIeq(value, "0") && !ModIeq(value, "off");
    }

    g_log.Write(
        "config enabled=%d logos=%d movies=%d control=%d",
        g_enabled,
        g_skip_logos,
        g_skip_movies,
        g_skip_control);
}

static bool PopCurrentMode(void* mode_holder) {
    if (!mode_holder) {
        g_log.Write("cannot pop mode: null ModeHolder");
        return false;
    }
    const int index = *(int*)((BYTE*)mode_holder + 0x48);
    if (index <= 0) {
        g_log.Write("cannot pop mode: ModeHolder stack index is %d", index);
        return false;
    }
    void** vtable = *(void***)mode_holder;
    if (!vtable || !vtable[4]) {
        g_log.Write("cannot pop mode: invalid ModeHolder vtable %p", vtable);
        return false;
    }
    g_log.Write("popping current startup mode via ModeHolder=%p vtable=%p index=%d", mode_holder, vtable, index);
    ((ModePopFn)vtable[4])(mode_holder);
    return true;
}

static bool SkipStartupMode(const char* name, bool skip_this_mode) {
    if (!g_enabled || g_startup_done || !skip_this_mode) return false;
    g_log.Write("skip startup mode: %s", name);
    return true;
}

static void __fastcall Hook_WakeLogoUpdate(void* self, void* mode_holder) {
    if (SkipStartupMode("CModeWakeLogo", g_skip_logos)) {
        if (PopCurrentMode(mode_holder)) return;
    }
    if (Orig_WakeLogoUpdate) Orig_WakeLogoUpdate(self, mode_holder);
}

static void __fastcall Hook_MoviePlayUpdate(void* self, void* mode_holder) {
    if (SkipStartupMode("ModeMoviePlay", g_skip_movies)) {
        if (PopCurrentMode(mode_holder)) return;
    }
    if (Orig_MoviePlayUpdate) Orig_MoviePlayUpdate(self, mode_holder);
}

static void __fastcall Hook_GameControlUpdate(void* self, void* mode_holder) {
    if (SkipStartupMode("ModeGameControl", g_skip_control)) {
        if (PopCurrentMode(mode_holder)) return;
    }
    if (Orig_GameControlUpdate) Orig_GameControlUpdate(self, mode_holder);
}

static void __fastcall Hook_TitleUpdate(void* self, void* mode_holder) {
    if (!g_startup_done) {
        g_startup_done = true;
        g_log.Write("title mode reached; startup skip disabled");
    }
    if (Orig_TitleUpdate) Orig_TitleUpdate(self, mode_holder);
}

static bool InstallHook(
    const char* name,
    DWORD rva,
    const BYTE* expected,
    size_t expected_size,
    void* hook,
    void** trampoline) {
    BYTE* target = (BYTE*)g_game + rva;
    if (memcmp(target, expected, expected_size) != 0) {
        g_log.Write(
            "hook %s skipped: prologue mismatch at RVA 0x%x "
            "(got %02x %02x %02x %02x ... expected %02x %02x %02x %02x ...)",
            name,
            rva,
            target[0], target[1], target[2], target[3],
            expected[0], expected[1], expected[2], expected[3]);
        return false;
    }
    if (!ModInstallAbsoluteJumpHook(target, hook, trampoline, expected_size)) {
        g_log.Write("hook %s failed at RVA 0x%x", name, rva);
        return false;
    }
    g_log.Write("hook %s installed at RVA 0x%x", name, rva);
    return true;
}

static void InstallHooks() {
    /* 40 53 48 83 EC 30 44 8B 81 90 00 00 00 48 8B D9 */
    static const BYTE wake_logo[] = {
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x44, 0x8B,
        0x81, 0x90, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xD9,
    };
    /* 48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B F9 48 8B F2 8B 49 28 */
    static const BYTE movie_play[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
        0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48,
        0x8B, 0xF9, 0x48, 0x8B, 0xF2, 0x8B, 0x49, 0x28,
    };
    /* 48 89 5C 24 08 57 48 83 EC 20 80 79 18 00 48 8B FA */
    static const BYTE game_control[] = {
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x80, 0x79, 0x18, 0x00, 0x48, 0x8B,
        0xFA,
    };
    /* 48 89 5C 24 10 57 48 83 EC 20 44 8B 81 B0 00 00 00 48 8B DA 48 8B F9 */
    static const BYTE title[] = {
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x44, 0x8B, 0x81, 0xB0, 0x00, 0x00,
        0x00, 0x48, 0x8B, 0xDA, 0x48, 0x8B, 0xF9,
    };

    InstallHook(
        "CModeWakeLogo::update",
        kRvaWakeLogoUpdate,
        wake_logo,
        sizeof(wake_logo),
        (void*)Hook_WakeLogoUpdate,
        (void**)&Orig_WakeLogoUpdate);
    InstallHook(
        "ModeMoviePlay::update",
        kRvaMoviePlayUpdate,
        movie_play,
        sizeof(movie_play),
        (void*)Hook_MoviePlayUpdate,
        (void**)&Orig_MoviePlayUpdate);
    InstallHook(
        "ModeGameControl::update",
        kRvaGameControlUpdate,
        game_control,
        sizeof(game_control),
        (void*)Hook_GameControlUpdate,
        (void**)&Orig_GameControlUpdate);
    InstallHook(
        "CModeTitle::update",
        kRvaTitleUpdate,
        title,
        sizeof(title),
        (void*)Hook_TitleUpdate,
        (void**)&Orig_TitleUpdate);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_self = instance;
        g_game = GetModuleHandleW(NULL);

        wchar_t module_path[MAX_PATH];
        if (GetModuleFileNameW(instance, module_path, MAX_PATH)) {
            wchar_t* slash = wcsrchr(module_path, L'\\');
            if (slash) {
                slash[1] = 0;
                wcscpy_s(g_mod_dir, module_path);
            }
        }

        OpenLog();
        g_log.Write("skip_startup attached base=%p self=%p", g_game, instance);
        ReadConfig();
        InstallHooks();
    }
    return TRUE;
}
