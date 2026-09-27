#include "ProcessContext.h"

//全局进程表：固定大小的数组，每一项描述一个活跃进程
//使用静态数组 + 临界区保护，实现简单的线程安全
static PROCESS_CONTEXT  g_ProcessTable[MAX_PROCESS_COUNT];

//临界区，用于保护 g_ProcessTable 的并发访问
static CRITICAL_SECTION g_ProcessLock;

// 初始化进程上下文：清空表并初始化临界区
BOOL InitProcessContext()
{
    ZeroMemory(g_ProcessTable, sizeof(g_ProcessTable));
    // 初始化临界区，后续 Add/Remove/Dump 都需要用它加锁
    InitializeCriticalSection(&g_ProcessLock);
    return TRUE;
}

// 按进程 ID 查找进程上下文，返回指向表项的指针
// 注意：此函数本身不加锁，调用者需确保已持有 g_ProcessLock，
//       或处于单线程/无并发访问的环境中
PPROCESS_CONTEXT FindProcessContext(ULONG pid)
{
    // 线性扫描进程表，找到 Active 且 PID 匹配的条目
    for (int i = 0; i < MAX_PROCESS_COUNT; i++)
    {
        if (g_ProcessTable[i].Active && g_ProcessTable[i].ProcessId == pid)
            return &g_ProcessTable[i];
    }
    return NULL;   // 未找到
}

// 添加一个进程记录
// pid:       进程 ID
// parentPid: 父进程 ID
// image:     进程映像路径（宽字符串，可为 NULL）
BOOL AddProcessContext(ULONG pid, ULONG parentPid, PCWSTR image)
{
    // 进入临界区，保证对 g_ProcessTable 的修改是线程安全的
    EnterCriticalSection(&g_ProcessLock);

    // 如果该 PID 已存在，则不再重复添加
    if (FindProcessContext(pid))
    {
        LeaveCriticalSection(&g_ProcessLock);
        return FALSE;
    }

    // 遍历查找第一个空闲槽位（Active == FALSE）
    for (int i = 0; i < MAX_PROCESS_COUNT; i++)
    {
        if (!g_ProcessTable[i].Active)
        {
            // 标记为已使用，并填充进程信息
            g_ProcessTable[i].Active = TRUE;
            g_ProcessTable[i].ProcessId = pid;
            g_ProcessTable[i].ParentProcessId = parentPid;

            // 如果提供了映像路径，则安全拷贝到表项中
            if (image)
            {
                wcsncpy_s(g_ProcessTable[i].Image, MAX_IMAGE_PATH, image, _TRUNCATE);
                // 额外保证字符串以 null 结尾
                g_ProcessTable[i].Image[MAX_IMAGE_PATH - 1] = L'\0';
            }

            // 记录进程创建时间（用于后续分析进程存活时间）
            g_ProcessTable[i].CreateTime = GetTickCount64();

            // 成功添加，离开临界区并返回
            LeaveCriticalSection(&g_ProcessLock);
            return TRUE;
        }
    }

    // 没有空闲槽位，添加失败
    LeaveCriticalSection(&g_ProcessLock);
    return FALSE;
}

// 按进程 ID 删除进程记录
// 当进程退出时调用，清理该进程的上下文
BOOL RemoveProcessContext(ULONG pid)
{
    EnterCriticalSection(&g_ProcessLock);

    // 查找目标进程
    PPROCESS_CONTEXT ctx = FindProcessContext(pid);
    if (!ctx)
    {
        // 未找到，直接返回失败
        LeaveCriticalSection(&g_ProcessLock);
        return FALSE;
    }

    // 直接清零整个条目，相当于删除并标记为非活跃
    ZeroMemory(ctx, sizeof(PROCESS_CONTEXT));

    LeaveCriticalSection(&g_ProcessLock);
    return TRUE;
}

// 调试用：打印当前进程表中所有活跃进程的信息
VOID DumpProcessTable()
{
    EnterCriticalSection(&g_ProcessLock);

    printf("\n====== Process Table ======\n");
    for (int i = 0; i < MAX_PROCESS_COUNT; i++)
    {
        if (g_ProcessTable[i].Active)
        {
            // 使用 %ls 打印宽字符串
            printf("PID:%lu Parent:%lu Image:%ls\n",
                g_ProcessTable[i].ProcessId,
                g_ProcessTable[i].ParentProcessId,
                g_ProcessTable[i].Image);
        }
    }

    LeaveCriticalSection(&g_ProcessLock);
}