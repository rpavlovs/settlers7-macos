/* SPDX-License-Identifier: MIT. Standalone reproduction; no game implementation included. */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef IDirect3D9 *(WINAPI *create9_fn)(UINT);
typedef void *(__cdecl *memset_fn)(void *, int, size_t);
typedef LONG (WINAPI *nt_set_process_fn)(HANDLE, ULONG, void *, ULONG);
typedef LONG (WINAPI *rib_main_fn)(ULONG, ULONG);
static LARGE_INTEGER frequency;
static volatile LONG finished;
static double now(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / frequency.QuadPart;
}
static DWORD WINAPI watchdog(void *unused) {
    (void)unused;
    for (int n = 0; n < 300 && !finished; ++n) Sleep(100);
    if (!finished) {
        fputs("FAIL isolated graphics benchmark exceeded 30 seconds\n", stderr);
        TerminateProcess(GetCurrentProcess(), 124);
    }
    return 0;
}
static int texture(IDirect3DDevice9 *dev, memset_fn fill, D3DPOOL pool) {
    IDirect3DTexture9 *tex = NULL;
    D3DLOCKED_RECT locked;
    double start = now();
    HRESULT hr = IDirect3DDevice9_CreateTexture(dev, 512, 512, 1, 0,
                                               D3DFMT_A8R8G8B8, pool, &tex, NULL);
    double created = now();
    if (FAILED(hr)) {
        printf("FAIL texture pool=%u create=%08lx\n", pool, hr);
        return 0;
    }
    hr = IDirect3DTexture9_LockRect(tex, 0, &locked, NULL, 0);
    double mapped = now();
    if (FAILED(hr)) {
        printf("FAIL texture pool=%u lock=%08lx\n", pool, hr);
        IDirect3DTexture9_Release(tex);
        return 0;
    }
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(locked.pBits, &mbi, sizeof(mbi));
    double fill_start = now();
    fill(locked.pBits, 0, (size_t)locked.Pitch * 512);
    double filled = now();
    hr = IDirect3DTexture9_UnlockRect(tex, 0);
    double unlocked = now();
    printf("TEXTURE pool=%u address=%p pitch=%d state=%lx protect=%lx type=%lx create_ms=%.6f lock_ms=%.6f memset_ms=%.6f unlock_ms=%.6f hr=%08lx\n",
           pool, locked.pBits, locked.Pitch, mbi.State, mbi.Protect, mbi.Type,
           (created-start)*1000, (mapped-created)*1000, (filled-fill_start)*1000,
           (unlocked-filled)*1000, hr);
    IDirect3DTexture9_Release(tex);
    return SUCCEEDED(hr);
}
static int buffer(IDirect3DDevice9 *dev, memset_fn fill, D3DPOOL pool) {
    IDirect3DVertexBuffer9 *buf = NULL;
    void *data = NULL;
    double start = now();
    HRESULT hr = IDirect3DDevice9_CreateVertexBuffer(dev, 0x1160, 0, 0,
                                                    pool, &buf, NULL);
    double created = now();
    if (FAILED(hr)) {
        printf("FAIL buffer pool=%u create=%08lx\n", pool, hr);
        return 0;
    }
    hr = IDirect3DVertexBuffer9_Lock(buf, 0, 0x1160, &data, 0);
    double mapped = now();
    if (SUCCEEDED(hr)) fill(data, 0, 0x1160);
    double filled = now();
    if (SUCCEEDED(hr)) hr = IDirect3DVertexBuffer9_Unlock(buf);
    printf("BUFFER pool=%u address=%p create_ms=%.6f lock_ms=%.6f memset_ms=%.6f hr=%08lx\n",
           pool, data, (created-start)*1000, (mapped-created)*1000,
           (filled-mapped)*1000, hr);
    IDirect3DVertexBuffer9_Release(buf);
    return SUCCEEDED(hr);
}
int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    QueryPerformanceFrequency(&frequency);
    HANDLE timer = CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    DWORD dep_flags = 0;
    BOOL dep_permanent = FALSE;
    BOOL dep_ok = GetProcessDEPPolicy(GetCurrentProcess(), &dep_flags, &dep_permanent);
    printf("DEP initial query_ok=%d flags=%08lx permanent=%d\n",
           dep_ok, dep_flags, dep_permanent);
    if (argc == 2 && !strcmp(argv[1], "dep-off")) {
        SetLastError(0);
        BOOL changed = SetProcessDEPPolicy(0);
        DWORD change_error = GetLastError();
        dep_ok = GetProcessDEPPolicy(GetCurrentProcess(), &dep_flags, &dep_permanent);
        printf("DEP disable_ok=%d error=%lu query_ok=%d flags=%08lx permanent=%d\n",
               changed, change_error, dep_ok, dep_flags, dep_permanent);
        if (!changed || !dep_ok || dep_flags) return 3;
    }
    if (argc >= 2 && !strcmp(argv[1], "nt-dep-off")) {
        /* Wine 11: ProcessExecuteFlags=34, MEM_EXECUTE_OPTION_ENABLE=2.
           This deliberately changes only this disposable test process. */
        nt_set_process_fn nt_set = (nt_set_process_fn)(void *)GetProcAddress(
            GetModuleHandleA("ntdll.dll"), "NtSetInformationProcess");
        ULONG execute_flags = 2;
        if (!nt_set) return 3;
        LONG status = nt_set(GetCurrentProcess(), 34, &execute_flags, sizeof(execute_flags));
        dep_ok = GetProcessDEPPolicy(GetCurrentProcess(), &dep_flags, &dep_permanent);
        printf("DEP native_disable_status=%08lx query_ok=%d flags=%08lx permanent=%d\n",
               (unsigned long)status, dep_ok, dep_flags, dep_permanent);
        if (status || !dep_ok || dep_flags) return 3;
    }
    if (argc == 3 && !strcmp(argv[1], "nt-dep-off")) {
        HMODULE asi = LoadLibraryA(argv[2]);
        rib_main_fn rib_main = asi ? (rib_main_fn)(void *)GetProcAddress(
            asi, "_RIB_Main@8") : NULL;
        if (!rib_main) {
            printf("FAIL ASI load or entry point error=%lu\n", GetLastError());
            return 3;
        }
        LONG registered = rib_main(0, 1);
        dep_ok = GetProcessDEPPolicy(GetCurrentProcess(), &dep_flags, &dep_permanent);
        printf("DEP after_ASI registration=%ld query_ok=%d flags=%08lx permanent=%d\n",
               registered, dep_ok, dep_flags, dep_permanent);
        /* Registration may fail because this graphics test does not load Miles. */
        if (!dep_ok || !(dep_flags & PROCESS_DEP_ENABLE) || dep_permanent) return 3;
    }
    HMODULE lib = LoadLibraryA("d3d9.dll");
    HMODULE crt = LoadLibraryA("msvcrt.dll");
    create9_fn create = (create9_fn)(void *)GetProcAddress(lib, "Direct3DCreate9");
    memset_fn fill = (memset_fn)(void *)GetProcAddress(crt, "memset");
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(lib, path, sizeof(path));
    printf("d3d9=%s memset_rva=%lx\n", path,
           (unsigned long)((uintptr_t)fill - (uintptr_t)crt));
    if (!timer || !create || !fill) return 2;
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "GraphicsMemoryBenchmark";
    if (!RegisterClassA(&wc)) return 2;
    HWND window = CreateWindowExA(0, wc.lpszClassName, "Graphics memory benchmark",
                                  WS_OVERLAPPED, 0, 0, 32, 32, NULL, NULL,
                                  wc.hInstance, NULL);
    IDirect3D9 *d3d = create(D3D_SDK_VERSION);
    if (!window || !d3d) return 2;
    D3DPRESENT_PARAMETERS pp = {0};
    pp.BackBufferWidth = pp.BackBufferHeight = 32;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9 *dev = NULL;
    double start = now();
    HRESULT hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                                        window, D3DCREATE_HARDWARE_VERTEXPROCESSING |
                                        D3DCREATE_FPU_PRESERVE, &pp, &dev);
    printf("DEVICE hr=%08lx elapsed_ms=%.3f\n", hr, (now()-start)*1000);
    if (FAILED(hr)) return 2;
    int ok = 1;
    for (int round = 0; round < 2; ++round) {
        printf("ROUND %d\n", round);
        ok &= texture(dev, fill, D3DPOOL_MANAGED);
        ok &= texture(dev, fill, D3DPOOL_SYSTEMMEM);
        ok &= buffer(dev, fill, D3DPOOL_MANAGED);
        ok &= buffer(dev, fill, D3DPOOL_DEFAULT);
    }
    puts("READY_FOR_LIBRARY_CHECK");
    Sleep(4000);
    IDirect3DDevice9_Release(dev);
    IDirect3D9_Release(d3d);
    DestroyWindow(window);
    InterlockedExchange(&finished, 1);
    WaitForSingleObject(timer, 1000);
    CloseHandle(timer);
    printf("RESULT %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
