#include "usb/xhci/port.hpp"

#include "usb/xhci/xhci.hpp"
#include "usb/xhci/registers.hpp"

namespace usb::xhci {
    uint8_t Port::Number() const {
        return port_num_;
    }

    bool Port::IsConnected() const {
        return port_reg_set_.PORTSC.Read().bits.current_connect_status;
    }

    bool Port::IsEnabled() const {
        return port_reg_set_.PORTSC.Read().bits.port_enabled_disabled;
    }

    bool Port::IsPortResetChanged() const {
        return port_reg_set_.PORTSC.Read().bits.port_reset_change;
    }

    int Port::Speed() const {
        return port_reg_set_.PORTSC.Read().bits.port_speed;
    }

    Error Port::Reset() {
        auto portsc = port_reg_set_.PORTSC.Read();

        // 強制的に0にしたいフラグを落とす。以下の値と論理積することで。
        // 0000 1110 0000 0000 1100 0011 1110 0000
        portsc.data[0] &= 0x0e00c3e0u;

        // bit4   (0x00000020) : PR (Port Rest) リセット信号を流す
        // bit 17 (0x00020000) : CSC (Connect Status Change) 接続検知フラグをクリア
        // 上記の2つのビットを立てる
        portsc.data[0] |= 0x00020010u;

        port_reg_set_.PORTSC.Write(portsc);
        while (port_reg_set_.PORTSC.Read().bits.port_reset);
        return MAKE_ERROR(Error::kSuccess);
    }
}
