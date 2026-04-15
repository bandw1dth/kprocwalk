#pragma once
#include <ntddk.h>

extern "C" NTKERNELAPI PEPROCESS PsInitialSystemProcess; // NOLINT(bugprone-dynamic-static-initializers)
extern "C" NTKERNELAPI BOOLEAN   MmIsAddressValid(_In_ PVOID VirtualAddress);
extern "C" NTKERNELAPI UCHAR    *PsGetProcessImageFileName(_In_ PEPROCESS Process);
