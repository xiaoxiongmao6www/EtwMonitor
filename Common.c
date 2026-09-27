#include "Common.h"

const GUID GUID_KERNEL_PROCESS =
{
    0x22FB2CD6,
    0x0E7B,
    0x422B,
    { 0xA0, 0xC7, 0x2F, 0xAD, 0x1F, 0xD0, 0xE7, 0x16 }
};

const GUID GUID_KERNEL_IMAGE =
{
    0x2CB15D1D,
    0x5FC1,
    0x11D2,
    { 0xAB, 0xE1, 0x00, 0xA0, 0xC9, 0x11, 0xF5, 0x18 }
};

//打印GUID
VOID PrintGUID(const GUID* g)
{
    if (!g) return;
    printf("{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        g->Data1, g->Data2, g->Data3,
        g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3],
        g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
}