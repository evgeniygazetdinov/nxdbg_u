#include <switch.h>
#include "commands.h"
#include <memory.h>
#include "variable.hpp"




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