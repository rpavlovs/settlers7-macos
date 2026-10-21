#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Owned-process-only test: no external PID or handle is accepted. */
typedef LONG (WINAPI *nt_wait_fn)(const void *, const LARGE_INTEGER *);
typedef LONG (WINAPI *nt_alert_fn)(HANDLE);
typedef LONG (WINAPI *rtl_wait_fn)(const void *, const void *, SIZE_T, const LARGE_INTEGER *);
typedef void (WINAPI *rtl_wake_fn)(const void *);
#define STATUS_OK ((LONG)0)
#define STATUS_ALERTED_VALUE ((LONG)0x101)
#define STATUS_TIMEOUT_VALUE ((LONG)0x102)

static nt_wait_fn original_wait;
static nt_alert_fn nt_alert;
static rtl_wait_fn rtl_wait;
static rtl_wake_fn rtl_wake;
static BYTE *stub, saved[16];
static HANDLE ready_event, consumed_event, done_event;
static volatile LONG target_tid, first_call, predicate, injection_mode;
static volatile LONG consume_status, worker_status, timeout_present;
static LONGLONG first_timeout;
static ULONGLONG worker_elapsed;
static int finite_mode;

static LONG WINAPI intercepted_wait(const void *addr, const LARGE_INTEGER *timeout)
{
    if (GetCurrentThreadId() == (DWORD)target_tid &&
        InterlockedCompareExchange(&first_call, 1, 0) == 0)
    {
        timeout_present = timeout != NULL;
        first_timeout = timeout ? timeout->QuadPart : 0;
        SetEvent(ready_event);
        if (injection_mode)
        {
            LARGE_INTEGER consume_limit;
            consume_limit.QuadPart = -10000000; /* 1 second, only this test. */
            /* The real RtlWaitOnAddress has already queued its stack entry. */
            consume_status = original_wait(NULL, &consume_limit);
            SetEvent(consumed_event);
            if (consume_status != STATUS_ALERTED_VALUE)
                return consume_status;
        }
    }
    return original_wait(addr, timeout);
}

