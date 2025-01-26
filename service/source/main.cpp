#include <switch.h>
#include "string"
#include <cstdio>
#include <thread>
#include <sys/stat.h>

#include <switch/services/usb.h>
#include <switch/runtime/devices/usb_comms.h>
#include "variable.hpp"
#include "commands.h"

<<<<<<< Updated upstream
void printer(const char* format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    printf("%s\n", buffer);
    svcSleepThread(250000000ULL);
    consoleUpdate(NULL);
}

// USB дескрипторы и информация об устройстве
static UsbDsDeviceInfo g_device_info = {
    .idVendor = 0x057E,
    .idProduct = 0x3000,
    .bcdDevice = 0x0100,
    .Manufacturer = "Nintendo",
    .Product = "NXDebugger",
    .SerialNumber = "0001"
};

static usb_interface_descriptor g_interface_descriptor = {
    .bLength = USB_DT_INTERFACE_SIZE,
    .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = USBDS_DEFAULT_InterfaceNumber,
    .bAlternateSetting = 0x00,
    .bNumEndpoints = 0x02,
    .bInterfaceClass = 0xFF,
    .bInterfaceSubClass = 0xFF,
    .bInterfaceProtocol = 0xFF,
};

static usb_endpoint_descriptor g_endpoint_descriptor_in = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_IN,
    .bmAttributes = USB_TRANSFER_TYPE_BULK,
    .wMaxPacketSize = 0x200,
};

static usb_endpoint_descriptor g_endpoint_descriptor_out = {
    .bLength = USB_DT_ENDPOINT_SIZE,
    .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = USB_ENDPOINT_OUT,
    .bmAttributes = USB_TRANSFER_TYPE_BULK,
    .wMaxPacketSize = 0x200,
};


const int KEY_MINUS = 2048;
const int KEY_PLUS = 1024;

const int KEY_X = 4;
const int KEY_Y = 28;
const int KEY_A = 5;
const int KEY_B = 27;

// USB endpoints
static UsbDsEndpoint *g_usbComms_endpoint_in = NULL;
static UsbDsEndpoint *g_usbComms_endpoint_out = NULL;
static u8 g_usbComms_endpoint_in_buffer[0x1000];
static u8 g_usbComms_endpoint_out_buffer[0x1000];
static u8 g_usbComms_buffer[0x1000];
static UsbDsInterface* g_interface = NULL;
static UsbDsEndpoint* g_endpoint_in = NULL;
static UsbDsEndpoint* g_endpoint_out = NULL;

Result initializeUsb2() {
    Result rc = 0;
    int retryCount = 0;
    const int maxRetries = 10;
    
    printer("Starting USB initialization...");

    // Сначала деинициализируем, если уже было инициализировано
    usbCommsExit();
    
    // Даем системе время очистить предыдущее состояние
    svcSleepThread(500000000ULL); // 500ms
    
    while (retryCount < maxRetries) {
        // Инициализируем USB через usbComms
        rc = usbCommsInitialize();
        if (R_FAILED(rc)) {
            printer("usbCommsInitialize failed: %x", rc);
            retryCount++;
            if (retryCount < maxRetries) {
                printer("Retrying USB initialization (%d/%d)...", retryCount + 1, maxRetries);
                svcSleepThread(1000000000ULL);
                continue;
            }
            return rc;
        }
        printer("USB initialized successfully");

        // Проверяем, что USB действительно работает
        if (!usbCommsInitialize()) {
            printer("USB initialization check failed");
            usbCommsExit();
            retryCount++;
            if (retryCount < maxRetries) {
                printer("Retrying USB initialization (%d/%d)...", retryCount + 1, maxRetries);
                svcSleepThread(1000000000ULL);
                continue;
            }
            return -1;
        }
        
        printer("USB is ready");
        break;
    }

    printer("USB initialization complete!");
    return rc;
}

Result initializeUsb() {
    Result rc = 0;
    int retryCount = 0;
    const int maxRetries = 10;
    
    printer("Starting USB initialization...");

    // Сначала деинициализируем, если уже было инициализировано
    usbDsExit();
    
    // Даем системе время очистить предыдущее состояние
    svcSleepThread(500000000ULL); // 500ms

    // Проверяем состояние USB
    UsbState state;
    rc = usbDsGetState(&state);
    if (R_SUCCEEDED(rc)) {
        printer("Current USB state: %d", state);
    }
    
    while (retryCount < maxRetries) {
        // Инициализируем USB
        rc = usbDsInitialize();
        if (R_FAILED(rc)) {
            printer("usbDsInitialize failed: %x", rc);
            retryCount++;
            if (retryCount < maxRetries) {
                printer("Retrying USB initialization (%d/%d)...", retryCount + 1, maxRetries);
                svcSleepThread(1000000000ULL);
                continue;
            }
            return rc;
        }
        printer("usbDsInitialize success");

        // Ждем готовности USB с увеличенным таймаутом
        rc = usbDsWaitReady(5000000000ULL); // 5 seconds
        if (R_FAILED(rc)) {
            printer("USB not ready: %x", rc);
            usbDsExit();
            retryCount++;
            if (retryCount < maxRetries) {
                printer("Retrying USB initialization (%d/%d)...", retryCount + 1, maxRetries);
                svcSleepThread(1000000000ULL);
                continue;
            }
            return rc;
        }
        
        printer("USB is ready");
        break;
    }

    // Дополнительная пауза после успешной инициализации
    svcSleepThread(1000000000ULL); // 1 second

    // Создаем интерфейс с базовыми параметрами
    rc = usbDsGetDsInterface(&g_interface, &g_interface_descriptor, "nx-debug");
    if (R_FAILED(rc)) {
        printer("Failed to get interface: %x", rc);
        usbDsExit();
        return rc;
    }
    printer("Interface created");

    // Пауза после создания интерфейса
    svcSleepThread(1000000000ULL); // 1 second

    // Создаем endpoints
    rc = usbDsInterface_GetDsEndpoint(g_interface, &g_endpoint_in, &g_endpoint_descriptor_in);
    if (R_FAILED(rc)) {
        printer("Failed to get input endpoint: %x", rc);
        usbDsExit();
        return rc;
    }
    printer("Input endpoint created");

    rc = usbDsInterface_GetDsEndpoint(g_interface, &g_endpoint_out, &g_endpoint_descriptor_out);
    if (R_FAILED(rc)) {
        printer("Failed to get output endpoint: %x", rc);
        usbDsExit();
        return rc;
    }
    printer("Output endpoint created");

    printer("USB initialization complete!");

    return rc;
}
=======

>>>>>>> Stashed changes


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
