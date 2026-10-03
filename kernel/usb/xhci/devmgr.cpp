#include "usb/xhci/devmgr.hpp"

#include "usb/memory.hpp"

namespace usb::xhci {
    Error DeviceManager::Initialize(size_t max_slots) {
        max_slots_ = max_slots;

        devices_ = AllocArray<Device*>(max_slots_ + 1, 0, 0);
        if (devices_ == nullptr) {
            return MAKE_ERROR(Error::kNoEnoughMemory);
        }

        device_context_pointers_ = AllocArray<DeviceContext*>(max_slots_ + 1, 64, 4096);
        if (device_context_pointers_ == nullptr) {
            FreeMem(devices_);
            return MAKE_ERROR(Error::kNoEnoughMemory);
        }

        for (size_t i = 0; i <= max_slots_; ++i) {
            devices_[i] = nullptr;
            device_context_pointers_[i] = nullptr;
        }

        return MAKE_ERROR(Error::kSuccess);
    }

    DeviceContext** DeviceManager::DeviceContexts() const {
        return device_context_pointers_;
    }

    Device* DeviceManager::FindBySlot(uint8_t slot_id) const {
        if (slot_id > max_slots_) {
            nullptr;
        }
        return devices_[slot_id];
    }

    Error DeviceManager::AllocDevice(uint8_t slot_id, DoorbellRegister* dbreg) {
        if (slot_id > max_slots_) {
            return MAKE_ERROR(Error::kInvalidSlotID);
        }

        if (devices_[slot_id] != nullptr) {
            return MAKE_ERROR(Error::kAlreadyAllocated);
        }

        devices_[slot_id] = AllocArray<Device>(1, 64, 4096);
        new(devices_[slot_id]) Device(slot_id, dbreg);
        return MAKE_ERROR(Error::kSuccess);
    }

    Error DeviceManager::LoadDCBAA(uint8_t slot_id) {
        if (slot_id > max_slots_) {
            return MAKE_ERROR(Error::kInvalidSlotID);
        }
        auto dev = devices_[slot_id];
        device_context_pointers_[slot_id] = dev->DeviceContext();
        return MAKE_ERROR(Error::kSuccess);
    }
}
