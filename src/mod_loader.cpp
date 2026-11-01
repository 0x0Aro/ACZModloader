// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 0x00Aro

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "mod_common.h"

struct ModInfo {
    std::wstring folder;
    std::wstring config_path;
    std::wstring dll_path;
    std::wstring name;
    std::wstring version;
    std::wstring author;
    std::wstring type;
    int priority = 100;
};

static std::wstring g_loader_dir;
static bool g_log_created;

static std::wstring LoaderDirectory() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(NULL, path, MAX_PATH)) return L"";
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) slash[1] = 0;
    return path;
}

static void LoaderLog(const char* format, ...) {
    if (g_loader_dir.empty()) g_loader_dir = LoaderDirectory();
    std::wstring mods = g_loader_dir + L"mods";
    CreateDirectoryW(mods.c_str(), NULL);
    std::wstring path = mods + L"\\loader.log";
    HANDLE file = CreateFileW(
        path.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        g_log_created ? OPEN_ALWAYS : CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    g_log_created = true;
    if (file == INVALID_HANDLE_VALUE) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    char line[2048];
    va_list args;
    va_start(args, format);
    int count = vsnprintf(line, sizeof(line) - 2, format, args);
    va_end(args);
    if (count >= 0) {
        char stamped[2304];
        int stamped_count = _snprintf(
            stamped,
            sizeof(stamped) - 2,
            "[%04u-%02u-%02u %02u:%02u:%02u][pid %lu] %s\n",
            st.wYear,
            st.wMonth,
            st.wDay,
            st.wHour,
            st.wMinute,
            st.wSecond,
            GetCurrentProcessId(),
            line);
        if (stamped_count > 0) {
            DWORD written = 0;
            WriteFile(file, stamped, (DWORD)stamped_count, &written, NULL);
        }
    }
    CloseHandle(file);
}

static void FatalError(const std::wstring& message) {
    LoaderLog("FATAL: %ls", message.c_str());
    MessageBoxW(NULL, message.c_str(), L"ACZ Mod Loader", MB_OK | MB_ICONERROR);
}

static bool EndsWithNoCase(const std::wstring& text, const std::wstring& suffix) {
    if (text.size() < suffix.size()) return false;
    return _wcsicmp(text.c_str() + text.size() - suffix.size(), suffix.c_str()) == 0;
}

static bool FileExists(const std::wstring& path) {
    DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring ReadIni(const std::wstring& ini, const char* key, const wchar_t* fallback = L"") {
    char value[512];
    if (!ModReadIniString(ini.c_str(), key, value, sizeof(value))) return fallback;
    wchar_t wide[512];
    MultiByteToWideChar(CP_UTF8, 0, value, -1, wide, 512);
    return wide;
}

static int ReadIniInt(const std::wstring& ini, const char* key, int fallback) {
    char value[64];
    if (!ModReadIniString(ini.c_str(), key, value, sizeof(value))) return fallback;
    return atoi(value);
}

static bool IsSafeRelativePath(const std::wstring& path) {
    if (path.empty()) return false;
    if (path[0] == L'\\' || path[0] == L'/') return false;
    if (path.size() >= 2 && path[1] == L':') return false;
    size_t start = 0;
    while (start <= path.size()) {
        size_t end = path.find_first_of(L"\\/", start);
        if (end == std::wstring::npos) end = path.size();
        std::wstring part = path.substr(start, end - start);
        if (part == L"..") return false;
        if (end == path.size()) break;
        start = end + 1;
    }
    return true;
}

static std::wstring AutoDetectDll(const std::wstring& folder) {
    WIN32_FIND_DATAW data;
    std::wstring pattern = folder + L"\\*.dll";
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return L"";
    std::wstring result;
    int count = 0;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        result = data.cFileName;
        ++count;
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return count == 1 ? result : L"";
}

static std::vector<ModInfo> ScanMods() {
    std::vector<ModInfo> mods;
    std::wstring mods_root = g_loader_dir + L"mods";
    CreateDirectoryW(mods_root.c_str(), NULL);

    WIN32_FIND_DATAW data;
    std::wstring pattern = mods_root + L"\\*";
    HANDLE find = FindFirstFileW(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) {
        LoaderLog("no mods directory entries found");
        return mods;
    }

    do {
        if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
        std::wstring folder_name = data.cFileName;
        if (EndsWithNoCase(folder_name, L".disabled")) {
            LoaderLog("skip %ls: folder disabled by suffix", folder_name.c_str());
            continue;
        }
        std::wstring folder = mods_root + L"\\" + data.cFileName;
        std::wstring config = folder + L"\\mod.ini";
        if (!FileExists(config)) {
            LoaderLog("skip %ls: missing mod.ini", folder.c_str());
            continue;
        }
        if (!ModIniEnabled(config.c_str())) {
            LoaderLog("skip %ls: enabled=false", folder.c_str());
            continue;
        }

        ModInfo mod;
        mod.folder = folder;
        mod.config_path = config;
        mod.name = ReadIni(config, "name", data.cFileName);
        mod.version = ReadIni(config, "version", L"");
        mod.author = ReadIni(config, "author", L"");
        mod.type = ReadIni(config, "type", L"dll");
        mod.priority = ReadIniInt(config, "priority", 100);
        if (_wcsicmp(mod.type.c_str(), L"dll") != 0) {
            LoaderLog("skip %ls: unsupported mod type '%ls'", config.c_str(), mod.type.c_str());
            continue;
        }
        std::wstring dll = ReadIni(config, "dll", L"");
        if (dll.empty()) dll = AutoDetectDll(folder);
        if (dll.empty()) {
            LoaderLog("skip %ls: no dll= entry and no unique DLL", config.c_str());
            continue;
        }
        if (!IsSafeRelativePath(dll)) {
            LoaderLog("skip %ls: unsafe dll path '%ls'", config.c_str(), dll.c_str());
            continue;
        }
        mod.dll_path = folder + L"\\" + dll;
        if (!FileExists(mod.dll_path)) {
            LoaderLog("skip %ls: DLL not found '%ls'", config.c_str(), mod.dll_path.c_str());
            continue;
        }
        mods.push_back(mod);
    } while (FindNextFileW(find, &data));
    FindClose(find);

    std::sort(mods.begin(), mods.end(), [](const ModInfo& a, const ModInfo& b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        return _wcsicmp(a.folder.c_str(), b.folder.c_str()) < 0;
    });

    for (const ModInfo& mod : mods) {
        LoaderLog(
            "mod: %ls | version=%ls | author=%ls | type=%ls | priority=%d | dll=%ls",
            mod.name.c_str(),
            mod.version.empty() ? L"-" : mod.version.c_str(),
            mod.author.empty() ? L"-" : mod.author.c_str(),
            mod.type.c_str(),
            mod.priority,
            mod.dll_path.c_str());
    }
    LoaderLog("discovered %zu enabled mod(s)", mods.size());
    return mods;
}

static std::wstring QuoteArgument(const std::wstring& argument) {
    if (argument.empty()) return L"\"\"";
    if (argument.find_first_of(L" \t\"") == std::wstring::npos) return argument;
    std::wstring result = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') {
            ++backslashes;
        } else if (c == L'"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'"');
            backslashes = 0;
        } else {
            result.append(backslashes, L'\\');
            backslashes = 0;
            result.push_back(c);
        }
    }
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}


