#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <stdint.h>
#include <stdio.h>
#include <share.h>

/* Buttons only. All other mai2io exports forward to the installed mai2hook. */
static HRESULT (*base_init)(void), (*base_poll)(void);
static void (*base_game)(uint16_t *, uint16_t *), (*base_op)(uint8_t *);
static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
static volatile LONG physical, coin_pending, report_count, connected;
static volatile LONG last_report;
static uint16_t game1, game2;
static uint8_t operators;
static uint32_t previous_output = 0xffffffff;
static FILE *logfile;

static void log_status(const char *s) {
    if (logfile) { fprintf(logfile, "%llu %s\n", GetTickCount64(), s); fflush(logfile); }
}

/* Confirmed against the local controller capture: active-low ring, active-high side. */
__declspec(dllexport) uint32_t adx_decode_report(const uint8_t *r, uint32_t n) {
    static const uint16_t ring[] = {4,8,1,0x8000,0x4000,0x2000,0x1000,0x800};
    uint32_t out = 0;
    if (n != 64 || r[0] != 1) return 0;
    uint16_t raw = r[29] | ((uint16_t)r[30] << 8);
    for (unsigned i=0; i<8; i++) if (!(raw & ring[i])) out |= 1u << i;
    if (raw & 2) out |= 0x100;
    if (raw & 0x200) out |= 0x10000; /* TEST */
    if (raw & 0x40) out |= 0x20000; /* SERVICE */
    return out;
}

