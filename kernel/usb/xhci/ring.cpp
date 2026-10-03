#include "usb/xhci/ring.hpp"

#include "usb/memory.hpp"

namespace usb::xhci {
    Ring::~Ring() {
        if (buf_ != nullptr) {
            FreeMem(buf_);
        }
    }

    Error Ring::Initialize(size_t buf_size) {
        if (buf_ != nullptr) {
            FreeMem(buf_);
        }

        cycle_bit_ = true;
        write_index_ = 0;
        buf_size_ = buf_size;

        buf_ = AllocArray<TRB>(buf_size_, 64, 64 * 1024);
        if (buf_ == nullptr) {
            return MAKE_ERROR(Error::kNoEnoughMemory);
        }
        memset(buf_, 0, buf_size_ * sizeof(TRB));

        return MAKE_ERROR(Error::kSuccess);
    }

    void Ring::CopyToLast(const std::array<uint32_t, 4>& data) {
        // 0,1,2番目のDWORDを書く
        for (int i = 0; i < 3; ++i) {
            buf_[write_index_].data[i] = data[i];
        }
        // 3番目のDWORDを最後に書く。Bit 0 は Cycle Bit である
        // Cycle Bit は最後に書かないとxHCが書き込み途中のデータを読み取ってしまうことがある
        buf_[write_index_].data[3]
            = (data[3] & 0xfffffffeu) | static_cast<uint32_t>(cycle_bit_);
    }

    TRB* Ring::Push(const std::array<uint32_t, 4>& data) {
        auto trb_ptr = &buf_[write_index_];
        CopyToLast(data);

        ++write_index_;
        if (write_index_ == buf_size_ - 1) {
            // LinkTRBを置くことでリングバッファを実現
            // Cycle Bitを使うことで1周した際に古いデータのままかどうかを判断できる
            LinkTRB link{buf_};
            link.bits.toggle_cycle = true; // Cycle Bit を反転してねとxHCに伝える
            CopyToLast(link.data);
            write_index_ = 0; // ポインタを先頭に戻る
            cycle_bit_ = !cycle_bit_; // Cycle Bit を反転
        }

        return trb_ptr;
    }

    Error EventRing::Initialize(size_t buf_size,
            InterrupterRegisterSet* interrupter) {
        if (buf_ != nullptr) {
            FreeMem(buf_);
        }

        cycle_bit_ = true;
        buf_size_ = buf_size;
        interrupter_ = interrupter;

        buf_ = AllocArray<TRB>(buf_size_, 64, 64 * 1024);
        if (buf_ == nullptr) {
            return MAKE_ERROR(Error::kNoEnoughMemory);
        }
        memset(buf_, 0, buf_size_ * sizeof(TRB));

        // 複数のバッファを束ねたテーブル（目次のようなもの）
        erst_ = AllocArray<EventRingSegmentTableEntry>(1, 64, 64 * 1024);
        if (erst_ == nullptr) {
            FreeMem(buf_);
            return MAKE_ERROR(Error::kNoEnoughMemory);
        }
        erst_[0].bits.ring_segment_base_address = reinterpret_cast<uint64_t>(buf_);
        erst_[0].bits.ring_segment_size = buf_size_;

        // テーブルの行数をxHCチップに伝える
        ERSTSZ_Bitmap erstsz = interrupter_->ERSTSZ.Read();
        erstsz.SetSize(1);
        interrupter_->ERSTSZ.Write(erstsz);

        WriteDequeuePointer(&buf_[0]);

        // テーブルのアドレスをxHCチップに伝える
        ERSTBA_Bitmap erstba = interrupter_->ERSTBA.Read();
        erstba.SetPointer(reinterpret_cast<uint64_t>(erst_));
        interrupter_->ERSTBA.Write(erstba);

        return MAKE_ERROR(Error::kSuccess);
    }

    void EventRing::WriteDequeuePointer(TRB* p) {
        // ERDP (既読通知レジスタ) に Event Ring の先頭アドレスを書き込む
        auto erdp = interrupter_->ERDP.Read();
        erdp.SetPointer(reinterpret_cast<uint64_t>(p));
        interrupter_->ERDP.Write(erdp);
    }

    void EventRing::Pop() {
        auto p = ReadDequeuePointer() + 1;
        TRB* segment_begin = reinterpret_cast<TRB*>(erst_[0].bits.ring_segment_base_address);
        TRB* segment_end = segment_begin + erst_[0].bits.ring_segment_size;
        if (p == segment_end) {
            p = segment_begin;
            cycle_bit_ = !cycle_bit_;
        }
        WriteDequeuePointer(p);
    }
}
