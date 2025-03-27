#include <switch.h>
#include "string"
#include <cstdio>
#include <thread>
#include <sys/stat.h>

#include <switch/services/usb.h>
#include <switch/runtime/devices/usb_comms.h>
#include "variable.hpp"
#include "commands.h"

Result tryUsbCommsRead2(void* buffer, size_t size, size_t *transferredSize, u64 timeout) {
    Result rc = 0;
    u32 urbId = 0;
    UsbDsReportData reportdata;
    u32 tmp_transferredSize = 0;

    // Проверяем готовность USB с таймаутом
    rc = usbDsWaitReady(timeout);
    if (R_FAILED(rc)) {
        printer("USB not ready: %x\n", rc);
        return rc;
    }

    // Начинаем асинхронный прием данных
    rc = usbDsEndpoint_PostBufferAsync(g_endpoint_in, buffer, size, &urbId);
    if (R_FAILED(rc)) {
        printer("Failed to post buffer: %x\n", rc);
        return rc;
    }

    // Ждем завершения с таймаутом
    rc = eventWait(&g_endpoint_in->CompletionEvent, timeout);
    if (R_FAILED(rc)) {
        printer("USB read timeout\n");
        return MAKERESULT(Module_Libnx, LibnxError_Timeout);
    }

    rc = usbDsEndpoint_GetReportData(g_endpoint_in, &reportdata);
    if (R_FAILED(rc)) return rc;

    rc = usbDsParseReportData(&reportdata, urbId, NULL, &tmp_transferredSize);
    if (R_FAILED(rc)) return rc;

    if (transferredSize) *transferredSize = tmp_transferredSize;
    return rc;
}
// Определение функции для проверки версии ядра
bool kernelAbove300(void) {
    u64 major=0, minor=0;
    Result rc;
       // Проверяем результаты вызовов
    if (R_FAILED(rc = svcGetSystemInfo(&major, 0, INVALID_HANDLE, 0)))
        return false;
    if (R_FAILED(rc = svcGetSystemInfo(&minor, 0, INVALID_HANDLE, 1)))
        return false;
    return major >= 3;
}


u32 __nx_applet_type = AppletType_None;

static char g_heap[0x20000];

void __libnx_initheap(void)
{
    extern char* fake_heap_start;
    extern char* fake_heap_end;

    fake_heap_start = &g_heap[0];
    fake_heap_end   = &g_heap[sizeof g_heap];
}


void log_to_file(const char* message) {
    FILE* log_file = fopen("sdmc:/switch/dolphin-emu/dolphin-launcher.log", "a");
    if (log_file) {
        time_t now = time(NULL);
        char timestamp[26];
        ctime_r(&now, timestamp);
        timestamp[24] = '\0'; // Убираем перенос строки
        fprintf(log_file, "[%s] %s\n", timestamp, message);
        fclose(log_file);
    }
}
// Function to execute a command on Switch with arguments

bool mainLoop() {
    printf("\n\n-------- Main Menu --------\n");
    printf("Press B to run debug runner\n");
    printf("Press - to exit\n");
    log_to_file("Main menu started");

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    bool needToRun = false;
    bool possibleToRun = false;
    while (appletMainLoop()) {
        // Сканируем ввод
        
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);
        if (kDown & KEY_MINUS || kDown & KEY_MINUS & possibleToRun) {
            printer("Exiting...\n");
            log_to_file("Exiting...");
            if (g_interface) {
                usbDsInterface_Close(g_interface);
                usbDsExit();
            }
            return false;
        }

        if (kDown & KEY_B) {
            printer("\nStarting Debug launch process...\n");
            needToRun = true;
        }
        if (needToRun){
           Result rc;
            printer("before initialize pmdmntInitialize");

            rc = pmdmntInitialize();
            printer("\n success initialize pmdmntInitialize\n");
            if (rc) {
                // Failed to get PM debug interface.
                fatalThrow(222 | (6 << 9));
            }
                
            printer("\n  before initialize USB\n");
            rc = initializeUsb2();
            if (R_FAILED(rc)) {
                printer("USB initialization failed: %x\n", rc);
                 pmdmntExit();
                return false;
            }
            printer("\n  success initialize USB\n");
                        svcSleepThread(1000000000ULL); // 1 секунда
            needToRun = false;
            possibleToRun = true;
                // while ();
        }
        if(possibleToRun){
            printer("\n  success ready for handleUsbCommand\n");
            if (!handleUsbCommand()) {
                printer("\n  USB command failed, retrying...\n");
                svcSleepThread(100000000ULL); // 100ms пауза перед следующей попыткой
            }

        }
        consoleUpdate(NULL);
        svcSleepThread(100000000ULL);
    }        

    return true;
}


int main(int argc, char* argv[]) {
    // Инициализация консоли

    consoleInit(NULL);
    log_to_file("Program started");
    mainLoop();
    consoleExit(NULL);
    return 0;
}
