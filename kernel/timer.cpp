#include "timer.hpp"

namespace {
    const uint32_t kCountMax = 0xffffffffu;
    volatile uint32_t& lvt_timer = *reinterpret_cast<uint32_t*>(0xfee00320);     // LVT Timer レジスタ 割り込みの発生方法の設定など
    volatile uint32_t& initial_count = *reinterpret_cast<uint32_t*>(0xfee00380); // Initial Count レジスタ カウンタの初期値
    volatile uint32_t& current_count = *reinterpret_cast<uint32_t*>(0xfee00390); // Current Count レジスタ
    volatile uint32_t& divide_config = *reinterpret_cast<uint32_t*>(0xfee003e0); // Divide Cofiguration レジスタ カウンタの減少スピードの設定
}

void InitializeLAPICTimer() {
    // ビット3,1,0を使って設定する。ビット2は予約領域で0固定
    // つまり以下は 111 と設定している。この場合、分周比は1である
    divide_config = 0b1011;

    // ビット16 割り込みマスク（1=割り込み不許可）
    // ビット17:18 タイマ動作モード（00=単発、01=周期）
    // 割り込みベクタの番号を32
    lvt_timer = (0b001 << 16) | 32;
}

void StartLAPICTimer() {
    initial_count = kCountMax;
}

uint32_t LAPICTimerElapsed() {
    return kCountMax - current_count;
}

void StopLAPICTimer() {
    initial_count = 0;
}

