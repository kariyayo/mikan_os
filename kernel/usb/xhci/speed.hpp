/**
 * @file usb/xhci/speed.hpp
 *
 * Protocol Speed IDのデフォルト定義。PSIC == 0 の時のみ有効
 */

#pragma once

namespace usb::xhci {
    const int kFullSpeed = 1;
    const int kLowSpeed = 2;
    const int kHighSpeed = 3;
    const int kSuperSpeed = 4;
    const int kSuperSpeedPlus = 5;
}