static HANDLE open_controller(void) {
    GUID guid; HidD_GetHidGuid(&guid);
    HDEVINFO list = SetupDiGetClassDevsW(&guid, NULL, NULL, DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if (list == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;
    HANDLE found = INVALID_HANDLE_VALUE;
    SP_DEVICE_INTERFACE_DATA iface = {sizeof(iface)};
    for (DWORD i=0; SetupDiEnumDeviceInterfaces(list,NULL,&guid,i,&iface); i++) {
        DWORD size=0;
        SetupDiGetDeviceInterfaceDetailW(list,&iface,NULL,0,&size,NULL);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail = (void*)HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);
        if (!detail) continue;
        detail->cbSize=sizeof(*detail);
        if (SetupDiGetDeviceInterfaceDetailW(list,&iface,detail,size,NULL,NULL)) {
            /* Never open MI_02 keyboard or COM3. Match the tested MI_04 interface explicitly. */
            if (wcsstr(detail->DevicePath,L"vid_0ca3&pid_0021&mi_04")) {
                HANDLE h=CreateFileW(detail->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                    NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
                if (h!=INVALID_HANDLE_VALUE) {
                    HIDD_ATTRIBUTES a={sizeof(a)}; PHIDP_PREPARSED_DATA pp=NULL; HIDP_CAPS caps={0};
                    BOOL valid=HidD_GetAttributes(h,&a) && a.VendorID==0x0ca3 && a.ProductID==0x21;
                    if (valid && HidD_GetPreparsedData(h,&pp)) {
                        valid=HidP_GetCaps(pp,&caps)==HIDP_STATUS_SUCCESS && caps.InputReportByteLength==64
                            && caps.UsagePage==1 && caps.Usage==4;
                        HidD_FreePreparsedData(pp);
                    } else valid=FALSE;
                    if (valid) { found=h; HidD_SetNumInputBuffers(h,32); }
                    else CloseHandle(h);
                }
            }
        }
        HeapFree(GetProcessHeap(),0,detail);
        if (found!=INVALID_HANDLE_VALUE) break;
    }
    SetupDiDestroyDeviceInfoList(list);
    return found;
}

static DWORD WINAPI reader(void *unused) {
    (void)unused;
    for (;;) {
        HANDLE h=open_controller();
        if (h==INVALID_HANDLE_VALUE) { Sleep(1000); continue; }
        InterlockedExchange(&connected,1); log_status("MI_04 connected, read-only, 64-byte reports");
        OVERLAPPED ov={0}; ov.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);
        uint8_t r[64]; BOOL first=TRUE; uint8_t prev_coin=0;
        for (;;) {
            DWORD count=0; ResetEvent(ov.hEvent);
            BOOL ok=ReadFile(h,r,sizeof(r),&count,&ov);
            if (!ok && GetLastError()==ERROR_IO_PENDING) {
                DWORD wait=WaitForSingleObject(ov.hEvent,250);
                if (wait!=WAIT_OBJECT_0) {
                    CancelIoEx(h,&ov); GetOverlappedResult(h,&ov,&count,TRUE);
                    InterlockedExchange(&physical,0);
                    if (wait==WAIT_TIMEOUT) continue;
                    break;
                }
                ok=GetOverlappedResult(h,&ov,&count,FALSE);
            }
            if (!ok) break;
            if (count!=64 || r[0]!=1) continue;
            InterlockedExchange(&physical,(LONG)adx_decode_report(r,count));
            InterlockedExchange(&last_report,(LONG)GetTickCount());
            InterlockedIncrement(&report_count);
            /* IO4 coin count is the high byte of chute[0]. Local firmware emits a pulse. */
            if (!first && r[26]!=prev_coin && r[26]!=0) InterlockedExchange(&coin_pending,1);
            prev_coin=r[26]; first=FALSE;
        }
        CancelIoEx(h,NULL); CloseHandle(ov.hEvent); CloseHandle(h);
        InterlockedExchange(&physical,0); InterlockedExchange(&connected,0);
        log_status("MI_04 disconnected; physical buttons released"); Sleep(500);
    }
}

static BOOL CALLBACK start(PINIT_ONCE o,PVOID p,PVOID *ctx) {
    (void)o;(void)p;(void)ctx;
    HMODULE base=GetModuleHandleW(L"mai2hook.dll");
    if (!base) return FALSE;
    base_init=(void*)GetProcAddress(base,"mai2_io_init");
    base_poll=(void*)GetProcAddress(base,"mai2_io_poll");
    base_game=(void*)GetProcAddress(base,"mai2_io_get_gamebtns");
    base_op=(void*)GetProcAddress(base,"mai2_io_get_opbtns");
    if (!base_init || !base_poll || !base_game || !base_op) return FALSE;
    if (FAILED(base_init())) return FALSE;
    logfile=_fsopen("adx_io4_bridge.log","a",_SH_DENYNO);
    log_status("ADX bridge 1.0 starting; keyboard, touch and LED use original mai2hook");
    HANDLE t=CreateThread(NULL,0,reader,NULL,0,NULL);
    if (!t) return FALSE;
    CloseHandle(t); return TRUE;
}

__declspec(dllexport) uint16_t mai2_io_get_api_version(void) { return 0x0102; }
__declspec(dllexport) HRESULT mai2_io_init(void) {
    return InitOnceExecuteOnce(&once,start,NULL,NULL) ? S_OK : E_FAIL;
}
__declspec(dllexport) HRESULT mai2_io_poll(void) {
    if (!base_poll) return E_UNEXPECTED;
    HRESULT hr=base_poll();
    if (FAILED(hr)) return hr;
    game1=game2=0; operators=0;
    base_game(&game1,&game2); base_op(&operators);
    LONG state=InterlockedCompareExchange(&physical,0,0);
    if ((DWORD)(GetTickCount()-(DWORD)InterlockedCompareExchange(&last_report,0,0))>250) state=0;
    game1|=(uint16_t)(state&0x1ff);
    operators|=(uint8_t)((state>>16)&3);
    if (InterlockedExchange(&coin_pending,0)) operators|=4;
    uint32_t output=game1|((uint32_t)operators<<16);
    if (output!=previous_output) {
        char line[128];
        sprintf_s(line,sizeof(line),"poll game1=%03x op=%02x physical=%05lx reports=%ld",game1,operators,state,report_count);
        log_status(line); previous_output=output;
    }
    return S_OK;
}
__declspec(dllexport) void mai2_io_get_gamebtns(uint16_t *a,uint16_t *b) { if(a)*a=game1;if(b)*b=game2; }
__declspec(dllexport) void mai2_io_get_opbtns(uint8_t *a) { if(a)*a=operators; }
__declspec(dllexport) uint32_t adx_get_report_count(void) { return (uint32_t)report_count; }
__declspec(dllexport) uint32_t adx_get_physical(void) { return (uint32_t)physical; }

static FARPROC original(const char *name) { return GetProcAddress(GetModuleHandleW(L"mai2hook.dll"),name); }
__declspec(dllexport) HRESULT mai2_io_touch_init(void *callback) {
    typedef HRESULT (*fn)(void*); fn f=(fn)original("mai2_io_touch_init"); return f?f(callback):E_FAIL;
}
__declspec(dllexport) void mai2_io_touch_set_sens(uint8_t *bytes) {
    typedef void (*fn)(uint8_t*); fn f=(fn)original("mai2_io_touch_set_sens"); if(f)f(bytes);
}
__declspec(dllexport) void mai2_io_touch_update(uint8_t a,uint8_t b) {
    typedef void (*fn)(uint8_t,uint8_t); fn f=(fn)original("mai2_io_touch_update"); if(f)f(a,b);
}
__declspec(dllexport) HRESULT mai2_io_led_init(void) {
    typedef HRESULT (*fn)(void); fn f=(fn)original("mai2_io_led_init"); return f?f():E_FAIL;
}
#define FORWARD_LED(name) __declspec(dllexport) void name(uint8_t board,const uint8_t *rgb) { \
    typedef void (*fn)(uint8_t,const uint8_t*); fn f=(fn)original(#name); if(f)f(board,rgb); }
FORWARD_LED(mai2_io_led_set_fet_output)
FORWARD_LED(mai2_io_led_dc_update)
FORWARD_LED(mai2_io_led_gs_update)
FORWARD_LED(mai2_io_led_billboard_set)
__declspec(dllexport) void mai2_io_led_cam_set(uint8_t value) {
    typedef void (*fn)(uint8_t); fn f=(fn)original("mai2_io_led_cam_set"); if(f)f(value);
}
