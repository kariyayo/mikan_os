/**
 * @file usb/endpoint.hpp
 *
 * エンドポイント設定に関する機能
 *
 * USBケーブルの中には物理的な通信線が1組しかないが、
 * 論理的には機器の中に複数の独立した土管（エンドポイント）を作ることができる。
 * 例えばマウスだと
 *   - エンドポイント 0 : コントロール転送用
 *   - エンドポイント 1 : 割り込み転送用
 *
 * 各エンドポイントは2つの属性を持つ
 *   - エンドポイント番号
 *   - 通信の向き（IN: デバイス→ホスト, OUT: ホスト→デバイス）
 */

#pragma once

#include "error.hpp"

namespace usb {
    enum class EndpointType {
        kControl = 0,     // 初期化・設定
        kIsochronous = 1, // Webカメラ、マイクなど、リアルタイム性最優先（多少データが欠落しても再送しない）
        kBulk = 2,        // USBメモリ、HDDなど、空き時間にまとめて大容量を送る（速度可変、エラー再送あり）
        kInterrupt = 3,   // マウス、キーボードなど、一定周期で定期的にポーリングする
    };

    class EndpointID {
        public:
            constexpr EndpointID() : addr_{0} {}

            constexpr EndpointID(const EndpointID& ep_id) : addr_{ep_id.addr_} {}

            /**
             * xHCIのインデックス番号（1~31）から構成する
             */
            explicit constexpr EndpointID(int addr) : addr_{addr} {}

            /**
             * エンドポイント番号と通信の向きからIDを構成する
             *
             * ep_numは0..15の整数
             * dir_inはControlエンドポイントでは常にtrueに深ければならない
             */
            constexpr EndpointID(int ep_num, bool dir_in) : addr_{ep_num << 1 | dir_in} {}

            EndpointID& operator =(const EndpointID& rhs) {
                addr_ = rhs.addr_;
                return *this;
            }

            /**
             * xHCI用の番号 0..31
             *
             * DCI (Device Context Index) という名前がついてるxHCIで決められた番号。
             * USBの規格上、エンドポイントは番号と向きで識別されるが、DCIはただの数値なので変換する必要がある。
             * 変換ルールは `DCI = エンドポイント番号 * 2 + (INなら1, OUTなら0)`
             * ただし、EP0（エンドポイント番号0）は固定ルールで、コントロール転送専用であり双方向であり、DCIは 1 である
             */
            int Address() const { return addr_; }

            /** エンドポイント番号 0..15 */
            int Number() const { return addr_ >> 1; }

            bool IsIn() const { return addr_ & 1; }

        private:
            int addr_;
    };

    constexpr EndpointID kDefaultControlPipeID{0, true};

    struct EndpointConfig {
        EndpointID ep_id;

        EndpointType ep_type;

        /** このエンドポイントの最大パケットサイズ（バイト） */
        int max_packet_size;

        /** このエンドポイントの制御周期（125*2^(interval-1)マイクロ秒） */
        int interval;
    };
}
