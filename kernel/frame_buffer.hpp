#pragma once

#include <vector>
#include <memory>

#include "frame_buffer_config.hpp"
#include "graphics.hpp"
#include "error.hpp"

class FrameBuffer {
    public:
        Error Initialize(const FrameBufferConfig& config);

        // posで指定された位置に、srcで指定されたバッファをコピーする
        Error Copy(Vector2D<int> pos, const FrameBuffer& src);

        FrameBufferWriter& Writer() { return *writer_; }

    private:
        // 忘我領域のサイズ、ピクセルのデータ形式など
        FrameBufferConfig config_{};

        // ピクセルの配列
        std::vector<uint8_t> buffer_{};

        std::unique_ptr<FrameBufferWriter> writer_{};
};

int BitsPerPixel(PixelFormat format);

