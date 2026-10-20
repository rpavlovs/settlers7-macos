/* SPDX-License-Identifier: MIT
 * Tests only this disposable process. Requires Wine; no game DLL is needed.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef LONG (WINAPI *NT_QUERY)(HANDLE, ULONG, void *, ULONG, ULONG *);
typedef LONG (WINAPI *NT_SET)(HANDLE, ULONG, void *, ULONG);
typedef LONG (WINAPI *RIB)(ULONG, ULONG);

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!GetProcAddress(ntdll, "wine_get_version")) {
        puts("SKIP: this test requires Wine");
        return 77;
    }
    NT_QUERY query = (NT_QUERY)(void *)GetProcAddress(ntdll, "NtQueryInformationProcess");
    NT_SET set = (NT_SET)(void *)GetProcAddress(ntdll, "NtSetInformationProcess");
    if (!query || !set) return 2;
    ULONG initial, expected;
    SetEnvironmentVariableA("SETTLERS7_DISABLE_DEP_FIX", NULL);
    if (!strcmp(argv[2], "disabled")) { initial = 2; expected = 1; }
    else if (!strcmp(argv[2], "enabled")) { initial = 1; expected = 1; }
    else if (!strcmp(argv[2], "permanent")) { initial = 10; expected = 10; }
    else if (!strcmp(argv[2], "opt-out")) {
        initial = expected = 2;
        SetEnvironmentVariableA("SETTLERS7_DISABLE_DEP_FIX", "1");
    } else return 2;
    if (set(GetCurrentProcess(), 34, &initial, sizeof(initial)) < 0) return 2;
    HMODULE codec = LoadLibraryA(argv[1]);
    RIB entry = codec ? (RIB)(void *)GetProcAddress(codec, "_RIB_Main@8") : NULL;
    if (!entry) { printf("FAIL DLL load/export: %lu\n", GetLastError()); return 1; }
    /* No Miles DLL is loaded, so registration returns 0 after the DEP hook. */
    if (entry(0, 1) != 0) return 1;
    ULONG after = 0;
    if (query(GetCurrentProcess(), 34, &after, sizeof(after), NULL) < 0 || after != expected) {
        printf("FAIL %s initial=%lu expected=%lu actual=%lu\n", argv[2], initial, expected, after);
        return 1;
    }
    if (entry(0, 1) != 0 || query(GetCurrentProcess(), 34, &after, sizeof(after), NULL) < 0 || after != expected)
        return 1;
    printf("PASS %s: flags %lu -> %lu, repeated initialization unchanged\n", argv[2], initial, after);
    FreeLibrary(codec);
    return 0;
}
