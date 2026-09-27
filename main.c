#include <windows.h>
#include <stdio.h>
#include "Common.h"
#include "EtwController.h"
#include "EtwConsumer.h"
#include "ProcessContext.h"
#include "ModuleContext.h"
#pragma comment(lib,"advapi32.lib")

int wmain()
{
    printf("\n");
    printf("==============================\n");
    printf("           Etw Demo\n");
    printf("==============================\n\n");

    if (!InitProcessContext())
    {
        printf("进程上下文初始化失败\n");
        return -1;
    }

    if (!InitModuleContext())
    {
        printf("模块上下文初始化失败\n");
        return -1;
    }

    if (!StartEtwSession())
    {
        printf("创建ETW Session失败\n");
        return -1;
    }

    if (!EnableKernelProviders())
    {
        printf("Enable provider failed\n");
        StopEtwSession();
        return -1;
    }

    HANDLE hThread = CreateThread(NULL, 0, EtwConsumerThread, NULL, 0, NULL);
    if (!hThread)
    {
        printf("Consumer线程创建失败\n");
        StopEtwSession();
        return -1;
    }

    printf("[+] 按ENTER停止\n");
    getchar();

    StopEtwSession();
    printf("Exit\n");

    // 等消费者线程完全退出（StopEtwSession 后必然返回）
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    // 输出最终表
    DumpProcessTable();
    DumpModuleTable();

    printf("\nEtw Demo停止\n");
    getchar();
    return 0;
}