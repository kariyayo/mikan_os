#include "segment.hpp"

#include "asmfunc.h"

namespace {
    std::array<SegmentDescriptor, 3> gdt;
}

void SetCodeSegment(SegmentDescriptor& desc,
        DescriptorType type,
        unsigned int descriptor_privilege_level,
        uint32_t base,
        uint32_t limit) {
    desc.data = 0;

    desc.bits.base_low = base & 0xffffu;
    desc.bits.base_middle = (base >> 16) & 0xffu;
    desc.bits.base_high = (base >> 24) & 0xffu;

    desc.bits.limit_low = limit & 0xffffu;
    desc.bits.limit_high = (limit >> 16) & 0xfu;

    desc.bits.type = type;
    desc.bits.system_segment = 1; // 1ならコードまたはデータセグメント
    desc.bits.descriptor_privilege_level = descriptor_privilege_level; // 権限レベル
    desc.bits.present = 1; // 1なら有効
    desc.bits.available = 0;
    desc.bits.long_mode = 1; // 1なら64ビットモード用のコードセグメント
    desc.bits.default_operation_size = 0; // long_mode = 1 の場合は 0 にする
    desc.bits.granularity = 1; // 1ならリミットを4KiB単位
}

void SetDataSegment(SegmentDescriptor& desc,
        DescriptorType type,
        unsigned int descriptor_privilege_level,
        uint32_t base,
        uint32_t limit) {
    SetCodeSegment(desc, type, descriptor_privilege_level, base, limit);
    desc.bits.long_mode = 0;
    desc.bits.default_operation_size = 1;
}

void SetupSegments() {
    gdt[0].data = 0;
    SetCodeSegment(gdt[1], DescriptorType::kExecuteRead, 0, 0, 0xfffff);
    SetDataSegment(gdt[2], DescriptorType::kReadWrite, 0, 0, 0xfffff);

    // GDTをCPUに登録する
    LoadGDT(sizeof(gdt) - 1, reinterpret_cast<uintptr_t>(&gdt[0]));
}

