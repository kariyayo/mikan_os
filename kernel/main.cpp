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
#include "memory_map.hpp"
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
#include "interrupt.hpp"
#include "asmfunc.h"
#include "queue.hpp"
#include "segment.hpp"
#include "paging.hpp"
#include "memory_manager.hpp"
#include "window.hpp"
#include "layer.hpp"
#include "timer.hpp"

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

char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager* memory_manager;


unsigned int mouse_layer_id;

void MouseObserver(int8_t displacement_x, int8_t displacement_y) {
    layer_manager->MoveRelative(mouse_layer_id, {displacement_x, displacement_y});
    StartLAPICTimer();
    layer_manager->Draw();
    auto elapsed = LAPICTimerElapsed();
    StopLAPICTimer();
    printk("MouseObserver: elapsed = %u\n", elapsed);
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


// 割り込みハンドラ
usb::xhci::Controller* xhc;

struct Message {
    enum Type {
        kInterruptXHCI,
    } type;
};

ArrayQueue<Message>* main_queue;

__attribute__((interrupt))
void IntHandlerXHCI(InterruptFrame* frame) {
    main_queue->Push(Message{Message::kInterruptXHCI});
    NotifyEndOfInterrupt();
}

/**
 * カーネルで使うスタック領域
 * asmfunc.asmで、UEFIのスタックからRSPレジスタをkernel_main_stackの末尾ににセットしてる
 */
alignas(16) uint8_t kernel_main_stack[1024 * 1024];

// asmfunc.asmから kernel_main_stack をセットした後に呼ばれる
extern "C" void KernelMainNewStack(const FrameBufferConfig& frame_buffer_config_ref,
        const MemoryMap& memory_map_ref) {
    FrameBufferConfig frame_buffer_config{frame_buffer_config_ref};
    MemoryMap memory_map{memory_map_ref};

    switch (frame_buffer_config.pixel_format) {
        case kPixelRGBResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) RGBResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
        case kPixelBGRResv8BitPerColor:
            pixel_writer = new(pixel_writer_buf) BGRResv8BitPerColorPixelWriter{frame_buffer_config};
            break;
    }

    // コンソール
    DrawDesktop(*pixel_writer);
    console = new(console_buf) Console{kDesktopFGColor, kDesktopBGColor};
    console->SetWriter(pixel_writer);
    printk("Welcome to MikanOS!\n");
    SetLogLevel(kInfo);

    // セグメンテーションの設定
    SetupSegments();
    const uint16_t kernel_cs = 1 << 3;
    const uint16_t kernel_ss = 2 << 3;
    SetDSAll(0);
    SetCSSS(kernel_cs, kernel_ss);
    SetupIdentityPageTable();

    // メモリ管理
    ::memory_manager = new(memory_manager_buf) BitmapMemoryManager;

    const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
    uintptr_t available_end = 0;
    // UEFIアプリケーションから渡されたメモリマップをmemory_managerに伝える
    for (uintptr_t iter = memory_map_base;
            iter < memory_map_base + memory_map.map_size;
            iter += memory_map.descriptor_size) {
        auto desc = reinterpret_cast<const MemoryDescriptor*>(iter);
        if (available_end < desc->physical_start) {
            memory_manager->MarkAllocated(
                    FrameID{available_end / kBytesPerFrame},
                    (desc->physical_start - available_end) / kBytesPerFrame);
        }
        const auto physical_end = desc->physical_start + desc->number_of_pages * kUEFIPageSize;
        if (IsAvailable(static_cast<MemoryType>(desc->type))) {
            available_end = physical_end;
        } else {
            memory_manager->MarkAllocated(
                    FrameID{desc->physical_start / kBytesPerFrame},
                    desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
        }
    }
    memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end / kBytesPerFrame});
    if (auto err = InitializeHeap(*memory_manager)) {
        Log(kError, "failed to allocate pages: %s at %s:%d\n",
                err.Name(), err.File(), err.Line());
        exit(1);
    }

