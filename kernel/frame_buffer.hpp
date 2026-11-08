#pragma once

#include <vector>
#include <memory>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "error.hpp"

class FrameBuffer {
    public:
        Error Initialize(const FrameBufferConfig& config);

        // dst_posで指定された位置に、srcで指定されたバッファの指定された矩形をコピーする
        Error Copy(Vector2D<int> dst_pos, const FrameBuffer& src, const Rectangle<int>& src_area);

        void Move(Vector2D<int> dst_pos, const Rectangle<int>& src);

        FrameBufferWriter& Writer() { return *writer_; }
        const FrameBufferConfig& Config() const { return config_; }

    private:
        // 忘我領域のサイズ、ピクセルのデータ形式など
        FrameBufferConfig config_{};

        // ピクセルの配列
        std::vector<uint8_t> buffer_{};

        std::unique_ptr<FrameBufferWriter> writer_{};
};

