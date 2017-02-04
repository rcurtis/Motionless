#pragma once

#include <memory>
#include <queue>
#include <cmath>

#include "include/DataSource.hpp"
#include "include/State.hpp"
#include "include/priv/VideoPacket.hpp"
#include "include/NonCopyable.h"

#include <chrono>

namespace mt
{
    class DataSource;

    class VideoPlayback : private mt::NonCopyable
    {
        friend class DataSource;

    private:
        DataSource* m_datasource;
        std::mutex m_protectionlock;
        std::queue<priv::VideoPacketPtr> m_queuedvideopackets;
		std::chrono::microseconds m_elapsed;
		std::chrono::microseconds m_frametime;
        int m_framejump;
		
        unsigned int m_playedframecount;

        void SourceReloaded();
        void StateChanged(State PreviousState, State NewState);
        void Update(std::chrono::microseconds DeltaTime);
		std::unique_ptr<mt::priv::VideoPacket> m_lastpacket;

    public:
        VideoPlayback(DataSource& DataSource);
        ~VideoPlayback();
        unsigned int GetPlayedFrameCount() const;
	    priv::VideoPacket* GetLastPacket() const;
    };
}
