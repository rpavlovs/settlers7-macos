#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef LONG (WINAPI *nt_query_fn)(HANDLE, ULONG, void *, ULONG, ULONG *);
typedef LONG (WINAPI *nt_set_fn)(HANDLE, ULONG, void *, ULONG);
static nt_query_fn nt_query;
static nt_set_fn nt_set;
static void *active_page;
static volatile DWORD faults, fault_kind, fault_addr;
void *resume_ip, *resume_esp;
extern DWORD try_code(void *entry);

static LONG WINAPI catch_fault(EXCEPTION_POINTERS *ep)
{
    EXCEPTION_RECORD *r = ep->ExceptionRecord;
    uintptr_t addr = (uintptr_t)r->ExceptionAddress;
    if (active_page && r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        r->NumberParameters >= 2 && r->ExceptionInformation[0] == 8 &&
        addr >= (uintptr_t)active_page && addr < (uintptr_t)active_page + 6 &&
        r->ExceptionInformation[1] == addr)
    {
        ++faults;
        fault_kind = (DWORD)r->ExceptionInformation[0];
        fault_addr = (DWORD)addr;
        ep->ContextRecord->Eip = (DWORD)(uintptr_t)resume_ip;
        ep->ContextRecord->Esp = (DWORD)(uintptr_t)resume_esp;
        ep->ContextRecord->Eax = 0xdead0008;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static int dep(ULONG bits)
{
    ULONG before=0, after=0;
    LONG q = nt_query(GetCurrentProcess(), 34, &before, sizeof(before), NULL);
    if (q || (before & 8)) { printf("DEP refused query=%08lx flags=%08lx\n", (DWORD)q,before); return 0; }
    ULONG next = (before & ~3u) | bits;
    LONG s = nt_set(GetCurrentProcess(), 34, &next, sizeof(next));
    q = nt_query(GetCurrentProcess(), 34, &after, sizeof(after), NULL);
    printf("DEP request=%lu before=%08lx set=%08lx after=%08lx query=%08lx\n",bits,before,(DWORD)s,after,(DWORD)q);
    return !s && !q && (after&3u)==bits;
}

static int call_case(const char *name, void *page, int expect_fault)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(page,&mbi,sizeof(mbi))) return 0;
    faults=0; fault_kind=0; fault_addr=0;
    active_page=page;
    DWORD result=try_code(page);
    active_page=NULL;
    int okay=expect_fault ? faults==1 && result==0xdead0008 : !faults && result==0x12345678;
    printf("CASE %s prot=%08lx result=%08lx execute_faults=%lu fault_kind=%lu address=%08lx expected=%s check=%s\n",
        name,mbi.Protect,result,faults,fault_kind,fault_addr,expect_fault?"deny":"execute",okay?"PASS":"FAIL");
    return okay;
}

int main(void)
{
    setvbuf(stdout,NULL,_IONBF,0);
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    HMODULE ntdll=GetModuleHandleA("ntdll.dll");
    nt_query=(nt_query_fn)(void *)GetProcAddress(ntdll,"NtQueryInformationProcess");
    nt_set=(nt_set_fn)(void *)GetProcAddress(ntdll,"NtSetInformationProcess");
    PVOID veh=AddVectoredExceptionHandler(1,catch_fault);
    if (!nt_query||!nt_set||!veh) return 2;
    printf("owned DEP transition test pid=%lu\n",GetCurrentProcessId());
    if (!dep(1)) return 3;
    unsigned char *control=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    unsigned char *promoted=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    unsigned char *nonexec=VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if (!control||!promoted||!nonexec) return 4;
    const BYTE code[]={0xb8,0x78,0x56,0x34,0x12,0xc3}; /* mov eax,12345678h; ret */
    memcpy(control,code,sizeof(code)); memcpy(promoted,code,sizeof(code)); memcpy(nonexec,code,sizeof(code));
    DWORD old;
    if (!VirtualProtect(control,4096,PAGE_EXECUTE_READ,&old)) return 5;
    FlushInstructionCache(GetCurrentProcess(),NULL,0);
    if (!call_case("RX-control-before",control,0) || !dep(2)) return 6;
    if (!VirtualProtect(promoted,4096,PAGE_EXECUTE_READ,&old)) return 7;
    FlushInstructionCache(GetCurrentProcess(),NULL,0);
    int before=call_case("RX-control-DEP-off",control,0);
    before &= call_case("RW-to-explicit-RX-DEP-off",promoted,0);
    before &= call_case("RW-only-DEP-off",nonexec,0);
    if (!before || !dep(1)) return 8;
    int rx=call_case("RX-control-DEP-on",control,0);
    int transition=call_case("RW-to-explicit-RX-DEP-on",promoted,0);
    int denied=call_case("RW-only-DEP-on",nonexec,1);
    printf("SUMMARY control=%s explicit_RX_survives=%s real_nonexec_denied=%s result=%s\n",
        rx?"PASS":"FAIL",transition?"PASS":"FAIL",denied?"PASS":"FAIL",rx&&transition&&denied?"PASS":"FAIL");
    RemoveVectoredExceptionHandler(veh);
    VirtualFree(control,0,MEM_RELEASE); VirtualFree(promoted,0,MEM_RELEASE); VirtualFree(nonexec,0,MEM_RELEASE);
    return rx&&transition&&denied?0:1;
}
