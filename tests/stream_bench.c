/* SPDX-License-Identifier: MIT. Standalone reproduction; no game implementation included. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef LONG (__stdcall *START)(void);
typedef void (__stdcall *STOP)(void);
typedef char *(__stdcall *REDIST)(const char *);
typedef char *(__stdcall *LAST_ERROR)(void);
typedef void *(__stdcall *OPEN_DRIVER)(DWORD,LONG,LONG,DWORD);
typedef void (__stdcall *CLOSE_DRIVER)(void *);
typedef void (__stdcall *VOLUME)(void *,float);
typedef void *(__stdcall *OPEN_STREAM)(void *,const char *,LONG);
typedef void (__stdcall *STREAM_VOID)(void *);
typedef LONG (__stdcall *STREAM_STATUS)(void *);
typedef void (__stdcall *STREAM_POSITION)(void *,LONG *,LONG *);

static LARGE_INTEGER freq;
static double clock_seconds(void) {
    LARGE_INTEGER v; QueryPerformanceCounter(&v);
    return (double)v.QuadPart/freq.QuadPart;
}
static DWORD WINAPI watchdog(void *unused) {
    (void)unused; Sleep(30000);
    puts("Watchdog: standalone benchmark exceeded 30 seconds");
    ExitProcess(124);
    return 0;
}
int main(int argc,char **argv) {
    setvbuf(stdout,NULL,_IONBF,0);
    if(argc!=3 && argc!=4) {puts("Usage: stream-bench original-binary-directory audio-file [stream-count]");return 2;}
    int count=argc==4?atoi(argv[3]):1;
    if(count<1||count>32)return 2;
    QueryPerformanceFrequency(&freq);
    HANDLE watch=CreateThread(NULL,0,watchdog,NULL,0,NULL);
    if(watch) CloseHandle(watch);
    SetDllDirectoryA(argv[1]);
    char dll[MAX_PATH];snprintf(dll,sizeof dll,"%s\\mss32.dll",argv[1]);
    HMODULE m=LoadLibraryA(dll);
    if(!m){printf("LoadLibrary error=%lu\n",GetLastError());return 2;}
#define LOAD(type,name,export) type name=(type)(void *)GetProcAddress(m,export); if(!name){puts("Missing " export);return 2;}
    LOAD(START,start,"_AIL_startup@0")
    LOAD(STOP,stop,"_AIL_shutdown@0")
    LOAD(REDIST,redist,"_AIL_set_redist_directory@4")
    LOAD(LAST_ERROR,last,"_AIL_last_error@0")
    LOAD(OPEN_DRIVER,open_driver,"_AIL_open_digital_driver@16")
    LOAD(CLOSE_DRIVER,close_driver,"_AIL_close_digital_driver@4")
    LOAD(VOLUME,volume,"_AIL_set_digital_master_volume_level@8")
    LOAD(OPEN_STREAM,open_stream,"_AIL_open_stream@12")
    LOAD(STREAM_VOID,close_stream,"_AIL_close_stream@4")
    LOAD(STREAM_VOID,start_stream,"_AIL_start_stream@4")
    LOAD(STREAM_STATUS,status,"_AIL_stream_status@4")
    LOAD(STREAM_POSITION,position,"_AIL_stream_ms_position@12")
    redist(argv[1]);
    printf("startup=%ld\n",start());
    void *driver=open_driver(44100,16,2,0);
    if(!driver){printf("Open driver failed: %s\n",last());stop();return 2;}
    volume(driver,0.0f);
    void *streams[32]={0};
    for(int i=0;i<count;i++) {
        streams[i]=open_stream(driver,argv[2],0);
        if(!streams[i]){printf("Open stream %d failed: %s\n",i,last());return 2;}
    }
    void *stream=streams[0];
    double begin=clock_seconds();
    for(int i=0;i<count;i++)start_stream(streams[i]);
    printf("started=%d startup_s=%.3f\n",count,clock_seconds()-begin);
    double max_query=0,next_report=0;
    LONG total=0,current=0,state=0;
    unsigned polls=0;
    while(clock_seconds()-begin<14.0) {
        double before=clock_seconds();
        position(stream,&total,&current);
        state=status(stream);
        double after=clock_seconds(),query=after-before,elapsed=after-begin;
        polls++;
        if(query>max_query)max_query=query;
        if(elapsed>=next_report || query>0.1) {
            printf("elapsed=%.3f current_ms=%ld total_ms=%ld status=%ld query_ms=%.3f\n",
                   elapsed,current,total,state,query*1000);
            next_report=elapsed+1.0;
        }
        if((state&2) && elapsed>0.5)break;
        Sleep(20);
    }
    printf("RESULT elapsed=%.3f current_ms=%ld total_ms=%ld status=%ld polls=%u max_query_ms=%.3f\n",
           clock_seconds()-begin,current,total,state,polls,max_query*1000);
    for(int i=0;i<count;i++)close_stream(streams[i]);
    close_driver(driver);stop();FreeLibrary(m);
    return (state&2)?0:3;
}
