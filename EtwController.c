#include "EtwController.h"
#include <strsafe.h>

TRACEHANDLE g_EtwSessionHandle = 0;

//分配一块内存，包含 EVENT_TRACE_PROPERTIES 结构体和 2KB 的附加空间
static EVENT_TRACE_PROPERTIES* AllocateTraceProperties()
{
    ULONG size = sizeof(EVENT_TRACE_PROPERTIES) + 2 * 1024;
    EVENT_TRACE_PROPERTIES* props = (EVENT_TRACE_PROPERTIES*)calloc(1, size);
    if (!props) return NULL;
    props->Wnode.BufferSize = size;//告诉 ETW 整块缓冲区有多大
    props->Wnode.ClientContext = 1;// 时间戳使用 QPC
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;//表示这是一个 ETW 跟踪会话相关的 WNODE
    props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;//实时模式：事件不写入 .etl 文件，而是供实时消费者读取
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
    //会话名称字符串相对于结构体起始地址的偏移
    WCHAR* sessionName = (WCHAR*)((BYTE*)props + props->LoggerNameOffset);
    StringCchCopyW(sessionName, 1024, MINI_EDR_SESSION_NAME);//
    return props;
}

//开启Etw事件会话
BOOL StartEtwSession()
{
    //申请并初始化 ETW 会话属性内存
    EVENT_TRACE_PROPERTIES* props = AllocateTraceProperties();
    if (!props) return FALSE;
    //启动ETW 会话
    ULONG status = StartTraceW(&g_EtwSessionHandle, MINI_EDR_SESSION_NAME, props);
    //如果同名会话已存在，先停止旧会话再重新启动
    if (status == ERROR_ALREADY_EXISTS)
    {
        //停止旧会话
        ControlTraceW(0, MINI_EDR_SESSION_NAME, props, EVENT_TRACE_CONTROL_STOP);
        g_EtwSessionHandle = 0;
        status = StartTraceW(&g_EtwSessionHandle, MINI_EDR_SESSION_NAME, props);
    }
    // 释放属性内存（StartTraceW 已经复制了需要的信息）
    free(props);

    if (status != ERROR_SUCCESS)
    {
        printf("Etw会话创建失败:%lu\n", status);
        return FALSE;
    }

    printf("[+] Etw会话创建成功\n");
    return TRUE;
}

//相同缓冲/输出/实时/权限/生命周期需求的 Provider，适合放在同一个 Session；
//但每个 Provider 的启用级别、关键字、过滤规则仍然可以单独配置。
BOOL EnableKernelProviders()
{
    ULONG status;
    //启用内核 Process 提供程序
    status = EnableTraceEx2(
        g_EtwSessionHandle,// ETW会话句柄
        &GUID_KERNEL_PROCESS,//内核进程提供程序 GUID
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,// 启用提供程序
        TRACE_LEVEL_VERBOSE,// 详细级别
        0, 0, 0, NULL);
    if (status != ERROR_SUCCESS)
    {
        printf("Process Provider开启失败:%lu\n", status);
        return FALSE;
    }
    printf("[+] Provider Process开启\n");

    //启用内核 Image 提供程序
    status = EnableTraceEx2(
        g_EtwSessionHandle,
        &GUID_KERNEL_IMAGE,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        0, 0, 0, NULL);
    if (status != ERROR_SUCCESS)
    {
        printf("Provider Image开启失败:%lu\n", status);
        return FALSE;
    }
    printf("[+] Provider Image开启\n");
    return TRUE;
}
VOID StopEtwSession()
{
    if (g_EtwSessionHandle == 0) return;
    // 分配属性内存，用于 ControlTraceW 停止会话
    EVENT_TRACE_PROPERTIES* props = AllocateTraceProperties();
    if (props)
    {
        ULONG status = ControlTraceW(
            g_EtwSessionHandle,
            MINI_EDR_SESSION_NAME,
            props,
            EVENT_TRACE_CONTROL_STOP);
        free(props);
    }
    else
    {
        // 内存分配失败时尝试用 NULL 属性停止
        ControlTraceW(g_EtwSessionHandle, MINI_EDR_SESSION_NAME, NULL,EVENT_TRACE_CONTROL_STOP);
    }
    g_EtwSessionHandle = 0;
    printf("[+] Etw会话停止\n");
}