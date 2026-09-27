#include "Parser.h"
#include "ProcessContext.h"
#include "ModuleContext.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <evntcons.h>
#include <tdh.h>

//比较 EVENT_RECORD中的 ProviderId 与给定 GUID 是否一致
static BOOL IsProvider(PEVENT_RECORD Record, const GUID* Guid)
{
    if (!Record || !Guid) return FALSE;
    return memcmp(&Record->EventHeader.ProviderId, Guid, sizeof(GUID)) == 0;
}

//使用TdhGetProperty 从事件负载中提取指定名称的 ULONG 属性
BOOL GetUInt32Property(PEVENT_RECORD Record, LPCWSTR Name, ULONG* Value)
{
    if (!Record || !Name || !Value) return FALSE;
    PROPERTY_DATA_DESCRIPTOR desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.PropertyName = (ULONGLONG)Name;// 属性名
    desc.ArrayIndex = ULONG_MAX;
    ULONG status = TdhGetProperty(Record, 0, NULL, 1, &desc,sizeof(ULONG), (PBYTE)Value);
    return status == ERROR_SUCCESS;
}

BOOL GetUInt64Property(PEVENT_RECORD Record, LPCWSTR Name, ULONG64* Value)
{
    if (!Record || !Name || !Value) return FALSE;
    PROPERTY_DATA_DESCRIPTOR desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.PropertyName = (ULONGLONG)Name;
    desc.ArrayIndex = ULONG_MAX;
    ULONG status = TdhGetProperty(Record, 0, NULL, 1, &desc,sizeof(ULONG64), (PBYTE)Value);
    return status == ERROR_SUCCESS;
}

// 查询所需大小，分配临时缓冲区，再读取内容，并安全拷贝到调用者提供的 Buffer
BOOL GetStringProperty(PEVENT_RECORD Record, LPCWSTR Name,PWCHAR Buffer, ULONG BufferSize)
{
    if (!Record || !Name || !Buffer || BufferSize == 0) return FALSE;
    Buffer[0] = L'\0';
    PROPERTY_DATA_DESCRIPTOR desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.PropertyName = (ULONGLONG)Name;
    desc.ArrayIndex = ULONG_MAX;
    //查询属性数据大小
    ULONG size = 0;
    ULONG status = TdhGetPropertySize(Record, 0, NULL, 1, &desc, &size);
    if (status != ERROR_SUCCESS || size == 0) return FALSE;
    if (size > 64 * 1024) return FALSE;    // 单字段 >64KB 视为异常
    //分配临时缓冲区，多留一个 WCHAR 用于确保 null 结尾
    ULONG alloc = size + sizeof(WCHAR);
    PWCHAR temp = (PWCHAR)malloc(alloc);
    if (!temp) return FALSE;
    ZeroMemory(temp, alloc);
    //第二步：读取实际属性数据
    status = TdhGetProperty(Record, 0, NULL, 1, &desc, size, (PBYTE)temp);
    BOOL ok = FALSE;
    if (status == ERROR_SUCCESS)
    {
        // 计算最多能拷多少个 WCHAR
        ULONG wcharCount = size / sizeof(WCHAR);
        ULONG realLen = 0;
        while (realLen < wcharCount && temp[realLen] != L'\0')
            ++realLen;
        // 防止拷贝长度超过调用者缓冲区
        if (realLen >= BufferSize)
            realLen = BufferSize - 1;

        if (realLen > 0)
            memcpy(Buffer, temp, realLen * sizeof(WCHAR));

        Buffer[realLen] = L'\0';
        ok = (realLen > 0);
    }
    free(temp);
    return ok;
}

//调试时打印事件所有顶层属性名
VOID DumpEventPayload(PEVENT_RECORD Record)
{
    ULONG size = 0;
    ULONG status = TdhGetEventInformation(Record, 0, NULL, NULL, &size);
    if (status != ERROR_INSUFFICIENT_BUFFER || size == 0) return;
    PTRACE_EVENT_INFO info = (PTRACE_EVENT_INFO)malloc(size);
    if (!info) return;
    status = TdhGetEventInformation(Record, 0, NULL, info, &size);
    if (status != ERROR_SUCCESS)
    {
        free(info);
        return;
    }

    printf("\n---- TDH Payload ----\n");
    printf("Property Count:%lu\n", info->TopLevelPropertyCount);
    //遍历顶层属性，打印属性名
    for (ULONG i = 0; i < info->TopLevelPropertyCount; i++)
    {
        PEVENT_PROPERTY_INFO prop = &info->EventPropertyInfoArray[i];
        LPCWSTR name = (LPCWSTR)((BYTE*)info + prop->NameOffset);
        wprintf(L"Property[%lu]: %s\n", i, name);
    }
    free(info);
}