static std::wstring PrependModPaths(const std::vector<ModInfo>& mods) {
    if (mods.empty()) return L"";
    DWORD needed = GetEnvironmentVariableW(L"PATH", NULL, 0);
    std::vector<wchar_t> old_path(needed ? needed : 1, 0);
    if (needed) GetEnvironmentVariableW(L"PATH", old_path.data(), needed);
    std::wstring combined;
    for (const ModInfo& mod : mods) {
        combined += mod.folder;
        combined.push_back(L';');
    }
    combined += old_path.data();
    SetEnvironmentVariableW(L"PATH", combined.c_str());
    return std::wstring(old_path.data());
}

static std::wstring BuildChildCommandLine(const std::wstring& game_path) {
    std::wstring command = QuoteArgument(game_path);
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return command;
    for (int i = 1; i < argc; ++i) {
        std::wstring argument = argv[i];
        // Steam launch options typically expand to the original game path.
        // Do not pass that path to the child as an extra argument.
        if (i == 1 && argument.find(L".exe") != std::wstring::npos) {
            std::wstring name = argument;
            size_t slash = name.find_last_of(L"\\/");
            if (slash != std::wstring::npos) name = name.substr(slash + 1);
            if (_wcsicmp(name.c_str(), L"acz.exe") == 0 ||
                _wcsicmp(name.c_str(), L"acz_game.exe") == 0) {
                continue;
            }
        }
        command += L" ";
        command += QuoteArgument(argument);
    }
    LocalFree(argv);
    return command;
}

