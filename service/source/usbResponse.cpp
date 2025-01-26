#include <switch.h>
#include "commands.h"


void sendUsbResponse(DebuggerResponse resp)
{
    #ifdef DEBUG
    DEBUG_PRINT("Отправка ответа:\n");
    DEBUG_PRINT("  Result: 0x%x\n", resp.Result);
    DEBUG_PRINT("  LenBytes: %u\n", resp.LenBytes);
    if (resp.LenBytes > 0 && resp.Data != NULL) {
        DEBUG_PRINT("  Data: ");
        u8* data = (u8*)resp.Data;
        for(u32 i = 0; i < resp.LenBytes && i < 32; i++) {
            DEBUG_PRINT("%02x ", data[i]);
        }
        DEBUG_PRINT("\n");
    }
    #endif

    // Отправляем результат и размер (8 байт)
    usbCommsWrite(&resp, 8);

    // Если есть данные, отправляем их
    if (resp.LenBytes > 0 && resp.Data != NULL) {
        usbCommsWrite(resp.Data, resp.LenBytes);
    }
}