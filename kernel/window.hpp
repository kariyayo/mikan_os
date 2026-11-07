/**
 * @file window.hpp
 *
 * 表示領域を表すWindowクラスを提供する。
 */

#pragma once

#include <vector>
#include <optional>
#include "graphics.hpp"

/**
 * @brief Windowクラスはグラフィックの表示領域を表す
 *
 * タイトルやメニューがあるウィンドウだけでなく、マウスカーソルの表じ領域なども対象とする。
 */
class Window {
    public:

        /**
         * @brief WindowWriter は Window と関連づけられた PixelWriter を提供する
         */
        class WindowWriter : public PixelWriter {
            public:
                WindowWriter(Window& window) : window_{window} {}

                /**
                 * @brief 指定された位置に指定された色を描く
                 */
                virtual void Write(int x, int y, const PixelColor& c) override {
                    window_.At(x, y) = c;
                }

                /** @brief Width は関連づけられたWindowの横幅をピクセル単位で返す */
                virtual int Width() const override { return window_.Width(); }

                /** @brief Height は関連づけられたWindowの高さをピクセル単位で返す */
                virtual int Height() const override { return window_.Height(); }

            private:
                Window& window_;
        };

        /** @brief 指定されたピクセル数の平面描画領域を作成する */
        Window(int width, int height);
        ~Window() = default;
        Window(const Window& rhs) = delete;
        Window& operator=(const Window& rhs) = delete;

        /**
         * @brief 与えられたPixelWriterにこのウィンドウの表示領域を描画する 
         *
         * @param writer 描画先
         * @param position writerの左上を基準とした描画位置
         */
        void DrawTo(PixelWriter& writer, Vector2D<int> position);
        /** @brief 透明色を設定する */
        void SetTransparentColor(std::optional<PixelColor> c);
        /** @brief このインスタンスに紐づいたWindowWriterを取得する */
        WindowWriter* Writer();

        /** @brief 指定した位置のピクセルを返す */
        PixelColor& At(int x, int y);
        /** @brief 指定した位置のピクセルを返す */
        const PixelColor& At(int x, int y) const;

        int Width() const;
        int Height() const;

    private:
        int width_, height_;
        std::vector<std::vector<PixelColor>> data_{};
        WindowWriter writer_{*this};
        std::optional<PixelColor> transparent_color_{std::nullopt};
};

