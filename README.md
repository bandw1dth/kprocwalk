# kprocwalk

A lightweight Windows kernel-mode utility for locating running processes by name substring.

---

## Overview

`process::find` walks the kernel's `ActiveProcessLinks` list to locate a `PEPROCESS` by a case-insensitive Unicode name substring, returning a safely referenced object pointer ready for use.

```cpp
auto proc = process::find(L"Notepad", 7);
if (proc) {
    // ...
    ObDereferenceObject(proc);
}
```

---

## Features

- **Automatic offset resolution**: detects the `ActiveProcessLinks` offset in `EPROCESS` at runtime, supporting multiple Windows versions without recompilation
- **Case-insensitive matching**: ASCII-aware, allocation-free substring search against `PsGetProcessImageFileName`
- **Safe traversal**: validates each list entry with `MmIsAddressValid` and checks backward links before dereferencing
- **Safe reference acquisition**: uses `ObReferenceObjectSafeWithTag` to avoid races against process teardown
- **No heap allocation**: entirely stack-based; safe at `DISPATCH_LEVEL` for offset resolution, `PASSIVE_LEVEL` for the walk itself

---

## How It Works

### 1. Offset Resolution

The byte offset of `ActiveProcessLinks` inside `EPROCESS` varies across Windows versions. On first call, `get_apl_offset` probes a set of known candidate offsets against `PsInitialSystemProcess` and selects the one whose `LIST_ENTRY` is self-consistent (`entry->Flink->Blink == entry`).

The result is cached with a lightweight spin protocol built on `InterlockedCompareExchange`, so concurrent callers never resolve twice and never observe a partial write.

```
Candidates → 0x1D8 (Win11 24H2+)
           → 0x448 (Win10 2004+)
           → 0x2F0 (Win10 1909, Win10 1507+)
           → 0x2E8 (Win10 1703+)
```
[Vergilius Project](https://www.vergiliusproject.com/) used as reference.

### 2. List Walk

Starting from `PsInitialSystemProcess`, each `LIST_ENTRY` is validated before use:

```
[System] → [proc A] → [proc B] → ... → [System]
              ↑
         pointer arithmetic: PEPROCESS = entry_ptr - offset
```

For each entry the image name is retrieved via `PsGetProcessImageFileName` and tested with `string::contains_i`.

### 3. Substring Search

`string::contains_i` performs a branchless-first-character sliding window match between a narrow (`char`) haystack and a wide (`wchar_t`) needle, lowercasing on the fly with a constexpr helper. For needles up to 256 characters the needle is lowercased once into a stack buffer to avoid redundant work.

---

## Requirements

| | |
|---|---|
| **Target** | Windows kernel-mode (ring 0) |
| **Language** | C++20 or newer |
| **IRQL** | `PASSIVE_LEVEL` for `find`, `DISPATCH_LEVEL` for offset resolution |
| **Dependencies** | WDK — `ntddk.h`, `ntstatus.h`, `ntdef.h` |

---

## Safety Notes

> **Always dereference the returned object.**
> `find` calls `ObReferenceObjectSafeWithTag` before returning. Failing to call `ObDereferenceObject` when finished will leak the reference and prevent the process object from being freed.

> **`MmIsAddressValid` is not a substitute for proper locking.**
> The list walk is inherently racy on a live system. This is acceptable for offline inspection or single-shot lookup at driver load time. Do not use this in a hot path on a production system without additional synchronisation.