static int write_stub(const BYTE bytes[5])
{
    DWORD old, ignored;
    if (!VirtualProtect(stub, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(stub, bytes, 5);
    BOOL flushed = FlushInstructionCache(GetCurrentProcess(), stub, 5);
    BOOL restored = VirtualProtect(stub, 5, old, &ignored);
    return flushed && restored;
}

static int install_hook(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    stub = (BYTE *)GetProcAddress(ntdll, "NtWaitForAlertByThreadId");
    nt_alert = (nt_alert_fn)(void *)GetProcAddress(ntdll, "NtAlertThreadByThreadId");
    rtl_wait = (rtl_wait_fn)(void *)GetProcAddress(ntdll, "RtlWaitOnAddress");
    rtl_wake = (rtl_wake_fn)(void *)GetProcAddress(ntdll, "RtlWakeAddressSingle");
    if (!stub || !nt_alert || !rtl_wait || !rtl_wake) return 0;
    /* Exact local Wine PE32 syscall stub: mov eax,imm; mov edx,abs;
       call edx; ret 8; nop. All copied operands are absolute, not PC-relative. */
    if (stub[0] != 0xb8 || stub[5] != 0xba || stub[10] != 0xff ||
        stub[11] != 0xd2 || stub[12] != 0xc2 || stub[13] != 8 ||
        stub[14] != 0 || stub[15] != 0x90)
    {
        puts("REFUSED: unexpected syscall stub byte pattern");
        return 0;
    }
    DWORD syscall_id, dispatcher;
    memcpy(&syscall_id, stub + 1, 4);
    memcpy(&dispatcher, stub + 6, 4);
    MEMORY_BASIC_INFORMATION mbi;
    if (syscall_id != 0x105 || !VirtualQuery((void *)(uintptr_t)dispatcher, &mbi, sizeof(mbi)) ||
        mbi.AllocationBase != ntdll || !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
    {
        puts("REFUSED: syscall number or dispatcher does not match verified Wine build");
        return 0;
    }
    memcpy(saved, stub, sizeof(saved));
    BYTE *trampoline = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!trampoline) return 0;
    memcpy(trampoline, saved, sizeof(saved));
    DWORD old;
    if (!VirtualProtect(trampoline, 4096, PAGE_EXECUTE_READ, &old) ||
        !FlushInstructionCache(GetCurrentProcess(), trampoline, sizeof(saved))) return 0;
    original_wait = (nt_wait_fn)trampoline;
    BYTE jump[5] = {0xe9};
    DWORD relative = (DWORD)(uintptr_t)intercepted_wait - ((DWORD)(uintptr_t)stub + 5);
    memcpy(jump + 1, &relative, sizeof(relative));
    printf("verified_stub syscall=%03lx copied_bytes=16; temporary hook is process-local\n", syscall_id);
    return write_stub(jump);
}

static DWORD WINAPI worker(void *unused)
{
    (void)unused;
    LONG expected = 0;
    LARGE_INTEGER finite;
    finite.QuadPart = -600000; /* 60 ms, original finite timeout remains exact. */
    ULONGLONG began = GetTickCount64();
    worker_status = rtl_wait((const void *)&predicate, &expected, sizeof(expected), finite_mode ? &finite : NULL);
    worker_elapsed = GetTickCount64() - began;
    SetEvent(done_event);
    return 0;
}

static int run_case(const char *name, int inject, int finite, int expect_candidate)
{
    ResetEvent(ready_event);
    ResetEvent(consumed_event);
    ResetEvent(done_event);
    predicate = 0;
    first_call = 0;
    timeout_present = 0;
    first_timeout = 0;
    injection_mode = inject;
    finite_mode = finite;
    consume_status = worker_status = (LONG)0xdeadbeef;
    DWORD tid;
    HANDLE thread = CreateThread(NULL, 0, worker, NULL, CREATE_SUSPENDED, &tid);
    if (!thread) return 0;
    target_tid = (LONG)tid;
    if (ResumeThread(thread) == (DWORD)-1) return 0;
    DWORD ready = WaitForSingleObject(ready_event, 1500);
    if (ready != WAIT_OBJECT_0)
    {
        printf("%s FAIL: wrapper was not reached, wait=%lu\n", name, ready);
        return 0;
    }
    ULONGLONG wake_time = GetTickCount64();
    int wakes = 0, recovery = 0;
    LONG alert_status = 0;
    if (!finite)
    {
        InterlockedExchange(&predicate, 1);
        rtl_wake((const void *)&predicate); /* Exactly one normal address wake. */
        wakes = 1;
    }
    if (inject && WaitForSingleObject(consumed_event, 1500) != WAIT_OBJECT_0)
    {
        printf("%s FAIL: injection did not consume the wake\n", name);
        return 0;
    }
    DWORD initial_done = WaitForSingleObject(done_event, 500);
    ULONGLONG observed_ms = GetTickCount64() - wake_time;
    if (initial_done == WAIT_TIMEOUT)
    {
        recovery = 1;
        /* Own worker only; a legitimate alert recovers the deliberately lost wake. */
        alert_status = nt_alert((HANDLE)(uintptr_t)tid);
    }
    DWORD final_done = WaitForSingleObject(done_event, 1500);
    DWORD joined = WaitForSingleObject(thread, 500);
    int ok = final_done == WAIT_OBJECT_0 && joined == WAIT_OBJECT_0;
    if (finite)
        ok = ok && worker_status == STATUS_TIMEOUT_VALUE && timeout_present &&
             first_timeout == -600000 && !recovery && worker_elapsed >= 40 && worker_elapsed < 500;
    else if (!inject)
        ok = ok && worker_status == STATUS_OK && !recovery;
    else
        ok = ok && worker_status == STATUS_OK && consume_status == STATUS_ALERTED_VALUE &&
             (expect_candidate ? (!recovery && timeout_present && first_timeout == -1000000 && observed_ms < 400)
                               : (recovery && !timeout_present && alert_status == STATUS_OK));
    printf("%s result=%s first_timeout=%s%lld consume=%08lx normal_wakes=%d observed_ms=%llu recovery_alerts=%d recovery_status=%08lx rtl_status=%08lx elapsed_ms=%llu joined=%lu\n",
           name, ok ? "PASS" : "FAIL", timeout_present ? "" : "NULL/", first_timeout,
           (DWORD)consume_status, wakes, (unsigned long long)observed_ms, recovery,
           (DWORD)alert_status, (DWORD)worker_status, (unsigned long long)worker_elapsed, joined);
    CloseHandle(thread);
    return ok;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc != 2 || (strcmp(argv[1], "baseline") && strcmp(argv[1], "candidate")))
    {
        puts("usage: settlers-alert-loss-test.exe baseline|candidate");
        return 2;
    }
    int candidate = !strcmp(argv[1], "candidate");
    printf("owned alert-loss test pid=%lu expected=%s\n", GetCurrentProcessId(), argv[1]);
    ready_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    consumed_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    done_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ready_event || !consumed_event || !done_event || !install_hook()) return 3;
    int ok = run_case("normal-address-wake", 0, 0, candidate);
    if (ok) ok = run_case("finite-timeout", 0, 1, candidate);
    if (ok) ok = run_case("selected-entry-consumed-alert", 1, 0, candidate);
    /* Restore original stub even when a check fails. The whole process is owned. */
    int restored = write_stub(saved) && !memcmp(stub, saved, sizeof(saved));
    printf("stub_restored=%d result=%s\n", restored, ok && restored ? "PASS" : "FAIL");
    if (!ok || !restored)
    {
        TerminateProcess(GetCurrentProcess(), 4);
        return 4;
    }
    return 0;
}
