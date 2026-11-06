/* Test fixture only: never deploy this DLL to the game directory. */
#include <windows.h>
#include <stdint.h>
static uint32_t flags;
__declspec(dllexport) HRESULT mai2_io_init(void) { flags|=1;return S_OK; }
__declspec(dllexport) HRESULT mai2_io_poll(void) {return S_OK;}
__declspec(dllexport) void mai2_io_get_gamebtns(uint16_t*a,uint16_t*b) {*a=0x100;*b=0x80;}
__declspec(dllexport) void mai2_io_get_opbtns(uint8_t*a) {*a=2;}
__declspec(dllexport) HRESULT mai2_io_touch_init(void*p) {flags|=2;return p==(void*)123?42:E_FAIL;}
__declspec(dllexport) void mai2_io_touch_set_sens(uint8_t*p) {if(p[0]==77)flags|=4;}
__declspec(dllexport) void mai2_io_touch_update(uint8_t a,uint8_t b) {if(a==1&&b==0)flags|=8;}
__declspec(dllexport) HRESULT mai2_io_led_init(void) {flags|=16;return 43;}
#define LED(name,bit) __declspec(dllexport) void name(uint8_t b,const uint8_t*p) {if(b==1&&p[0]==77)flags|=bit;}
LED(mai2_io_led_set_fet_output,32)
LED(mai2_io_led_dc_update,64)
LED(mai2_io_led_gs_update,128)
LED(mai2_io_led_billboard_set,256)
__declspec(dllexport) void mai2_io_led_cam_set(uint8_t b) {if(b==77)flags|=512;}
__declspec(dllexport) uint32_t test_flags(void) {return flags;}
