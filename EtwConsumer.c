#include "EtwConsumer.h"
#include "Parser.h"
#include <evntrace.h>
#include <evntcons.h>

static TRACEHANDLE g_TraceHandle = 0;

// 每个事件独立保护区：任何异常只影响当前事件，不会整进程崩掉
static VOID WINAPI EventRecordCallback(PEVENT_RECORD Record)
{
    if (!Record) return;
    __try
    {
        ParseEtwEvent(Record);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        printf("[!] ParseEtwEvent出现异常, code=0x%08lX\n",GetExceptionCode());
    }
}
//ETW消费者线程入口函数
DWORD WINAPI EtwConsumerThread(LPVOID param)
{
    (void)param;
    EVENT_TRACE_LOGFILEW logfile;
    ZeroMemory(&logfile, sizeof(logfile));
    // 指定要连接的实时 ETW 会话名称
    logfile.LoggerName = (LPWSTR)MINI_EDR_SESSION_NAME;
    // 设置处理模式
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME
        | PROCESS_TRACE_MODE_EVENT_RECORD;
    // 绑定事件记录回调函数，每收到一个事件就会调用 EventRecordCallback
    logfile.EventRecordCallback = EventRecordCallback;
    // 打开跟踪会话
    g_TraceHandle = OpenTraceW(&logfile);
    if (g_TraceHandle == INVALID_PROCESSTRACE_HANDLE)
    {
        printf("跟踪会话打开失败:%lu\n", GetLastError());
        return 0;
    }
    printf("[+] Etw Consumer运行\n");
    // 开始处理跟踪事件
    ULONG status = ProcessTrace(&g_TraceHandle, 1, NULL, NULL);
    if (status != ERROR_SUCCESS)
    {
        printf("ProcessTrace:%lu\n", status);
    }
    CloseTrace(g_TraceHandle);
    g_TraceHandle = 0;
    return 0;
}