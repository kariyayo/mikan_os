#pragma once

#include <stdint.h>

struct MemoryMap {
    unsigned long long buffer_size;
    void* buffer;
    unsigned long long map_size;
    unsigned long long map_key;
    unsigned long long descriptor_size;
    uint32_t descriptor_version;
};

struct MemoryDescriptor {
    uint32_t type;
    uintptr_t physical_start;
    uintptr_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

#ifdef __cplusplus
enum class MemoryType {
    kEfiReserveMemoryType,
    kEfiLoaderCode,
    kEfiLoaderData,
    kEfiBootServicesCode,
    kEfiBootServicesData,
    kEfiRuntimeServicesCode,
    kEfiRuntimeServicesData,
    kEfiConventionalMemory,
    kEfiUnusableMemory,
    kEfiACPIReclaimMemory,
    kEfiACPIMemoryNVS,
    kEfiMemoryMappedIO,
    kEfiMemoryMappedIOPortSpace,
    kEfiPalCode,
    kEfiPersistentMemory,
    kEfiMaxMemoryType
};

inline bool operator==(uint32_t lhs, MemoryType rhs) {
    return lhs == static_cast<uint32_t>(rhs);
}

inline bool operator==(MemoryType lhs, uint32_t rhs) {
    return rhs == lhs;
}

/**
 * @brief 指定されたメモリタイプが空き領域かどうかを判定する
 */
inline bool IsAvailable(MemoryType memory_type) {
    // UEFIアプリケーションでExitBootServices()を呼び出した後、以下の3種類は空き領域として扱って良いことになってる
    return
        memory_type == MemoryType::kEfiBootServicesCode ||
        memory_type == MemoryType::kEfiBootServicesData ||
        memory_type == MemoryType::kEfiConventionalMemory;
}

const int kUEFIPageSize = 4096;
#endif

