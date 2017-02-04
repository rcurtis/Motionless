#pragma once

#include <memory>
#include <queue>
#include <thread>
#include <chrono>
#include <cmath>
#include <vector>
#include <cstdint>

#include "include/DataSource.hpp"
#include "include/priv/AudioPacket.hpp"
#include "include/State.hpp"
#include "include/NonCopyable.h"

namespace mt
{
    class DataSource;

    class AudioPlayback : private mt::NonCopyable
    {
        friend class DataSource;

    private:
		std::chrono::microseconds m_audiorate;
        int m_channelcount;
		std::chrono::microseconds m_audioposition;
		std::chrono::microseconds m_offsetcorrection;
		std::chrono::steady_clock::time_point mStart;
        DataSource* m_datasource;
        std::mutex m_protectionlock;
        std::queue<priv::AudioPacketPtr> m_queuedaudiopackets;
        priv::AudioPacketPtr m_activepacket;

        void onSeek(std::chrono::microseconds timeOffset);
        bool WaitForData();
        void SourceReloaded();
        void StateChanged(State PreviousState, State NewState);
    public:
        AudioPlayback(DataSource& DataSource, std::chrono::microseconds OffsetCorrection = std::chrono::milliseconds(300));
        ~AudioPlayback();
        const float GetVolume();
        void SetVolume(float Volume);
        const std::chrono::microseconds GetOffsetCorrection();
        void SetOffsetCorrection(std::chrono::microseconds OffsetCorrection);
    };
}