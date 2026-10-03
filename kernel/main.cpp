/**
 * @file main.cpp
 *
 * カーネル本体のプログラムを書いたファイル
 */

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdarg>

#include <numeric>
#include <vector>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "mouse.hpp"
#include "font.hpp"
#include "console.hpp"
#include "pci.hpp"
#include "logger.hpp"
#include "usb/memory.hpp"
#include "usb/device.hpp"
#include "usb/classdriver/mouse.hpp"
#include "usb/xhci/xhci.hpp"
#include "usb/xhci/trb.hpp"

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

char mouse_cursor_buf[sizeof(MouseCursor)];
MouseCursor* mouse_cursor;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
    mouse_cursor->MoveRelative({displacement_x, displacement_y});
}

void SwitchEhci2Xhci(const pci::Device& xhc_dev) {
    bool intel_ehc_exist = false;
    for (int i = 0; i < pci::num_device; ++i) {
        if (pci::devices[i].class_code.Match(0x0cu, 0x03u, 0x20u) &&
                0x8086 == pci::ReadVendorId(pci::devices[i])) {
            intel_ehc_exist = true;
            break;
        }
    }
    if (!intel_ehc_exist) {
        return;
    }
    uint32_t superspeed_ports = pci::ReadConfReg(xhc_dev, 0xdc); // USB3PRM
    pci::WriteConfReg(xhc_dev, 0xd8, superspeed_ports); // USB3_PSSEN
    uint32_t ehci2xhci_ports = pci::ReadConfReg(xhc_dev, 0xd4); // XUSB2PRM
    pci::WriteConfReg(xhc_dev, 0xd0, ehci2xhci_ports); // XUSB2PR
    Log(kDebug, "SwitchEhci2Xhci: ss = %02, xHCI = %02x\n", superspeed_ports, ehci2xhci_ports);
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
    SetLogLevel(kInfo);

    mouse_cursor = new(mouse_cursor_buf) MouseCursor{
        pixel_writer, kDesktopBGColor, {300, 200}
    };

    auto err = pci::ScanAllBus();
    Log(kDebug, "ScalAllBus: %s\n", err.Name());

    for (int i = 0; i < pci::num_device; ++i) {
        const auto& dev = pci::devices[i];
        auto vendor_id = pci::ReadVendorId(dev.bus, dev.device, dev.function);
        auto class_code = pci::ReadClassCode(dev.bus, dev.device, dev.function);
        Log(kDebug, "%d.%d.%d: vend %04x, class %08x, head %02x\n",
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
        Log(kInfo, "xHC has been found: %d.%d.%d\n", xhc_dev->bus, xhc_dev->device, xhc_dev->function);
    }

    // 追加（実機・PCIeカードだとUEFIが自動的にBit1とBit2を立ててくれなかった
    uint16_t cmd = pci::ReadConfReg(*xhc_dev, 0x04);
    Log(kDebug, "xHC PCI Command before: 0x%04x\n", cmd);
    // 0000 0000 0000 0110 とOR演算して、Bit1とBit2を立てる
    //   - Bit1 (Memory Space Enable): 1 にすると、このデバイスのメモリマップドI/O (MMIO) が有効になる
    //   - Bit2 (Bus Master Enable): 1 にすると、このデバイスがバスマスター（DMA：ダイレクト・メモリ・アクセス）として振る舞えるようになる。xHCI コントローラは、OSのメインメモリにある Ring や TRB（転送要求ブロック）を「自分から読みに行き、結果をメモリに書き込む」という動作をしますが、このビットが0だとコントローラはメモリに一切アクセスできない
    cmd |= 0x0006;
    pci::WriteConfReg(*xhc_dev, 0x04, cmd);
    Log(kDebug, "xHC PCI Command after: 0x%04x\n", pci::ReadConfReg(*xhc_dev, 0x04));
    // 追加ここまで

    // BAR0 (PCIコンフィギュレーション空間の Base Address Register 0）から、xHCのMMIOアドレスを読む
    const WithError<uint64_t> xhc_bar = pci::ReadBar(*xhc_dev, 0);
    Log(kDebug, "ReadBar: %s\n", xhc_bar.error.Name());
    const uint64_t xhc_mmio_base = xhc_bar.value & ~static_cast<uint64_t>(0xf);
    Log(kDebug, "xHC mmio_base = %08lx\n", xhc_mmio_base);

    // xHCの初期化と起動
    usb::xhci::Controller xhc{xhc_mmio_base};

    if (0x8086 == pci::ReadVendorId(*xhc_dev)) {
        SwitchEhci2Xhci(*xhc_dev);
    }
    {
        auto err = xhc.Initialize();
        Log(kDebug, "xhc.Initialize: %s\n", err.Name());
    }
    Log(kInfo, "xHC starting\n");
    xhc.Run();

    // USBポートを調べて接続済みポートの設定をする
    usb::HIDMouseDriver::default_observer = MouseObserver;

    for (int i = 1; i <= xhc.MaxPorts(); ++i) {
        auto port = xhc.PortAt(i);
        Log(kInfo, "Port %d: IsConnected=%d\n", i, port.IsConnected());
//        Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());
        if (port.IsConnected()) {
            // IsConnected() で確認した後、ポートのリセットをする
            // USBデバイスは、まだ正常な通信プロトコルに乗れる状態ではなく、
            // ホスト側から差込口に一定時間リセット信号を流すことで、
            // USBデバイス側の内部コントローラが初期化され、
            // 「自分はどの通信速度で動くか」をxHC側に報告してくる。
            if (auto err = ConfigurePort(xhc, port)) {
                Log(kError, "failed to configure port: %s at %s:%d\n",
                        err.Name(), err.File(), err.Line());
                continue;
            }
        }
    }

    // マウスを動かした時のイベントに対するするポーリング
    while (1) {
        if (auto err = ProcessEvent(xhc)) {
            Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
                    err.Name(), err.File(), err.Line());
//            while (1) __asm__("hlt");
        }
    }

    while (1) __asm__("hlt");
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}
