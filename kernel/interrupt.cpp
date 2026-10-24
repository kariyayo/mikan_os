/**
 * @file interrupt.cpp
 *
 * 割り込み用のプログラムを集めたファイル
 */

#include "interrupt.hpp"

std::array<InterruptDescriptor, 256> idt;

void SetIDTEntry(InterruptDescriptor& desc,
        InterruptDescriptorAttribute attr,
        uint64_t offset,
        uint16_t segment_selector) {
    desc.attr = attr;
    desc.offset_low = offset & 0xffffu;
    desc.offset_middle = (offset >> 16) & 0xffffu;
    desc.offset_high = offset >> 32;
    desc.segment_selector = segment_selector;
}

void NotifyEndOfInterrupt() {
    // 0xfee00000 ~ 0xfee00400 はレジスタが置かれている
    volatile auto end_of_interrupt = reinterpret_cast<uint32_t*>(0xfee000b0);
    // レジスタへの書き込みをCPU側のトリガーにしている
    *end_of_interrupt = 0;
}
