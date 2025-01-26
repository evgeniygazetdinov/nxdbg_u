#ifndef NXDBG_COMMANDS_H
#define NXDBG_COMMANDS_H

#include <switch.h>

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

typedef struct {
    u64 Pid;
} AttachProcessReq;

typedef struct {
    u32 DbgHandle;
} AttachProcessResp;

typedef struct {
    u32 DbgHandle;
} DetachProcessReq;

typedef struct {
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

typedef struct {
    u32 DbgHandle;
} GetDbgEventReq;

typedef struct {
    u8 Event[0x40];
} GetDbgEventResp;

typedef struct {
    u32 DbgHandle;
    u32 Size;
    u64 Addr;
} ReadMemoryReq;

typedef struct {
    u32 DbgHandle;
    u32 Flags;
    u64 ThreadId;
} ContinueDbgEventReq;

typedef struct {
    u32 DbgHandle;
    u32 Flags;
    u64 ThreadId;
} GetThreadContextReq;

typedef struct {
    u8 Out[0x320];
} GetThreadContextResp;

typedef struct {
    u32 DbgHandle;
} BreakProcessReq;

typedef struct {
    u32 DbgHandle;
    u32 Value;
    u64 Addr;
} WriteMemory32Req;

typedef struct {
    u64 Pid;
} GetAppPidResp;

typedef struct {
    u64 Pid;
} StartProcessReq;

typedef struct {
    u64 TitleId;
} GetTitlePidReq;

typedef struct {
    u64 Pid;
} GetTitlePidResp;

#endif // NXDBG_COMMANDS_H