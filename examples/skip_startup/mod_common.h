#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

struct ModLogger {
    HANDLE handle = INVALID_HANDLE_VALUE;

    void Open(const wchar_t* path) {
        handle = CreateFileW(
            path,
            GENERIC_WRITE,
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
    }

    void Write(const char* fmt, ...) {
        char buffer[2048];
        va_list args;
        va_start(args, fmt);
        int count = vsnprintf(buffer, sizeof(buffer) - 2, fmt, args);
        va_end(args);
        if (count < 0) {
            return;
        }
        buffer[count] = '\n';
        buffer[count + 1] = 0;
        if (handle != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(handle, buffer, (DWORD)strlen(buffer), &written, NULL);
        }
        OutputDebugStringA(buffer);
    }
};

inline int ModIeq(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        ++a;
        ++b;
    }
    return *a == 0 && *b == 0;
}

inline void ModTrim(char* text) {
    char* end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
        *--end = 0;
    }
    while (*text == ' ' || *text == '\t') {
        memmove(text, text + 1, strlen(text));
    }
}

inline bool ModReadIniString(const wchar_t* path, const char* key, char* out, size_t capacity) {
    if (!out || capacity == 0) return false;
    out[0] = 0;
    HANDLE handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE) return false;
    char body[8192];
    DWORD read = 0;
    ReadFile(handle, body, sizeof(body) - 1, &read, NULL);
    CloseHandle(handle);
    body[read] = 0;
    char* line = body;
    while (line && *line) {
        char* newline = strchr(line, '\n');
        if (newline) *newline = 0;
        ModTrim(line);
        if (line[0] && line[0] != ';' && line[0] != '#' && line[0] != '[') {
            char* equals = strchr(line, '=');
            if (equals) {
                *equals = 0;
                ModTrim(line);
                ModTrim(equals + 1);
                if (ModIeq(line, key)) {
                    strncpy(out, equals + 1, capacity - 1);
                    out[capacity - 1] = 0;
                    return true;
                }
            }
        }
        line = newline ? newline + 1 : NULL;
    }
    return false;
}

inline void ModJoinPath(wchar_t* out, DWORD capacity, const wchar_t* left, const wchar_t* right) {
    _snwprintf(out, capacity, L"%s\\%s", left, right);
    out[capacity - 1] = 0;
}

inline bool ModInstallAbsoluteJumpHook(void* target, void* hook, void** trampoline, size_t patch_length) {
    if (!target || !hook || !trampoline || patch_length < 14) {
        return false;
    }
    BYTE* thunk = (BYTE*)VirtualAlloc(
        NULL,
        patch_length + 16,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
    if (!thunk) {
        return false;
    }

    memcpy(thunk, target, patch_length);
    BYTE* jump = thunk + patch_length;
    jump[0] = 0xFF;
    jump[1] = 0x25;
    *(DWORD*)(jump + 2) = 0;
    *(void**)(jump + 6) = (BYTE*)target + patch_length;

    DWORD old_protect = 0;
    if (!VirtualProtect(target, patch_length, PAGE_EXECUTE_READWRITE, &old_protect)) {
        VirtualFree(thunk, 0, MEM_RELEASE);
        return false;
    }
    BYTE* destination = (BYTE*)target;
    destination[0] = 0xFF;
    destination[1] = 0x25;
    *(DWORD*)(destination + 2) = 0;
    *(void**)(destination + 6) = hook;
    for (size_t i = 14; i < patch_length; ++i) {
        destination[i] = 0x90;
    }
    VirtualProtect(target, patch_length, old_protect, &old_protect);
    FlushInstructionCache(GetCurrentProcess(), target, patch_length);
    *trampoline = thunk;
    return true;
}
