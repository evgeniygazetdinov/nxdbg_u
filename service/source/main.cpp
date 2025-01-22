#include <switch.h>
#include "string"
#include <cstdio>
#include <thread>
#include <sys/stat.h>
#include <memory.h>
#include <cstdarg>
#include <switch/services/usb.h>
#include <switch/runtime/devices/usb_comms.h>

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


Result tryUsbCommsRead2(void* buffer, size_t size, size_t *transferredSize, u64 timeout = 100000000ULL) {
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
static bool kernelAbove300(void) {
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

typedef enum {
    REQ_LIST_PROCESSES   =0,
    REQ_ATTACH_PROCESS   =1,
    REQ_DETACH_PROCESS   =2,
    REQ_QUERYMEMORY      =3,
    REQ_GET_DBGEVENT     =4,
    REQ_READMEMORY       =5,
    REQ_CONTINUE_DBGEVENT=6,
    REQ_GET_THREADCONTEXT=7,
    REQ_BREAK_PROCESS    =8,
    REQ_WRITEMEMORY32    =9,
    REQ_LISTENAPPLAUNCH  =10,
    REQ_GETAPPPID        =11,
    REQ_START_PROCESS    =12,
    REQ_GET_TITLE_PID    =13
} RequestType;

typedef struct {
    u32 Type;
} DebuggerRequest;

typedef struct {
    u32 Result;
    u32 LenBytes;
    void* Data;
} DebuggerResponse;


typedef struct { // Cmd1
    u64 Pid;
} AttachProcessReq;

typedef struct {
    u32 DbgHandle;
} AttachProcessResp;

typedef struct { // Cmd2
    u32 DbgHandle;
} DetachProcessReq;

typedef struct { // Cmd3
    u32 DbgHandle;
    u32 Pad;
    u64 Addr;
} QueryMemoryReq;

typedef struct {
    u64 Addr;
    u64 Size;
    u32 Perm;
    u32 Type;
} QueryMemoryResp;

typedef struct { // Cmd4
    u32 DbgHandle;
} GetDbgEventReq;

typedef struct {
    u8 Event[0x40];
} GetDbgEventResp;

typedef struct { // Cmd5
    u32 DbgHandle;
    u32 Size;
    u64 Addr;
} ReadMemoryReq;

typedef struct { // Cmd6
    u32 DbgHandle;
    u32 Flags;
    u64 ThreadId;
} ContinueDbgEventReq;

typedef struct { // Cmd7
    u32 DbgHandle;
    u32 Flags;
    u64 ThreadId;
} GetThreadContextReq;

typedef struct {
    u8 Out[0x320];
} GetThreadContextResp;

typedef struct { // Cmd8
    u32 DbgHandle;
} BreakProcessReq;

typedef struct { // Cmd9
    u32 DbgHandle;
    u32 Value;
    u64 Addr;
} WriteMemory32Req;

typedef struct { // Cmd11
    u64 Pid;
} GetAppPidResp;

typedef struct { // Cmd12
    u64 Pid;
} StartProcessReq;

typedef struct { // Cmd13
    u64 TitleId;
} GetTitlePidReq;

typedef struct {
    u64 Pid;
} GetTitlePidResp;


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

int handleUsbCommand()
{
    // u32 cmd;
    size_t size;
    printer("inside handleUsbCommand");
    consoleInit(NULL);
    DebuggerRequest r;
    DebuggerResponse resp;
    Result rc;
    size_t transferredSize = 0;

   rc = tryUsbCommsRead2(&r, sizeof(r), &transferredSize);
    if (R_FAILED(rc)) {
        if (rc == MAKERESULT(Module_Libnx, LibnxError_Timeout)) {
            return false; // Таймаут - просто выходим
        }
        printer("USB read error: %x\n", rc);
        return false;
    }

    if (transferredSize != sizeof(r)) {
        printer("Incomplete data: %zu/%zu\n", transferredSize, sizeof(r));
        return false;
    }
    printer("Received request type: %u\n", r.Type);
    resp.LenBytes = 0;
    resp.Result = 0;

    switch (r.Type) {
    case REQ_LIST_PROCESSES: { // Cmd0
        static u64 pids[256];
        s32 numOut = 256;

        rc = svcGetProcessList(&numOut, pids, numOut);
        resp.Result = rc;

        if (rc == 0) {
            resp.LenBytes = numOut * sizeof(u64);
            resp.Data = &pids[0];
        }

        sendUsbResponse(resp);
        break;
    }

    case REQ_ATTACH_PROCESS: { // Cmd1
        AttachProcessReq req_;
        AttachProcessResp resp_;
        usbCommsRead(&req_, sizeof(req_));

        rc = svcDebugActiveProcess(&resp_.DbgHandle, req_.Pid);
        resp.Result = rc;

        if (rc == 0) {
            resp.LenBytes = sizeof(resp_);
            resp.Data = &resp_;
        }

        sendUsbResponse(resp);
        break;
    }

    case REQ_DETACH_PROCESS: { // Cmd2
        DetachProcessReq req_;
        usbCommsRead(&req_, sizeof(req_));

        rc = svcCloseHandle(req_.DbgHandle);
        resp.Result = rc;

        sendUsbResponse(resp);
        break;
    }

    case REQ_QUERYMEMORY: { // Cmd3
        QueryMemoryReq req_;
        QueryMemoryResp resp_;
        usbCommsRead(&req_, sizeof(req_));

        MemoryInfo info;
        u32 who_cares;  // Изменили тип на u32
        rc = svcQueryDebugProcessMemory(&info, &who_cares, req_.DbgHandle, req_.Addr);
        resp.Result = rc;

        if (rc == 0) {
            resp_.Addr = info.addr;
            resp_.Size = info.size;
            resp_.Type = info.type;
            resp_.Perm = info.perm;

            resp.LenBytes = sizeof(resp_);
            resp.Data = &resp_;
        }

        sendUsbResponse(resp);
        break;
    }

    case REQ_GET_DBGEVENT: { // Cmd4
        GetDbgEventReq req_;
        GetDbgEventResp resp_;
        usbCommsRead(&req_, sizeof(req_));

        rc = svcGetDebugEvent(&resp_.Event[0], req_.DbgHandle);
        resp.Result = rc;

        if (rc == 0) {
            resp.LenBytes = sizeof(resp_);
            resp.Data = &resp_;
        }

        sendUsbResponse(resp);
        break;
    }

    case REQ_READMEMORY: { // Cmd5
        ReadMemoryReq req_;
        usbCommsRead(&req_, sizeof(req_));

        if (req_.Size > 0x1000) {
            // Too big read.
            fatalThrow(MAKERESULT(Module_Libnx, 222));  // Используем fatalThrow вместо fatalSimple
        }

        static u8 page[0x1000];
        rc = svcReadDebugProcessMemory(page, req_.DbgHandle, req_.Addr, req_.Size);
        resp.Result = rc;

        if (rc == 0) {
            resp.LenBytes = req_.Size;
            resp.Data = &page[0];
        }

        sendUsbResponse(resp);
        break;
    }

    case REQ_CONTINUE_DBGEVENT: { // Cmd6
        ContinueDbgEventReq req_;
        usbCommsRead(&req_, sizeof(req_));

        if (kernelAbove300()) {
            u32 flags = 4; // ContinueDebugFlags_Resume

            if (req_.Flags & 4) {
                rc = svcContinueDebugEvent(req_.DbgHandle, flags, NULL, 0);
            }
            else {
                rc = svcContinueDebugEvent(req_.DbgHandle, flags, &req_.ThreadId, 1);
            }
        }
        else {
            rc = svcLegacyContinueDebugEvent(req_.DbgHandle, req_.Flags, req_.ThreadId);
        }

        resp.Result = rc;

        sendUsbResponse(resp);
        break;
    }

    case REQ_GET_THREADCONTEXT: { // Cmd7
        GetThreadContextReq req_;
        GetThreadContextResp resp_;
        usbCommsRead(&req_, sizeof(req_));

        ThreadContext ctx;
        rc = svcGetDebugThreadContext(&ctx, req_.DbgHandle, req_.ThreadId, req_.Flags);
        if (rc == 0) {
            memcpy(resp_.Out, &ctx, sizeof(ThreadContext));
            resp.LenBytes = sizeof(resp_);
            resp.Data = &resp_;
        }
        resp.Result = rc;
                sendUsbResponse(resp);
        break;
    }

    case REQ_BREAK_PROCESS: { // Cmd8
        BreakProcessReq req_;
        usbCommsRead(&req_, sizeof(req_));

        rc = svcBreakDebugProcess(req_.DbgHandle);
        resp.Result = rc;

        sendUsbResponse(resp);
        break;
    }

    case REQ_WRITEMEMORY32: { // Cmd9
        WriteMemory32Req req_;
        usbCommsRead(&req_, sizeof(req_));

        rc = svcWriteDebugProcessMemory(req_.DbgHandle, (void*)&req_.Value, req_.Addr, 4);
        resp.Result = rc;

        sendUsbResponse(resp);
        break;
    }

 case REQ_LISTENAPPLAUNCH: { // Cmd10
        Handle h;
        Event debug_event;
        rc = pmdmntHookToCreateApplicationProcess(&debug_event);
        resp.Result = rc;

        if (rc == 0) {  // Добавили фигурные скобки
            h = debug_event.revent;
            svcCloseHandle(h);
        }

        sendUsbResponse(resp);
        break;
    }

    case REQ_GETAPPPID: { // Cmd11
        GetAppPidResp resp_;

        rc = pmdmntGetApplicationProcessId(&resp_.Pid);  // Changed from pmdmntGetApplicationPid
        resp.Result = rc;

        if (rc == 0) {
            resp.LenBytes = sizeof(resp_);
            resp.Data = &resp_;
        }

        sendUsbResponse(resp);
        break;
    }

    case REQ_START_PROCESS: { // Cmd12
        StartProcessReq req_;
        usbCommsRead(&req_, sizeof(req_));

        rc = pmdmntStartProcess(req_.Pid);
        resp.Result = rc;

        sendUsbResponse(resp);
        break;
    }

     case REQ_GET_TITLE_PID: { // Cmd13
        GetTitlePidReq req_;
        GetTitlePidResp resp_;
        usbCommsRead(&req_, sizeof(req_));

        rc = pmdmntGetProcessId(&resp_.Pid, req_.TitleId);  // Changed from pmdmntGetTitlePid
        resp.Result = rc;

        if (rc == 0) {
            resp.LenBytes = sizeof(resp_);
            resp.Data = &resp_;
        }

        sendUsbResponse(resp);
        break;
    }

    default:
        // Unknown request.
        fatalThrow(MAKERESULT(Module_Libnx, 222));  // Заменяем fatalSimple на fatalThrow
    }

    return 1;
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