static uintptr_t FindRemoteModuleBase(DWORD pid, const wchar_t* module_name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W module;
    module.dwSize = sizeof(module);
    uintptr_t base = 0;
    if (Module32FirstW(snapshot, &module)) {
        do {
            if (_wcsicmp(module.szModule, module_name) == 0) {
                base = (uintptr_t)module.modBaseAddr;
                break;
            }
        } while (Module32NextW(snapshot, &module));
    }
    CloseHandle(snapshot);
    return base;
}

static bool InjectDll(HANDLE process, DWORD pid, const std::wstring& dll_path) {
    size_t bytes = (dll_path.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        LoaderLog("VirtualAllocEx failed for %ls (error %lu)", dll_path.c_str(), GetLastError());
        return false;
    }
    if (!WriteProcessMemory(process, remote, dll_path.c_str(), bytes, NULL)) {
        LoaderLog("WriteProcessMemory failed for %ls (error %lu)", dll_path.c_str(), GetLastError());
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }

    HMODULE local_kernel = GetModuleHandleW(L"kernel32.dll");
    FARPROC local_load = local_kernel ? GetProcAddress(local_kernel, "LoadLibraryW") : NULL;
    uintptr_t remote_kernel = FindRemoteModuleBase(pid, L"kernel32.dll");
    uintptr_t remote_load = 0;
    if (local_kernel && local_load && remote_kernel) {
        remote_load = remote_kernel + ((uintptr_t)local_load - (uintptr_t)local_kernel);
    } else {
        remote_load = (uintptr_t)local_load;
    }
    if (!remote_load) {
        LoaderLog("could not resolve remote LoadLibraryW for %ls", dll_path.c_str());
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }

    HANDLE thread = CreateRemoteThread(
        process, NULL, 0, (LPTHREAD_START_ROUTINE)remote_load, remote, 0, NULL);
    if (!thread) {
        LoaderLog("CreateRemoteThread failed for %ls (error %lu)", dll_path.c_str(), GetLastError());
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return false;
    }
    DWORD wait = WaitForSingleObject(thread, 30000);
    if (wait != WAIT_OBJECT_0) {
        LoaderLog(
            "LoadLibraryW timed out for %ls; terminating child to avoid a stuck loader-lock and leaked remote memory",
            dll_path.c_str());
        CloseHandle(thread);
        TerminateProcess(process, 1);
        WaitForSingleObject(process, 5000);
        return false;
    }
    DWORD module_handle = 0;
    GetExitCodeThread(thread, &module_handle);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    if (module_handle == 0) {
        LoaderLog("LoadLibraryW returned 0 for %ls (error=%lu)", dll_path.c_str(), GetLastError());
        return false;
    }
    LoaderLog("injected %ls (remote module=%p)", dll_path.c_str(), (void*)(uintptr_t)module_handle);
    return true;
}