// 进程创建事件：Event ID = 1
static BOOL IsProcessCreate(PEVENT_RECORD Record)
{
    return Record && Record->EventHeader.EventDescriptor.Id == 1;
}
// 进程退出事件：Event ID = 2
static BOOL IsProcessExit(PEVENT_RECORD Record)
{
    return Record && Record->EventHeader.EventDescriptor.Id == 2;
}
// 映像加载事件：Event ID = 5，且 Keyword 低位包含 0x40
static BOOL IsImageLoad(PEVENT_RECORD Record)
{
    if (!Record) return FALSE;
    if (Record->EventHeader.EventDescriptor.Id != 5)
        return FALSE;
    return (Record->EventHeader.EventDescriptor.Keyword & 0x40) != 0;
}

// 从事件中提取 PID、父 PID、映像路径
static BOOL ExtractProcessPayload(PEVENT_RECORD Record,ULONG* pid, ULONG* parentPid, WCHAR* image)
{
    if (!Record || !pid || !parentPid || !image) return FALSE;
    *pid = 0;
    *parentPid = 0;
    wcsncpy_s(image, MAX_IMAGE_PATH, L"Unknown", _TRUNCATE);
    GetUInt32Property(Record, L"ProcessID", pid);
    GetUInt32Property(Record, L"ParentProcessID", parentPid); 
    GetStringProperty(Record, L"ImageName", image, MAX_IMAGE_PATH);
    return *pid != 0;
}
// 解析进程创建事件，并更新进程上下文
BOOL ParseProcessCreate(PEVENT_RECORD Record)
{
    ULONG pid = 0, parentPid = 0;
    WCHAR image[MAX_IMAGE_PATH] = { 0 };
    if (!ExtractProcessPayload(Record, &pid, &parentPid, image))
        return FALSE;
    //记录进程信息到上下文表中
    AddProcessContext(pid, parentPid, image);
    printf("\n[PROCESS CREATE]\n");
    printf("PID:%lu Parent:%lu\n", pid, parentPid);
    wprintf(L"Image:%s\n", image);
    return TRUE;
}
// 解析进程退出事件，清理进程及其模块上下文
BOOL ParseProcessExit(PEVENT_RECORD Record)
{
    ULONG pid = 0;
    GetUInt32Property(Record, L"ProcessID", &pid);

    if (pid)
    {
        RemoveProcessContext(pid);// 移除进程上下文
        RemoveModuleByProcess(pid);// 联动清理该进程加载的模块
        printf("[PROCESS EXIT] PID:%lu\n", pid);
    }
    return TRUE;
}

// 提取模块路径、基址、大小，并更新模块上下文
BOOL ParseImageLoad(PEVENT_RECORD Record)
{
    ULONG pid = (ULONG)Record->EventHeader.ProcessId;
    WCHAR image[MAX_IMAGE_PATH] = { 0 };
    // 尝试两种可能的属性名
    if (!GetStringProperty(Record, L"ImageName", image, MAX_IMAGE_PATH) &&
        !GetStringProperty(Record, L"ImageFileName", image, MAX_IMAGE_PATH))
    {
        wcsncpy_s(image, MAX_IMAGE_PATH, L"<unknown>", _TRUNCATE);
    }
    ULONG64 base = 0;
    ULONG64 size64 = 0;
    GetUInt64Property(Record, L"ImageBase", &base);
    if (!GetUInt64Property(Record, L"ImageSize", &size64))
    {
        ULONG size32 = 0;
        if (GetUInt32Property(Record, L"ImageSize", &size32))
            size64 = size32;
    }

    AddModuleContext(pid, image,(PVOID)(ULONG_PTR)base,(ULONG)size64);

    printf("\n[IMAGE LOAD]\n");
    printf("PID:%lu\n", pid);
    wprintf(L"Module:%s\n", image);
    printf("Base:%p Size:%llu\n",(PVOID)(ULONG_PTR)base, (unsigned long long)size64);
    return TRUE;
}

//总入口
VOID ParseEtwEvent(PEVENT_RECORD Record)
{
    if (!Record) return;
    // 内核进程 Provider：包含进程创建、退出、映像加载
    if (IsProvider(Record, &GUID_KERNEL_PROCESS))
    {
        if (IsProcessCreate(Record))      ParseProcessCreate(Record);
        else if (IsProcessExit(Record))   ParseProcessExit(Record);
        else if (IsImageLoad(Record))     ParseImageLoad(Record); 
        return;
    }
    if (IsProvider(Record, &GUID_KERNEL_IMAGE) && IsImageLoad(Record))
    {
        ParseImageLoad(Record);
    }
}