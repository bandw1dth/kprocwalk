#pragma once
#include <cstdint>
#include <kernelspecs.h>

#include "imports.hpp"

namespace string {

[[nodiscard]] static constexpr unsigned char ascii_lower(unsigned char c) noexcept {
    return (c - 'A' < 26u) ? (c | 0x20) : c;
}

[[nodiscard]] static constexpr wchar_t ascii_lower(wchar_t c) noexcept {
    return (c - L'A' < 26u) ? (c | 0x20) : c;
}

_IRQL_requires_max_(DISPATCH_LEVEL) static bool contains_i(
    _In_reads_bytes_opt_(hay_max) const char *hay,
    size_t                                    hay_max,
    _In_reads_opt_(nlen) const wchar_t       *needle,
    size_t                                    nlen
) noexcept {
    if (!hay || !needle || nlen == 0) [[unlikely]]
        return false;

    size_t hlen = 0;
    while (hlen < hay_max && hay[hlen] != '\0') ++hlen;

    if (hlen < nlen) return false;

    wchar_t    lowered[256];
    const bool use_local = (nlen <= 256);
    if (use_local)
        for (size_t k = 0; k < nlen; ++k) lowered[k] = ascii_lower(needle[k]);

    auto get_nc = [&](size_t idx) -> wchar_t { return use_local ? lowered[idx] : ascii_lower(needle[idx]); };

    const wchar_t first_nc = get_nc(0);
    const wchar_t last_nc  = get_nc(nlen - 1);

    for (size_t i = 0; i + nlen <= hlen; ++i) {
        if (ascii_lower(static_cast<unsigned char>(hay[i])) != first_nc) continue;
        if (ascii_lower(static_cast<unsigned char>(hay[i + nlen - 1])) != last_nc) continue;

        bool matched = true;
        for (size_t j = 1; j < nlen - 1; ++j) {
            if (ascii_lower(static_cast<unsigned char>(hay[i + j])) != get_nc(j)) {
                matched = false;
                break;
            }
        }
        if (matched) return true;
    }

    return false;
}

} // namespace string

namespace process {

static volatile long g_apl_offset_resolved = 0;
static std::uint32_t g_apl_offset          = 0;

[[nodiscard]] _IRQL_requires_max_(DISPATCH_LEVEL) static std::uint32_t get_apl_offset() noexcept {
    if (g_apl_offset_resolved == 2) [[likely]]
        return g_apl_offset;

    if (InterlockedCompareExchange(&g_apl_offset_resolved, 1, 0) == 0) {
        const auto              base                = reinterpret_cast<std::uint8_t *>(PsInitialSystemProcess);
        constexpr std::uint32_t offset_candidates[] = {0x1D8, 0x448, 0x2F0, 0x2E8};

        for (const auto offset : offset_candidates) {
            const auto entry = reinterpret_cast<const PLIST_ENTRY>(base + offset);
            if (entry->Flink && entry->Blink && entry->Flink->Blink == entry) {
                g_apl_offset = offset;
                break;
            }
        }

        KeMemoryBarrier();
        InterlockedExchange(&g_apl_offset_resolved, 2);
    } else {
        while (g_apl_offset_resolved != 2) _mm_pause();
    }

    return g_apl_offset;
}

[[nodiscard]] _IRQL_requires_max_(PASSIVE_LEVEL) static PEPROCESS find(
    _In_reads_opt_(name_len) const wchar_t *name_substr, size_t name_len
) noexcept {
    if (!name_substr || name_len == 0) [[unlikely]]
        return nullptr;

    const std::uint32_t offset = get_apl_offset();
    if (offset == 0) [[unlikely]]
        return nullptr;

    const auto start_base  = reinterpret_cast<std::uint8_t *>(PsInitialSystemProcess);
    const auto start_entry = reinterpret_cast<PLIST_ENTRY>(start_base + offset);

    PLIST_ENTRY curr = start_entry;
    do {
        PLIST_ENTRY const next = curr->Flink;

        if (!MmIsAddressValid(next) || !MmIsAddressValid(static_cast<void *>(&next->Blink))) [[unlikely]]
            break;
        if (next->Blink != curr) [[unlikely]]
            break;

        const auto process = reinterpret_cast<PEPROCESS>(reinterpret_cast<std::uint8_t *>(curr) - offset);
        if (MmIsAddressValid(process)) {
            const auto image = PsGetProcessImageFileName(process);
            const bool found =
                image && string::contains_i(reinterpret_cast<const char *>(image), 15, name_substr, name_len);
            if (found && ObReferenceObjectSafeWithTag(process, 'PsrP')) return process;
        }

        curr = next;
    } while (curr != start_entry);

    return nullptr;
}

} // namespace process
