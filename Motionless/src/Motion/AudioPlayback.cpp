#pragma once

#include "../../include/AudioPlayback.hpp"
#include "../../include/DataSource.hpp"

namespace mt
{

	/// Most of this file is unuseable at the moment.  I have no need of audio for my application.
	/// If you require it, you will need to test this code out.

    AudioPlayback::AudioPlayback(DataSource& DataSource, std::chrono::microseconds AudioOffsetCorrection) :
        //m_audioplayrate(sf::Time::Zero),
		m_audiorate(0),
        m_channelcount(0),
        //m_audioposition(),
		m_audioposition(0),
        //m_offsetcorrection(AudioOffsetCorrection),
		m_offsetcorrection(AudioOffsetCorrection),
        //m_updateclock(),
        m_datasource(&DataSource),
        m_protectionlock(),
        m_queuedaudiopackets(),
        m_activepacket(nullptr)
    {
        SourceReloaded();
		std::lock_guard<std::mutex> lock(m_datasource->m_playbacklock);
        m_datasource->m_audioplaybacks.push_back(this);
    }

    AudioPlayback::~AudioPlayback()
    {
        if (m_datasource)
        {
			std::lock_guard<std::mutex> lock(m_datasource->m_playbacklock);
            for (auto& audioplayback : m_datasource->m_audioplaybacks)
            {
                if (audioplayback == this)
                {
                    std::swap(audioplayback, m_datasource->m_audioplaybacks.back());
                    m_datasource->m_audioplaybacks.pop_back();
                    break;
                }
            }
            m_datasource = nullptr;
        }
        //stop();
    }

    /*bool AudioPlayback::onGetData(Chunk& data)
    {
        bool hasdata = WaitForData();
		std::lock_guard<std::mutex> lock(m_protectionlock);
        m_activepacket = nullptr;
        if (m_datasource && hasdata)
        {
            if (m_offsetcorrection != sf::Time::Zero && m_audioposition >= m_offsetcorrection)
            {
                // add samples
                std::size_t samplecount = static_cast<std::size_t>(std::floor(m_audioposition.asSeconds() * m_audioplayrate.asSeconds() * m_channelcount));
                std::vector<int16_t> newsamples;
                newsamples.resize(samplecount);
                m_activepacket = std::make_shared<priv::AudioPacket>(&newsamples[0], samplecount / m_channelcount, m_channelcount);
                m_audioposition -= sf::seconds(m_activepacket->GetSamplesBufferLength() / m_audioplayrate.asSeconds() / m_channelcount);

            }
            else if (m_offsetcorrection != sf::Time::Zero && m_audioposition < -m_offsetcorrection)
            {
                // skip samples
                bool shouldSkip = true;
                while (shouldSkip && hasdata)
                {
                    m_audioposition += sf::seconds(m_queuedaudiopackets.front()->GetSamplesBufferLength() / m_audioplayrate.asSeconds() / m_channelcount);
                    m_queuedaudiopackets.pop();
                    m_protectionlock.unlock(); // unlock to wait for data
                    hasdata = WaitForData();
                    m_protectionlock.lock(); // relock after waiting for data
                    shouldSkip = m_audioposition <= -m_offsetcorrection;
                }
                if (hasdata)
                {
                    m_activepacket = m_queuedaudiopackets.front();
                    m_queuedaudiopackets.pop();
                    m_audioposition += sf::seconds(m_activepacket->GetSamplesBufferLength() / m_audioplayrate.asSeconds() / m_channelcount);
                }
            }
            else
            {
                // continue as normal
                if (m_queuedaudiopackets.size() > 0)
                {
                    m_activepacket = m_queuedaudiopackets.front();
                    m_queuedaudiopackets.pop();
                    m_audioposition += sf::seconds(m_activepacket->GetSamplesBufferLength() / m_audioplayrate.asSeconds() / m_channelcount);
                }
            }
        }
        if (m_activepacket)
        {
            data.samples = m_activepacket->GetSamplesBuffer();
            data.sampleCount = m_activepacket->GetSamplesBufferLength();
        }
        return static_cast<bool>(m_activepacket);
    }*/

    void AudioPlayback::onSeek(std::chrono::microseconds timeOffset)
    {
        // nothing to do
    }

    bool AudioPlayback::WaitForData()
    {
        bool hasdata = false;
        while (!hasdata && m_datasource && !m_datasource->m_eofreached)
        {
            {
				std::lock_guard<std::mutex> lock(m_protectionlock);
                hasdata = m_queuedaudiopackets.size() > 0;
                if (hasdata)
                {
					auto now = std::chrono::steady_clock::now();
					auto delta = now - mStart;
					mStart = now;
					auto duration = std::chrono::duration_cast<std::chrono::microseconds>(delta);
					m_audioposition -= duration;
					//m_audioposition -= m_updateclock.restart() * getPitch();
                }
            }
            if (!hasdata) std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }

        if (!hasdata)
        {
			std::lock_guard<std::mutex> lock(m_protectionlock);
			auto now = std::chrono::steady_clock::now();
			auto delta = now - mStart;
			mStart = now;
			auto duration = std::chrono::duration_cast<std::chrono::microseconds>(delta);
			m_audioposition -= duration;
            //m_audioposition -= m_updateclock.restart() * getPitch();
        }

        return hasdata;
    }

    void AudioPlayback::SourceReloaded()
    {
        if (m_datasource->HasAudio())
        {
			m_audiorate = std::chrono::microseconds(m_datasource->GetAudioSampleRate() / 1000000);
            //m_audioplayrate = sf::seconds(static_cast<float>(m_datasource->GetAudioSampleRate()));
            m_channelcount = m_datasource->GetAudioChannelCount();
            //initialize(m_channelcount, m_datasource->GetAudioSampleRate());
            //setPitch(m_datasource->GetPlaybackSpeed());
            StateChanged(m_datasource->GetState(), m_datasource->GetState());
        }
    }

    void AudioPlayback::StateChanged(State PreviousState, State NewState)
    {
        if (NewState == State::Playing && m_datasource->HasAudio())
        {
            //play();
			std::lock_guard<std::mutex> lock(m_protectionlock);
			mStart = std::chrono::steady_clock::now();
            //m_updateclock.restart();
        }
        else if (NewState == State::Paused && m_datasource->HasAudio())
        {
            //pause();
        }
        else if (NewState == State::Stopped)
        {
            //stop();
			std::lock_guard<std::mutex> lock(m_protectionlock);
			m_audioposition = std::chrono::microseconds(0);
            //m_audioposition = sf::Time::Zero;
            while (m_queuedaudiopackets.size() > 0)
            {
                m_queuedaudiopackets.pop();
            }
        }
    }

    const float AudioPlayback::GetVolume()
    {
        //return getVolume();
		return 0.f;
    }

    void AudioPlayback::SetVolume(float Volume)
    {
        //setVolume(Volume);
    }

    const std::chrono::microseconds AudioPlayback::GetOffsetCorrection()
    {
		std::lock_guard<std::mutex> lock(m_protectionlock);
		return m_offsetcorrection;
        //return m_offsetcorrection;
    }

    void AudioPlayback::SetOffsetCorrection(std::chrono::microseconds OffsetCorrection)
    {
		std::lock_guard<std::mutex> lock(m_protectionlock);
		m_offsetcorrection = OffsetCorrection;
        //m_offsetcorrection = OffsetCorrection;
    }
}