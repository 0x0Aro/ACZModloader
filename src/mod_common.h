// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 0x00Aro

#pragma once

// Minimal INI reader used by ACZModLoader. This file intentionally contains
// no game-specific or mod-specific hook helpers.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>

inline int ModIeq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<char>(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<char>(cb - 'A' + 'a');
        if (ca != cb) return 0;
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

inline void ModTrim(char* text) {
    char* end = text + std::strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = 0;
    }
    while (*text == ' ' || *text == '\t') {
        std::memmove(text, text + 1, std::strlen(text));
    }
}

inline bool ModReadIniString(const wchar_t* path, const char* key, char* out, size_t capacity) {
    if (!out || capacity == 0) return false;
    out[0] = 0;
    HANDLE handle = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    char body[8192];
    DWORD read = 0;
    ReadFile(handle, body, sizeof(body) - 1, &read, nullptr);
    CloseHandle(handle);
    body[read] = 0;

    char* line = body;
    while (line && *line) {
        char* newline = std::strchr(line, '\n');
        if (newline) *newline = 0;
        ModTrim(line);
        if (line[0] && line[0] != ';' && line[0] != '#' && line[0] != '[') {
            char* equals = std::strchr(line, '=');
            if (equals) {
                *equals = 0;
                ModTrim(line);
                ModTrim(equals + 1);
                if (ModIeq(line, key)) {
                    std::strncpy(out, equals + 1, capacity - 1);
                    out[capacity - 1] = 0;
                    return true;
                }
            }
        }
        line = newline ? newline + 1 : nullptr;
    }
    return false;
}

inline bool ModIniEnabled(const wchar_t* path) {
    char value[32];
    if (!ModReadIniString(path, "enabled", value, sizeof(value))) return true;
    return !(ModIeq(value, "0") || ModIeq(value, "false") ||
             ModIeq(value, "off") || ModIeq(value, "no"));
}
