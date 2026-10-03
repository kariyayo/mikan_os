/**
 * @file usb/xhci/ring.hpp
 *
 * Event Ring, Command Ring, Transfer Ring の関連機能
 */

#pragma once

#include <cstdint>
#include <vector>

#include "error.hpp"
#include "usb/memory.hpp"
#include "usb/xhci/registers.hpp"
#include "usb/xhci/trb.hpp"

namespace usb::xhci {

    /**
     * @brief Command/Transfer Ring を表すクラスでxHCとの通信に使うリングバッファ（TRBの循環キー）
     *
     * ドライバ → xHCチップ 方向へデータを送るのに使う
     */
    class Ring {
        public:
            Ring() = default;
            Ring(const Ring&) = delete;
            ~Ring();
            Ring& operator=(const Ring&) = delete;

            /**
             * @brief リングのメモリ領域を割り当て、メンバを初期化する
             *
             * @param buf_size リングに格納するTRBの総数（末尾の LinkTRB を含む）
             */
            Error Initialize(size_t buf_size);

            template <typename TRBType>
            TRB* Push(const TRBType& trb) {
                return Push(trb.data);
            }

            TRB* Buffer() const { return buf_; }

        private:
            /** @brief TRBの配列 */
            TRB* buf_ = nullptr;

            /** @brief リング全体のTRB個数 */
            size_t buf_size_ = 0;

            /**
             * @brief プロデューサ・サイクル・ステートを表すビット
             *
             * リングバッファのインデックスが一周して先頭に戻ってきた際に、
             * xHCチップに対して、「これは1周目の古いデータではなく、2回目の新しいデータ」だと教える。
             * xHCでは1周するごとに cycle_bit_ を反転させる。1周目は `1` 、2周目は `0` ...
             */
            bool cycle_bit_;

            /** @brief リング上で次に書き込む位置 */
            size_t write_index_;

            /**
             * @brief TRBにcycle bitを設定した上でリング末尾に書き込む
             * write_index_ は変化させない
             */
            void CopyToLast(const std::array<uint32_t, 4>& data);

            /**
             * @brief TRBにcycle bitを設定した上でリング末尾に追加する
             *
             * write_index_ をインクリメントする。その結果 write_index_ がリング末尾に達したら
             * LinkTRBを適切に配置して write_index_ を 0 に戻し、cycle bitを反転させる
             *
             * @return 追加された（リング上の）TRBを指すポインタ
             */
            TRB* Push(const std::array<uint32_t, 4>& data);
    };

    union EventRingSegmentTableEntry {
        std::array<uint32_t, 4> data;
        struct {
            uint64_t ring_segment_base_address;

            uint32_t ring_segment_size : 16;
            uint32_t : 16;

            uint32_t : 32;
        } __attribute__((packed)) bits;
    };

    /**
     * @brief xHCチップ → ドライバ 方向に応答を返すのに使われるリングバッファ
     *
     * ドライバがxHCに対して既読通知を送る際は ERDP レジスタを更新する
     */
    class EventRing {
        public:
            Error Initialize(size_t buf_size, InterrupterRegisterSet* interrupter);

            TRB* ReadDequeuePointer() const {
                return reinterpret_cast<TRB*>(interrupter_->ERDP.Read().Pointer());
            }

            void WriteDequeuePointer(TRB* p);

            bool HasFront() const {
                return Front()->bits.cycle_bit == cycle_bit_;
            }

            TRB* Front() const {
                return ReadDequeuePointer();
            }

            void Pop();

        private:
            TRB* buf_;
            size_t buf_size_;

            bool cycle_bit_;
            EventRingSegmentTableEntry* erst_;
            InterrupterRegisterSet* interrupter_;
    };
}
