/**
 * @file usb/device.hpp
 *
 * USBデバイスを表すクラスと関連機能
 * USB 2.0/3.0 の標準規格書に基づく処理を記述、EHCIやxHCIに関することは知らない
 */

#pragma once

#include <array>

#include "error.hpp"
#include "usb/setupdata.hpp"
#include "usb/endpoint.hpp"
#include "usb/arraymap.hpp"

namespace usb {
    class ClassDriver;

    class Device {
        public:
            virtual ~Device();
            virtual Error ControlIn(EndpointID ep_id, SetupData setup_data,
                    void* buf, int len, ClassDriver* issuer);
            virtual Error ControlOut(EndpointID ep_id, SetupData setup_data,
                    const void* buf, int len, ClassDriver* issuer);
            virtual Error InterruptIn(EndpointID ep_id, void* buf, int len);

            Error StartInitialize();
            bool IsInitialized() { return is_initialized_; }
            EndpointConfig* EndpointConfigs() { return ep_configs_.data(); }
            int NumEndpointConfigs() { return num_ep_configs_; }
            Error OnEndpointsConfigured();

        protected:
            Error OnControlCompleted(EndpointID ep_id, SetupData setup_data,
                    const void* buf, int len);
            Error OnInterruptCompleted(EndpointID ep_id, const void* buf, int len);

        private:
            /**
             * @brief エンドポイントに割り当て済みのクラスドライバ
             *
             * 添字はエンドポイント番号 0~15
             * 添字0はどのクラスドライバからも使われないため、常に未使用
             */
            std::array<ClassDriver*, 16> class_drivers_{};

            std::array<uint8_t, 256> buf_{};

            // デバイスがいくつの設定パターンを持っているか
            uint8_t num_configurations_;

            uint8_t config_index_;

            bool is_initialized_ = false;
            int initialize_phase_ = 0;
            std::array<EndpointConfig, 16> ep_configs_;
            int num_ep_configs_;
            Error InitializePhase1(const uint8_t* buf, int len);
            Error InitializePhase2(const uint8_t* buf, int len);
            Error InitializePhase3(uint8_t config_value);
            Error InitializePhase4();

            // OnControlCompletedの中で要求の発行元を特定するためのマップ構造
            // ControlOutまたはControlInを発行した時に発行元が登録される
            ArrayMap<SetupData, ClassDriver*, 4> event_waiters_{};
    };

    Error GetDescriptor(Device& dev, EndpointID ep_id,
            uint8_t desc_type, uint8_t desc_index,
            void* buf, int len, bool debug = false);
    Error SetConfiguration(Device& dev, EndpointID ep_id,
            uint8_t config_value, bool debug = false);
}
