/**
 * @file usb/xhci/device.hpp
 *
 * USBデバイスを表すクラスと関連機能
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "error.hpp"
#include "usb/device.hpp"
#include "usb/arraymap.hpp"
#include "usb/xhci/context.hpp"
#include "usb/xhci/trb.hpp"
#include "usb/xhci/registers.hpp"

namespace usb::xhci {
    class Device : public usb::Device {
        public:
            Device(uint8_t slot_id, DoorbellRegister* dbreg);

            Error Initialize();

            DeviceContext* DeviceContext() { return &ctx_; }
            InputContext* InputContext() { return &input_ctx_; }

            uint8_t SlotID() const { return slot_id_; }

            Error ControlIn(EndpointID ep_id, SetupData setup_data,
                    void* buf, int len, ClassDriver* issuer) override;
            Error ControlOut(EndpointID ep_id, SetupData setup_data,
                    const void* buf, int len, ClassDriver* issuer) override;
            Error InterruptIn(EndpointID ep_id, void* buf, int len) override;

            Ring* AllocTransferRing(DeviceContextIndex index, size_t buf_size);

            Error OnTransferEventReceived(const TransferEventTRB& trb);

        private:
            alignas(64) struct DeviceContext ctx_;
            alignas(64) struct InputContext input_ctx_;

            const uint8_t slot_id_;
            DoorbellRegister* const dbreg_;

            std::array<Ring*, 31> transfer_rings_; // index = dci -1

            /**
             * @brief コントロール転送が完了した際にDataStageTRBやStatusStageTRBから対応するSetupStageTRBを検索するためのマップ
             */
            ArrayMap<const void*, const SetupStageTRB*, 16> setup_stage_map_{};
    };
}
