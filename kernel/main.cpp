/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル
 */

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"

// pci.hppで<array>をincludeしたら、<new>にも依存することになったのでコメントアウト
//
// /**
//  * 配置new
//  * OSがないベアメタル環境でOSにメモリ確保を依頼できないため、配置newが必要
//  * <new>をインクルードするのではなく自前実装してる
//  */
// void* operator new(size_t size, void* buf) {
//     return buf;
// }

void operator delete(void* obj) noexcept {
}

const PixelColor kDesktopBGColor{45, 118, 237};
const PixelColor kDesktopFGColor{255, 255, 255};

const int kMouseCursorWidth = 15;
const int kMouseCursorHeight = 24;
const char mouse_cursor_shape[kMouseCursorHeight][kMouseCursorWidth + 1] = {
    "@              ",
    "@@             ",
    "@.@            ",
    "@..@           ",
    "@...@          ",
    "@....@         ",
    "@.....@        ",
    "@......@       ",
    "@.......@      ",
    "@........@     ",
    "@.........@    ",
    "@..........@   ",
    "@...........@  ",
    "@............@ ",
    "@......@@@@@@@@",
    "@......@       ",
    "@....@@.@      ",
    "@...@ @.@      ",
    "@..@   @.@     ",
    "@.@    @.@     ",
    "@@      @.@    ",
    "@       @.@    ",
    "         @.@   ",
    "         @@@   ",
};

char pixel_writer_buf[sizeof(RGBResv8BitPerColorPixelWriter)];
PixelWriter* pixel_writer;

char console_buf[sizeof(Console)];
Console* console;

int printk(const char* format, ...) {
    va_list ap;
    int result;
    char s[1024];

    va_start(ap, format);
    result = vsprintf(s, format, ap);
    va_end(ap);

    console->PutString(s);
    return result;
}

extern "C" void KernelMain(const FrameBufferConfig& frame_buffer_config) {
    switch (frame_buffer_config.pixel_format) {
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
    }

    const int kFrameWidth = frame_buffer_config.horizontal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;

    FillRectangle(*pixel_writer,
            {0, 0},
            {kFrameWidth, kFrameHeight - 50},
            kDesktopBGColor);
    FillRectangle(*pixel_writer,
            {0, kFrameHeight - 50},
            {kFrameWidth, 50},
            {1, 8, 17});
    FillRectangle(*pixel_writer,
            {0, kFrameHeight - 50},
            {kFrameWidth / 5, 50},
            {80, 80, 80});
    DrawRectangle(*pixel_writer,
            {10, kFrameHeight - 40},
            {30, 30},
            {160, 160, 160});

    console = new(console_buf) Console{*pixel_writer, kDesktopFGColor, kDesktopBGColor};

    printk("Welcome to MikanOS!\n");

    for (int dy = 0; dy < kMouseCursorHeight; ++dy) {
        for (int dx = 0; dx < kMouseCursorWidth; ++dx) {
            if (mouse_cursor_shape[dy][dx] == '@') {
                pixel_writer->Write(200 + dx, 100 + dy, {0, 0, 0});
            } else if (mouse_cursor_shape[dy][dx] == '.') {
                pixel_writer->Write(200 + dx, 100 + dy, {255, 255, 255});
            }
        }
    }

    auto err = pci::ScanAllBus();
    printk("ScalAllBus: %s\n", err.Name());

    for (int i = 0; i < pci::num_device; ++i) {
        const auto& dev = pci::devices[i];
        auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
        auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
        printk("%d.%d.%d: vend %04x, class %08x, head %02x\n",
                dev.bus, dev.device, dev.function,
                vendor_id, class_code, dev.header_type);
    }

    pci::Device* xhc_dev = nullptr;
    for (int i = 0; i < pci::num_device; ++i) {
        // ベースクラス 0x0c (シリアルバスのコントローラ全体), サブクラス 0x03 (USB), インタフェース 0x03 (xHCI)
        if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x30u)) {
            xhc_dev = &pci::devices[i];

            // Intel製を優先してxHCを探す（著者の経験上、Intel製がメインのコントローラである可能性が高いらしい）
            if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
                break;
            }
        }
    }
    if (xhc_dev) {
        printk("xHC has been found: %d.%d.%d\n", xhc_dev->bus, xhc_dev->device, xhc_dev->function);
    }

    while (1) __asm__("hlt");
}
