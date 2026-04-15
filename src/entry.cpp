#include <ntdef.h>
#include <ntstatus.h>

#include "process.hpp"

extern "C" NTSTATUS DriverEntry() {
    auto proc = process::find(L"Notepad", 7);
    if (proc) {
        // ...
        ObDereferenceObject(proc);
    }

    return STATUS_SUCCESS;
}