static bool WaitForDecryptedOep(HANDLE process, DWORD pid, const std::wstring& module_name, uintptr_t* base) {
    static const BYTE expected[] = {
        0x48, 0x83, 0xEC, 0x28, 0xE8, 0x5B, 0x05, 0x00,
        0x00, 0x48, 0x83, 0xC4, 0x28, 0xE9, 0x7A, 0xFE,
        0xFF, 0xFF,
    };
    ULONGLONG deadline = GetTickCount64() + 180000;
    DWORD last_log = 0;
    while (GetTickCount64() < deadline) {
        if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0) {
            DWORD exit_code = 0;
            GetExitCodeProcess(process, &exit_code);
            LoaderLog("acz.exe exited before OEP decrypted (exit=%lu)", exit_code);
            return false;
        }
        if (*base == 0) {
            *base = FindRemoteModuleBase(pid, module_name.c_str());
        }
        if (*base) {
            BYTE current[sizeof(expected)] = {};
            SIZE_T read = 0;
            if (ReadProcessMemory(
                    process,
                    (const void*)(*base + 0x22F638),
                    current,
                    sizeof(current),
                    &read) &&
                read == sizeof(current) &&
                memcmp(current, expected, sizeof(current)) == 0) {
                LoaderLog("child pid=%lu base=%p OEP decrypted", pid, (void*)*base);
                return true;
            }
        }
        DWORD now = (DWORD)GetTickCount64();
        if (now - last_log >= 5000) {
            LoaderLog("waiting for child OEP (pid=%lu base=%p)", pid, (void*)*base);
            last_log = now;
        }
        Sleep(50);
    }
    LoaderLog("timed out waiting for child OEP");
    return false;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    g_loader_dir = LoaderDirectory();
    LoaderLog("ACZ Mod Loader start");
    if (g_loader_dir.empty()) {
        FatalError(L"Could not determine the loader directory.");
        return 1;
    }

    std::vector<ModInfo> mods = ScanMods();

    std::wstring game_path = g_loader_dir + L"acz_game.exe";
    if (!FileExists(game_path)) {
        game_path = g_loader_dir + L"acz.exe";
    }
    wchar_t self_path[MAX_PATH];
    GetModuleFileNameW(NULL, self_path, MAX_PATH);
    if (!FileExists(game_path) || _wcsicmp(game_path.c_str(), self_path) == 0) {
        FatalError(
            L"Could not find the game executable.\n\n"
            L"Place ACZModLoader.exe beside acz.exe.\n"
            L"If the loader is named acz.exe, rename the original game to acz_game.exe.");
        return 1;
    }

    std::wstring module_name = game_path;
    size_t slash = module_name.find_last_of(L"\\/");
    if (slash != std::wstring::npos) module_name = module_name.substr(slash + 1);

    std::wstring command_line = BuildChildCommandLine(game_path);
    LoaderLog("launching %ls", game_path.c_str());
    LoaderLog("command line: %ls", command_line.c_str());
    std::wstring old_path = PrependModPaths(mods);
    LoaderLog("prepended %zu mod folder(s) to child PATH", mods.size());

    STARTUPINFOW startup;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_SHOWNORMAL;
    PROCESS_INFORMATION process_info;
    ZeroMemory(&process_info, sizeof(process_info));

    if (!CreateProcessW(
            game_path.c_str(),
            command_line.data(),
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            g_loader_dir.c_str(),
            &startup,
            &process_info)) {
        wchar_t message[512];
        _snwprintf(
            message,
            512,
            L"Could not start acz.exe.\n\nWindows error: %lu\n\nSee mods\\loader.log.",
            GetLastError());
        message[511] = 0;
        if (!old_path.empty()) SetEnvironmentVariableW(L"PATH", old_path.c_str());
        FatalError(message);
        return 1;
    }
    if (!old_path.empty()) SetEnvironmentVariableW(L"PATH", old_path.c_str());
    CloseHandle(process_info.hThread);
    LoaderLog("child pid=%lu", process_info.dwProcessId);

    if (!mods.empty()) {
        uintptr_t base = 0;
        if (WaitForDecryptedOep(process_info.hProcess, process_info.dwProcessId, module_name, &base)) {
            int injected = 0;
            for (const ModInfo& mod : mods) {
                if (InjectDll(process_info.hProcess, process_info.dwProcessId, mod.dll_path)) {
                    ++injected;
                }
                if (WaitForSingleObject(process_info.hProcess, 0) == WAIT_OBJECT_0) {
                    LoaderLog("child exited during injection");
                    break;
                }
            }
            LoaderLog("injection complete: %d/%zu mod(s)", injected, mods.size());
        }
    } else {
        LoaderLog("no enabled mods; continuing with an unmodified game process");
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(process_info.hProcess, &exit_code);
    LoaderLog("child exited with code %lu", exit_code);
    CloseHandle(process_info.hProcess);
    return (int)exit_code;
}