//    // memory_mapの確認
//    const std::array available_memory_types{
//        MemoryType::kEfiBootServicesCode,
//        MemoryType::kEfiBootServicesData,
//        MemoryType::kEfiConventionalMemory,
//    };
//    printk("memory_map: %p\n", &memory_map);
//    for (uintptr_t iter = reinterpret_cast<uintptr_t>(memory_map->buffer);
//            iter < reinterpret_cast<uintptr_t>(memory_map->buffer) + memory_map->map_size;
//            iter += memory_map->descriptor_size) {
//        auto desc = reinterpret_cast<MemoryDescriptor*>(iter);
//        for (int i = 0; i < available_memory_types.size(); ++i) {
//            if (desc->type == available_memory_types[i]) {
//                printk("type = %u, phys = %08lx - %08lx, pages = %lu, attr = %08lx\n",
//                        desc->type,
//                        desc->physical_start,
//                        desc->physical_start + desc->number_of_pages * 4096 - 1,
//                        desc->number_of_pages,
//                        desc->attribute);
//            }
//        }
//    }

    std::array<Message, 32> main_queue_data;
    ArrayQueue<Message> main_queue{main_queue_data};
    ::main_queue = &main_queue;

    // PCIデバイス
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

    // 割り込みハンドラを登録 (InterruptVector::kXHCIに対する)
    SetIDTEntry(idt[InterruptVector::kXHCI], MakeIDTAttr(DescriptorType::kInterruptGate, 0),
            reinterpret_cast<uint64_t>(IntHandlerXHCI), kernel_cs);
    LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0])); // 割り込み記述子をCPUに登録

    // MSI割り込みを有効化 (InterruptVector::kXHCIを指定する)
    const uint8_t bsp_local_apic_id =
        *reinterpret_cast<const uint32_t*>(0xfee00020) >> 24; // CPUコア毎に割り当てられる APIC ID
    pci::ConfigureMSIFixedDestination(
            *xhc_dev, bsp_local_apic_id,
            pci::MSITriggerMode::kLevel, pci::MSIDeliveryMode::kFixed,
            InterruptVector::kXHCI, 0);

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

    ::xhc = &xhc;
    __asm__("sti"); // CPUの割り込みを有効化

    // USBポートを調べて接続済みポートの設定をする
    usb::HIDMouseDriver::default_observer = MouseObserver;

    for (int i = 1; i <= xhc.MaxPorts(); ++i) {
        auto port = xhc.PortAt(i);
        Log(kDebug, "Port %d: IsConnected=%d\n", i, port.IsConnected());
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

    const int kFrameWidth = frame_buffer_config.horizontal_resolution;
    const int kFrameHeight = frame_buffer_config.vertical_resolution;

    // 背景のレイヤーを描画
    auto bgwindow = std::make_shared<Window>(
            kFrameWidth, kFrameHeight, frame_buffer_config.pixel_format);
    auto bgwriter = bgwindow->Writer();
    DrawDesktop(*bgwriter);
    console->SetWriter(bgwriter);

    // マウスのレイヤーを描画
    auto mouse_window = std::make_shared<Window>(
            kMouseCursorWidth, kMouseCursorHeight, frame_buffer_config.pixel_format);
    mouse_window->SetTransparentColor(kMouseTransparentColor);
    DrawMouseCursor(mouse_window->Writer(), {0, 0});

    // レイヤーを管理
    FrameBuffer screen;
    if (auto err = screen.Initialize(frame_buffer_config)) {
        Log(kError, "failed to initialize frame buffer: %s at %s:%d\n",
                err.Name(), err.File(), err.Line());
    }
    layer_manager = new LayerManager;
    layer_manager->SetWriter(&screen);
    auto bglayer_id = layer_manager->NewLayer()
        .SetWindow(bgwindow)
        .Move({0, 0})
        .ID();
    mouse_layer_id = layer_manager->NewLayer()
        .SetWindow(mouse_window)
        .Move({200, 200})
        .ID();
    layer_manager->UpDown(bglayer_id, 0);
    layer_manager->UpDown(mouse_layer_id, 1);
    layer_manager->Draw();


    while (true) {
        __asm__("cli"); // CPUの割り込みフラグを0にする命令。0だとCPUは外部割り込みを受け取らなくなる
        if (main_queue.Count() == 0) {
            __asm__("sti\n\thlt");
            continue;
        }

        Message msg = main_queue.Front();
        main_queue.Pop();
        __asm__("sti"); // CPUの割り込みフラグを1にする命令

        switch (msg.type) {
            case Message::kInterruptXHCI:
                while (xhc.PrimaryEventRing()->HasFront()) {
                    if (auto err = ProcessEvent(xhc)) {
                        Log(kError, "Error while ProcessEvent: %s at %s:%d\n",
                                err.Name(), err.File(), err.Line());
                    }
                }
                break;
            default:
                Log(kError, "Unknown message type: %d\n", msg.type);
        }
    }
}

extern "C" void __cxa_pure_virtual() {
    while (1) __asm__("hlt");
}
