#pragma once

#include <cstdint>
#include <array>

#include "error.hpp"

namespace pci {
    /** @brief CONFIG_ADDRESSレジスタのIOポートアドレス */
    const uint16_t kConfigAddress = 0x0cf8;
    /** @brief CONFIG_DATAレジスタのIOポートアドレス */
    const uint16_t kConfigData = 0x0cfc;

    /** @brief PCIデバイスのクラスコード */
    struct ClassCode {
        uint8_t base, sub, interface;

        bool Match(uint8_t b) { return b == base; }
        bool Match(uint8_t b, uint8_t s) { return Match(b) && s == sub; }
        bool Match(uint8_t b, uint8_t s, uint8_t i) {
            return Match(b, s) && i == interface;
        }
    };

    /**
     * @brief PCIデバイスを操作するための基礎データを格納する
     *
     * バス番号、デバイス番号、ファンクション番号はデバイスを特定するのに必須。
     */
    struct Device {
        uint8_t bus, device, function, header_type;
        ClassCode class_code;
    };

    /** @brief CONFIG_ADDRESSに指定された整数を書き込む */
    void WriteAddress(uint32_t address);
    /** @brief CONFIG_DATAに指定された整数を書き込む */
    void WriteData(uint32_t value);
    /** @brief CONFIG_DATAから32ビット整数を読み込む */
    uint32_t ReadData();

    /** @brief ベンダーIDレジスタを読み取る（全ヘッダタイプ共通） */
    uint16_t ReadVendorId(uint8_t bus, uint8_t device, uint8_t function);

    /** @brief デバイスIDレジスタを読み取る（全ヘッダタイプ共通） */
    uint16_t ReadDeviceId(uint8_t bus, uint8_t device, uint8_t function);

    /** @brief ヘッダータイプレジスタを読み取る（全ヘッダタイプ共通） */
    uint8_t ReadHeaderType(uint8_t bus, uint8_t device, uint8_t function);

    /** @brief クラスコードレジスタを読み取る（全ヘッダタイプ共通） */
    ClassCode ReadClassCode(uint8_t bus, uint8_t device, uint8_t function);

    inline uint16_t ReadVendorId(const Device& dev) {
        return ReadVendorId(dev.bus, dev.device, dev.function);
    }

    uint32_t ReadConfReg(const Device& dev, uint8_t reg_addr);

    void WriteConfReg(const Device& dev, uint8_t reg_addr, uint32_t value);

    /**
     * @brief バス番号レジスタを読み取る（ヘッダタイプ1）
     *
     * 返される32ビット整数の構造は以下の通り
     * - 23:16 : サブオーディネイトバス番号
     * - 15:8  : セカンダリバス番号
     * - 7:0   : リビジョン
     */
    uint32_t ReadBusNumbers(uint8_t bus, uint8_t device, uint8_t function);

    /** @brief 単一ファンクションの場合 true を返す */
    bool IsSingleFunctionDevice(uint8_t header_type);

    /** @brief ScanAllBus()により発見されたPCIデバイス一覧 */
    inline std::array<Device, 32> devices;

    /** @brief devicesの有効な要素の数 */
    inline int num_device;

    /**
     * @brief PCIデバイスを全て探索しdevicesに格納する
     */
    Error ScanAllBus();

    constexpr uint8_t CalcBarAddress(unsigned int bar_index) {
        return 0x10 + 4 * bar_index;
    }

    WithError<uint64_t> ReadBar(Device& device, unsigned int bar_index);


    union CapabilityHeader {
        uint32_t data;
        struct {
            uint32_t cap_id : 8;
            uint32_t next_ptr : 8;
            uint32_t cap : 16;
        } __attribute__((packed)) bits;
    } __attribute__((packed));

    const uint8_t kCapabilityMSI = 0x05;
    const uint8_t kCapabilityMSIX = 0x11;

    CapabilityHeader ReadCapabilityHeader(const Device& dev, uint8_t addr);

    /**
     * @brief MSI Capability 構造
     *
     * MSI Cpability は64ビットサポートの有無などで亜種がたくさんある
     * この構造体は各亜種に対応するために最大の亜種に合わせてメンバーを定義してある
     */
    struct MSICapability {
        union {
            uint32_t data;
            struct {
                uint32_t cap_id : 8;
                uint32_t next_ptr : 8;
                uint32_t msi_enable : 1;
                uint32_t multi_msg_capable : 3;
                uint32_t multi_msg_enable : 3;
                uint32_t addr_64_capable : 1;
                uint32_t per_vector_mask_capable : 1;
                uint32_t : 7;
            } __attribute__((packed)) bits;
        } __attribute__((packed)) header ;

        uint32_t msg_addr;
        uint32_t msg_upper_addr;
        uint32_t msg_data;
        uint32_t mask_bits;
        uint32_t pending_bits;
    } __attribute__((packed));

    /**
     * @brief MSI または MSI-X 割り込みを設定する
     *
     * @param dev 設定対象のPCIデバイス
     * @param msg_addr 割り込み発生時にメッセージを書き込む先のアドレス
     * @param msg_data ワイこみ発生時に書き込むメッセージの値
     * @param num_vector_exponent 割り当てるベクタ数（2^n の n を指定）
     */
    Error ConfigureMSI(const Device& dev, uint32_t msg_addr, uint32_t msg_data,
            unsigned int num_vector_exponent);

    enum class MSITriggerMode {
        kEdge = 0,
        kLevel = 1,
    };

    enum class MSIDeliveryMode {
        kFixed          = 0b000,
        kLowestPriority = 0b001,
        kSMI            = 0b010,
        kNMI            = 0b100,
        kINIT           = 0b101,
        kExtINT         = 0b111,
    };

    /**
     * @brief xHCに対してMSI割り込みを有効化するための設定をする
     */
    Error ConfigureMSIFixedDestination(
            const Device& dev, uint8_t apic_id,
            MSITriggerMode trigger_mode, MSIDeliveryMode delivery_mode,
            uint8_t vector, unsigned int num_vector_exponent);
}
