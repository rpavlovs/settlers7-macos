// SPDX-License-Identifier: MIT
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

static unsigned long number(const char *s)
{
    char *end = NULL;
    errno = 0;
    unsigned long n = strtoul(s, &end, 10);
    return !errno && s != end && end && !*end ? n : 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    unsigned long width = number(argv[1]), height = number(argv[2]);
    if (width < 320 || width > 8192 || height < 240 || height > 8192) return 2;
    HWND window = FindWindowA(NULL, "Settlers 7 Window");
    if (!window || IsIconic(window)) return 3;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return 4;
    char path[32768];
    DWORD length = sizeof(path);
    BOOL identified = QueryFullProcessImageNameA(process, 0, path, &length);
    CloseHandle(process);
    const char *expected = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\The Settlers 7 - History Edition\\Data\\Base\\Bin\\Settlers7R.exe";
    if (!identified || _stricmp(path, expected)) return 4;
    MONITORINFO monitor = {0};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoA(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor) ||
        !(monitor.dwFlags & MONITORINFOF_PRIMARY)) return 5;
    RECT outer, client;
    POINT origin = {0, 0};
    if (!GetWindowRect(window, &outer) || !GetClientRect(window, &client) ||
        !ClientToScreen(window, &origin)) return 6;
    int extraWidth = (outer.right - outer.left) - (client.right - client.left);
    int extraHeight = (outer.bottom - outer.top) - (client.bottom - client.top);
    if (width > (unsigned long)(monitor.rcWork.right - monitor.rcWork.left) ||
        height > (unsigned long)(monitor.rcWork.bottom - monitor.rcWork.top) ||
        extraWidth < 0 || extraWidth > 100 || extraHeight < 0 || extraHeight > 200) return 6;
    DWORD currentPid = 0;
    GetWindowThreadProcessId(window, &currentPid);
    if (currentPid != pid) return 7;
    if (!SetWindowPos(window, NULL, monitor.rcWork.left - (origin.x - outer.left),
        monitor.rcWork.top - (origin.y - outer.top), (int)width + extraWidth, (int)height + extraHeight,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED)) return 8;
    if (!GetClientRect(window, &client) || client.right != (LONG)width || client.bottom != (LONG)height) return 9;
    printf("[fullscreen-launch] guest-client=%ldx%ld\n", client.right, client.bottom);
    return 0;
}
