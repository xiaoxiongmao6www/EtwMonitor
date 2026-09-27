#include "ModuleContext.h"

// 全局模块表：固定大小的数组，每一项描述一个已加载模块
// 使用静态数组 + 临界区保护，实现简单的线程安全
static MODULE_CONTEXT   g_ModuleTable[MAX_MODULE_COUNT];

// 临界区，用于保护 g_ModuleTable 的并发访问
static CRITICAL_SECTION g_ModuleLock;

// 初始化模块上下文：清空表并初始化临界区
BOOL InitModuleContext()
{
    ZeroMemory(g_ModuleTable, sizeof(g_ModuleTable));
    // 初始化临界区，后续 Add/Remove/Dump 都需要用它加锁
    InitializeCriticalSection(&g_ModuleLock);
    return TRUE;
}

// 添加一个模块记录
// pid:        所属进程 ID
// moduleName: 模块路径（宽字符串）
// base:       模块基址
// size:       模块大小（字节）
BOOL AddModuleContext(ULONG pid, PCWSTR moduleName, PVOID base, ULONG size)
{
    if (!moduleName) return FALSE;
    // 进入临界区，保证对 g_ModuleTable 的修改是线程安全的
    EnterCriticalSection(&g_ModuleLock);
    // 遍历查找第一个空闲位（Active == FALSE）
    for (int i = 0; i < MAX_MODULE_COUNT; i++)
    {
        if (!g_ModuleTable[i].Active)
        {
            // 标记为已使用，并填充模块信息
            g_ModuleTable[i].Active = TRUE;
            g_ModuleTable[i].ProcessId = pid;
            g_ModuleTable[i].BaseAddress = base;
            g_ModuleTable[i].Size = size;
            g_ModuleTable[i].LoadTime = GetTickCount64();  // 记录加载时间

            // 安全拷贝模块名，防止缓冲区溢出
            wcsncpy_s(g_ModuleTable[i].ModuleName, MAX_MODULE_PATH,
                moduleName, _TRUNCATE);

            // 额外保证字符串以 null 结尾（双保险）
            g_ModuleTable[i].ModuleName[MAX_MODULE_PATH - 1] = L'\0';

            // 成功添加，离开临界区并返回
            LeaveCriticalSection(&g_ModuleLock);
            return TRUE;
        }
    }

    // 没有空闲槽位，添加失败
    LeaveCriticalSection(&g_ModuleLock);
    return FALSE;
}

// 按进程 ID 删除该进程的所有模块记录
// 当进程退出时调用，联动清理其加载的模块
BOOL RemoveModuleByProcess(ULONG pid)
{
    EnterCriticalSection(&g_ModuleLock);

    // 遍历所有条目，找到属于该进程的活跃模块
    for (int i = 0; i < MAX_MODULE_COUNT; i++)
    {
        if (g_ModuleTable[i].Active && g_ModuleTable[i].ProcessId == pid)
        {
            // 直接清零整个条目，相当于删除并标记为非活跃
            ZeroMemory(&g_ModuleTable[i], sizeof(MODULE_CONTEXT));
        }
    }

    LeaveCriticalSection(&g_ModuleLock);
    return TRUE;
}

// 调试用：打印当前模块表中所有活跃模块的信息
VOID DumpModuleTable()
{
    EnterCriticalSection(&g_ModuleLock);
    printf("\n========== Module Table ==========\n");
    for (int i = 0; i < MAX_MODULE_COUNT; i++)
    {
        if (g_ModuleTable[i].Active)
        {
            // 使用 %ls 打印宽字符串
            printf("PID:%lu Module:%ls Base:%p Size:%lu\n",
                g_ModuleTable[i].ProcessId,
                g_ModuleTable[i].ModuleName,
                g_ModuleTable[i].BaseAddress,
                g_ModuleTable[i].Size);
        }
    }
    LeaveCriticalSection(&g_ModuleLock);
}