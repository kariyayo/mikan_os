/**
 * @file usb/xhci/devmgr.hpp
 *
 * USBデバイスの管理機能
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "error.hpp"
#include "usb/xhci/context.hpp"
#include "usb/xhci/device.hpp"

namespace usb::xhci {
    class DeviceManager {
        public:
            Error Initialize(size_t max_slots);
            DeviceContext** DeviceContexts() const;
            Device* FindBySlot(uint8_t slot_id) const;
            Error AllocDevice(uint8_t slot_id, DoorbellRegister* dbreg);
            Error LoadDCBAA(uint8_t slot_id);

        private:
            DeviceContext** device_context_pointers_;
            size_t max_slots_;

            Device** devices_;
    };
}
