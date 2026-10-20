/* SPDX-License-Identifier: MIT */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

typedef LONG (WINAPI *spawn_fn)(char * const[], int);
typedef char *(__cdecl *unix_path_fn)(LPCWSTR);

static int failure(const wchar_t *message) {
    MessageBoxW(NULL, message, L"Settlers 7 on Mac", MB_OK | MB_ICONERROR);
    return 1;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command, int show) {
    (void)instance; (void)previous; (void)command; (void)show;
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2 || argc > 64)
        return failure(L"Start the game with Play in Steam.");
    spawn_fn spawn = (spawn_fn)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "__wine_unix_spawnvp");
    unix_path_fn unix_path = (unix_path_fn)(void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "wine_get_unix_file_name");
    if (!spawn || !unix_path) return failure(L"This launcher requires CrossOver.");
    wchar_t module[32768];
    DWORD length = GetModuleFileNameW(NULL, module, 32768);
    if (!length || length >= 32768) return failure(L"Cannot locate the compatibility launcher.");
    wchar_t *slash = wcsrchr(module, L'\\');
    if (!slash || (size_t)(slash - module) + 11 >= 32768)
        return failure(L"Cannot locate the compatibility folder.");
    wcscpy(slash + 1, L"launch.sh");
    if (GetFileAttributesW(module) == INVALID_FILE_ATTRIBUTES)
        return failure(L"The compatibility launch script is missing.");
    char *script = unix_path(module);
    if (!script) return failure(L"Cannot resolve the compatibility folder.");
    char *native[67] = { "/bin/sh", script, NULL };
    for (int i = 1; i < argc; ++i) {
        int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], -1, NULL, 0, NULL, NULL);
        if (!bytes) return failure(L"A launch argument is not valid Unicode.");
        native[i + 1] = HeapAlloc(GetProcessHeap(), 0, (SIZE_T)bytes);
        if (!native[i + 1] || !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                argv[i], -1, native[i + 1], bytes, NULL, NULL))
            return failure(L"Cannot prepare the game launch.");
    }
    LONG status = spawn(native, 1);
    for (int i = 1; i < argc; ++i) HeapFree(GetProcessHeap(), 0, native[i + 1]);
    HeapFree(GetProcessHeap(), 0, script);
    LocalFree(argv);
    if (status) return failure(L"The game did not finish normally. Its local compatibility log contains details.");
    return 0;
}
