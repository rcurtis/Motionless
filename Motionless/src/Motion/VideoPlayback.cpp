#pragma once

#include "include/DataSource.hpp"
#include "include/VideoPlayback.hpp"

namespace mt
{
    VideoPlayback::VideoPlayback(DataSource& DataSource) :
        m_datasource(&DataSource),
        m_protectionlock(),
        m_queuedvideopackets(),
		m_elapsed(),
		m_frametime(0),
        m_framejump(0),
        m_playedframecount(0)
    {
        SourceReloaded();
		std::lock_guard<std::mutex> lock(m_datasource->m_playbacklock);
        m_datasource->m_videoplaybacks.push_back(this);
    }

    VideoPlayback::~VideoPlayback()
    {
        if (m_datasource)
        {
			std::lock_guard<std::mutex> lock(m_datasource->m_playbacklock);
            for (auto& videoplayback : m_datasource->m_videoplaybacks)
            {
                if (videoplayback == this)
                {
                    std::swap(videoplayback, m_datasource->m_videoplaybacks.back());
                    m_datasource->m_videoplaybacks.pop_back();
                    break;
                }
            }
            m_datasource = nullptr;
        }
    }

    void VideoPlayback::Update(std::chrono::microseconds DeltaTime)
    {
        if (m_datasource && m_datasource->HasVideo())
        {
            if (m_datasource->GetState() == State::Playing)
            {
				m_elapsed += DeltaTime;
                int jumpcount = static_cast<int>(std::floor(m_elapsed / m_frametime));
				
				m_elapsed -= m_frametime * jumpcount;
                m_framejump += jumpcount;
            }
			std::lock_guard<std::mutex> lock(m_protectionlock);
            while (m_queuedvideopackets.size() > 0)
            {
                if (m_framejump > 1)
                {
                    m_framejump -= 1;
                    m_playedframecount++;
                    m_queuedvideopackets.pop();
                }
                else if (m_framejump == 1)
                {
                    m_framejump -= 1;
                    m_playedframecount++;
					m_lastpacket = std::make_unique<mt::priv::VideoPacket>(*m_queuedvideopackets.front().get());
                    m_queuedvideopackets.pop();
                    break;
                }
                else
                {
                    break;
                }
            }
        }
    }

    void VideoPlayback::SourceReloaded()
    {
        if (m_datasource->HasVideo())
        {
			m_frametime = m_datasource->GetVideoFrameTime();
        }
    }

    void VideoPlayback::StateChanged(State PreviousState, State NewState)
    {
        if (NewState == State::Playing && PreviousState == State::Stopped)
        {
            m_framejump = 1;
            m_playedframecount = 0;
        }
        else if (NewState == State::Stopped)
        {
            m_framejump = 0;
            m_playedframecount = 0;
			std::lock_guard<std::mutex> lock(m_protectionlock);
            while (m_queuedvideopackets.size() > 0)
            {
                m_queuedvideopackets.pop();
            }
        }
    }

    unsigned int VideoPlayback::GetPlayedFrameCount() const
    {
        return m_playedframecount;
    }

	priv::VideoPacket* VideoPlayback::GetLastPacket() const
	{
		return m_lastpacket.get();
	}
}