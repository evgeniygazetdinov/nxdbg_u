// Copyright 2017 plutoo
#include <switch.h>
#include "string"
#include <cstdio>
#include <thread>
#include <sys/stat.h>
#include <memory.h>


const int KEY_MINUS = 2048;
const int KEY_PLUS = 1024;

const int KEY_X = 4;
const int KEY_Y = 28;
const int KEY_A = 5;
const int KEY_B = 27;

#define DEBUG 1

#ifdef DEBUG
#define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif


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
    printf("inside handleUsbCommand");
    consoleInit(NULL);
    DebuggerRequest r;
    DebuggerResponse resp;
    Result rc;

//   size_t len = usbCommsRead(&r, sizeof(r));e
// if (len == 0) {
//     printf("No data\n");
//     return false; // нет данных, выходим сразу
// }
 

    resp.LenBytes = 0;
    resp.Data = NULL;

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
            printf("Exiting...\n");
            log_to_file("Exiting...");
            return false;
        }

        if (kDown & KEY_B) {
            printf("\nStarting Debug launch process...\n");
            needToRun = true;
        }
        if (needToRun){
            Result rc;
            printf("\n  before initialize pmdmntInitialize\n");

            rc = pmdmntInitialize();
            printf("\n success initialize pmdmntInitialize\n");
            if (rc) {
                // Failed to get PM debug interface.
                 fatalThrow(222 | (6 << 9));
            }
               
            printf("\n  before initialize usbCommsInitialize\n");
            rc = usbCommsInitialize();
            printf("\n  success initialize usbCommsInitialize\n");
            if (rc) {
                fatalThrow(rc);
            }
            needToRun = false;
            possibleToRun = true;
            // while ();
        }
        if(possibleToRun){
            printf("\n  success ready for handleUsbCommand\n");
            consoleUpdate(NULL);
            handleUsbCommand();

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
