/* SPDX-License-Identifier: MIT
 * Process-local workaround for Wine/Rosetta GPU-memory write stalls.
 * The underlying DEP issue is documented by athei and Highball; see CREDITS.md.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *NT_QUERY)(HANDLE, ULONG, void *, ULONG, ULONG *);
typedef LONG (WINAPI *NT_SET)(HANDLE, ULONG, void *, ULONG);
static LONG attempted;

void settlers_enable_dep_once(void) {
    if (InterlockedCompareExchange(&attempted, 1, 0)) return;
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    /* Native Windows is outside the scope of this workaround. */
    if (!ntdll || !GetProcAddress(ntdll, "wine_get_version")) return;
    char disabled[2];
    if (GetEnvironmentVariableA("SETTLERS7_DISABLE_DEP_FIX", disabled,
                                sizeof(disabled)) == 1 && disabled[0] == '1')
        return;
    NT_QUERY query = (NT_QUERY)(void *)GetProcAddress(ntdll, "NtQueryInformationProcess");
    NT_SET set = (NT_SET)(void *)GetProcAddress(ntdll, "NtSetInformationProcess");
    if (!query || !set) return;

    /* ProcessExecuteFlags = 34. ENABLE (2) means execution is unrestricted;
       DISABLE (1) enables DEP. Preserve other flags and never set PERMANENT (8). */
    ULONG before = 0, after = 0;
    LONG status = query(GetCurrentProcess(), 34, &before, sizeof(before), NULL);
    if (status < 0 || (before & 3u) != 2u || (before & 8u)) return;
    ULONG next = (before & ~3u) | 1u;
    status = set(GetCurrentProcess(), 34, &next, sizeof(next));
    LONG checked = query(GetCurrentProcess(), 34, &after, sizeof(after), NULL);
    char report[192];
    snprintf(report, sizeof(report),
             "[settlers7-dep] before=%08lx set_status=%08lx query_status=%08lx after=%08lx\n",
             before, (unsigned long)status, (unsigned long)checked, after);
    OutputDebugStringA(report);
}
