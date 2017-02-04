#pragma once

#include "../../include/DataSource.hpp"
#include <thread>

#define MAX_AUDIO_SAMPLES 192000
#define PACKET_QUEUE_AMOUNT 5

namespace mt
{
    DataSource::DataSource() :
        m_videostreamid(-1),
        m_audiostreamid(-1),
		m_playingoffset(),
        m_videosize({-1, -1}),
        m_audiochannelcount(-1),
        m_playbackspeed(1),
        m_formatcontext(nullptr),
        m_videocontext(nullptr),
        m_audiocontext(nullptr),
        m_videocodec(nullptr),
        m_audiocodec(nullptr),
        m_videorawframe(nullptr),
        m_videorgbaframe(nullptr),
        m_audiorawbuffer(nullptr),
        m_videorawbuffer(nullptr),
        m_videorgbabuffer(nullptr),
        m_audiopcmbuffer(nullptr),
        m_videoswcontext(nullptr),
        m_audioswcontext(nullptr),
        m_state(State::Stopped),
        m_decodethread(nullptr),
        m_shouldthreadrun(false),
        m_eofreached(false),
        m_playingtoeof(false),
        m_playbacklock(),
        m_videoplaybacks(),
        m_audioplaybacks()
    {
        av_register_all();
    }

    DataSource::~DataSource()
    {
        Cleanup();
        {
			std::lock_guard<std::mutex> lock(m_playbacklock);
            while (m_videoplaybacks.size() > 0)
            {
                m_videoplaybacks.back()->m_datasource = nullptr;
                m_videoplaybacks.pop_back();
            }
            while (m_audioplaybacks.size() > 0)
            {
                m_audioplaybacks.back()->m_datasource = nullptr;
                m_audioplaybacks.pop_back();
            }
        }
    }

    void DataSource::Cleanup()
    {
        Stop();
        StopDecodeThread();
        m_videostreamid = -1;
        m_audiostreamid = -1;
		m_playingoffset = std::chrono::microseconds(0);
		m_videosize = Vector2{ -1, -1 };
        m_audiochannelcount = -1;
        if (m_videocontext)
        {
            avcodec_close(m_videocontext);
            m_videocontext = nullptr;
        }
        m_videocodec = nullptr;
        if (m_audiocontext)
        {
            avcodec_close(m_audiocontext);
            m_audiocontext = nullptr;
        }
        m_audiocodec = nullptr;
        if (m_videorawframe) DestroyPictureFrame(m_videorawframe, m_videorawbuffer);
        if (m_videorgbaframe) DestroyPictureFrame(m_videorgbaframe, m_videorgbabuffer);
        if (m_audiorawbuffer)
        {
            av_frame_free(&m_audiorawbuffer);
            m_audiorawbuffer = nullptr;
        }
        if (m_audiopcmbuffer)
        {
            av_free(m_audiopcmbuffer);
            m_audiopcmbuffer = nullptr;
        }
        if (m_videoswcontext)
        {
            sws_freeContext(m_videoswcontext);
            m_videoswcontext = nullptr;
        }
        if (m_audioswcontext)
        {
            swr_free(&m_audioswcontext);
            m_audioswcontext = nullptr;
        }
        if (m_formatcontext)
        {
            avformat_close_input(&m_formatcontext);
            m_formatcontext = nullptr;
        }
    }

    bool DataSource::LoadFromFile(const std::string& Filename, bool EnableVideo, bool EnableAudio)
    {
        Cleanup();
        if (avformat_open_input(&m_formatcontext, Filename.c_str(), nullptr, nullptr) != 0)
        {
            std::cout << "Motion: Failed to open file: '" << Filename << "'" << std::endl;
            return false;
        }
        if (avformat_find_stream_info(m_formatcontext, nullptr) < 0)
        {
            std::cout << "Motion: Failed to find stream information" << std::endl;
            return false;
        }
        for (unsigned int i = 0; i < m_formatcontext->nb_streams; i++)
        {
            switch (m_formatcontext->streams[i]->codec->codec_type)
            {
                case AVMEDIA_TYPE_VIDEO:
                    if (m_videostreamid == -1 && EnableVideo) m_videostreamid = i;
                    break;
                case AVMEDIA_TYPE_AUDIO:
                    if (m_audiostreamid == -1 && EnableAudio) m_audiostreamid = i;
                    break;
                default:
                    break;
            }
        }
        if (HasVideo())
        {
            m_videocontext = m_formatcontext->streams[m_videostreamid]->codec;
            if (!m_videocontext)
            {
                std::cout << "Motion: Failed to get video codec context" << std::endl;
                m_videostreamid = -1;
            }
            else
            {
                m_videocodec = avcodec_find_decoder(m_videocontext->codec_id);
                if (!m_videocodec)
                {
                    std::cout << "Motion: Failed to find video codec" << std::endl;
                    m_videostreamid = -1;
                }
                else
                {
                    if (avcodec_open2(m_videocontext, m_videocodec, nullptr) != 0)
                    {
                        std::cout << "Motion: Failed to load video codec" << std::endl;
                        m_videostreamid = -1;
                    }
                    else
                    {
                        m_videosize = Vector2(m_videocontext->width, m_videocontext->height);
                        m_videorawframe = CreatePictureFrame(m_videocontext->pix_fmt, m_videosize.x, m_videosize.y, m_videorawbuffer);
                        m_videorgbaframe = CreatePictureFrame(AVPixelFormat::AV_PIX_FMT_BGRA, m_videosize.x, m_videosize.y, m_videorgbabuffer);
                        if (!m_videorawframe || !m_videorgbaframe)
                        {
                            std::cout << "Motion: Failed to create video frames" << std::endl;
                            m_videostreamid = -1;
                        }
                        else
                        {
                            int swapmode = SWS_FAST_BILINEAR;
                            if (m_videosize.x * m_videosize.y <= 500000 && m_videosize.x % 8 != 0) swapmode |= SWS_ACCURATE_RND;
                            m_videoswcontext = sws_getCachedContext(nullptr, m_videosize.x, m_videosize.y, m_videocontext->pix_fmt, m_videosize.x, m_videosize.y, AVPixelFormat::AV_PIX_FMT_RGBA, swapmode, nullptr, nullptr, nullptr);
                        }
                    }
                }
            }
        }
        if (HasAudio())
        {
            m_audiocontext = m_formatcontext->streams[m_audiostreamid]->codec;
            if (!m_audiocontext)
            {
                std::cout << "Motion: Failed to get audio codec context" << std::endl;
                m_audiostreamid = -1;
            }
            else
            {
                m_audiocodec = avcodec_find_decoder(m_audiocontext->codec_id);
                if (!m_audiocodec)
                {
                    std::cout << "Motion: Failed to find audio codec" << std::endl;
                    m_audiostreamid = -1;
                }
                else
                {
                    if (avcodec_open2(m_audiocontext, m_audiocodec, nullptr) != 0)
                    {
                        std::cout << "Motion: Failed to load video codec" << std::endl;
                        m_audiostreamid = -1;
                    }
                    else
                    {
                        m_audiorawbuffer = av_frame_alloc();
                        if (!m_audiorawbuffer)
                        {
                            std::cout << "Motion: Failed to allocate audio buffer" << std::endl;
                            m_audiostreamid = -1;
                        }
                        else
                        {
                            if (av_samples_alloc(&m_audiopcmbuffer, nullptr, m_audiocontext->channels, av_samples_get_buffer_size(nullptr, m_audiocontext->channels, MAX_AUDIO_SAMPLES, AV_SAMPLE_FMT_S16, 0), AV_SAMPLE_FMT_S16, 0) < 0)
                            {
                                std::cout << "Motion: Failed to create audio samples buffer" << std::endl;
                                m_audiostreamid = -1;
                            }
                            else
                            {
                                av_frame_unref(m_audiorawbuffer);
                                m_audioswcontext = swr_alloc();
                                uint64_t inchanlayout = m_audiocontext->channel_layout;
                                if (inchanlayout == 0) inchanlayout = av_get_default_channel_layout(m_audiocontext->channels);
                                uint64_t outchanlayout = inchanlayout;
                                if (outchanlayout != AV_CH_LAYOUT_MONO) outchanlayout = AV_CH_LAYOUT_STEREO;
                                av_opt_set_int(m_audioswcontext, "in_channel_layout", inchanlayout, 0);
                                av_opt_set_int(m_audioswcontext, "out_channel_layout", outchanlayout, 0);
                                av_opt_set_int(m_audioswcontext, "in_sample_rate", m_audiocontext->sample_rate, 0);
                                av_opt_set_int(m_audioswcontext, "out_sample_rate", m_audiocontext->sample_rate, 0);
                                av_opt_set_sample_fmt(m_audioswcontext, "in_sample_fmt", m_audiocontext->sample_fmt, 0);
                                av_opt_set_sample_fmt(m_audioswcontext, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
                                swr_init(m_audioswcontext);
                                m_audiochannelcount = av_get_channel_layout_nb_channels(outchanlayout);
                            }
                        }
                    }
                }
            }
        }
        if (m_formatcontext->duration != AV_NOPTS_VALUE)
        {
			m_filelength = std::chrono::microseconds(static_cast<int>(m_formatcontext->duration));
	        //m_filelength = sf::milliseconds(static_cast<int>(m_formatcontext->duration) / 1000);
        }
        if (HasVideo() || HasAudio())
        {
            StartDecodeThread();
			std::lock_guard<std::mutex> lock(m_playbacklock);
            for (auto& videoplayback : m_videoplaybacks)
            {
                videoplayback->SourceReloaded();
            }
            for (auto& audioplayback : m_audioplaybacks)
            {
                audioplayback->SourceReloaded();
            }
            return true;
        }
        else
        {
            std::cout << "Motion: Failed to load audio or video" << std::endl;
            Cleanup();
            return false;
        }
    }

    const bool DataSource::HasVideo()
    {
        return m_videostreamid != -1;
    }

    const bool DataSource::HasAudio()
    {
        return m_audiostreamid != -1;
    }

    const Vector2 DataSource::GetVideoSize()
    {
        return m_videosize;
    }

    const State DataSource::GetState()
    {
        return m_state;
    }

    const std::chrono::microseconds DataSource::GetVideoFrameTime()
    {
        if (!HasVideo()) return std::chrono::microseconds(0);
        AVRational r1 = m_formatcontext->streams[m_videostreamid]->avg_frame_rate;
        AVRational r2 = m_formatcontext->streams[m_videostreamid]->r_frame_rate;

		std::chrono::microseconds time(0);

        if ((!r1.num || !r1.den) && (!r2.num || !r2.den))
        {
			time = std::chrono::seconds(static_cast<long long>(1.f / 29.97f * 1000000));
        }
        else
        {
            if (r2.num && r1.den)
            {
				time = std::chrono::microseconds(static_cast<long long>(
					1.0f / ((float)r1.num / (float)r1.den) * 1000000) );
            }
            else
            {
				time = std::chrono::microseconds(static_cast<long long>(
					1.0f / ((float)r2.num / (float)r2.den) * 1000000));
            }
        }
		return time;
    }

    const int DataSource::GetAudioChannelCount()
    {
        return m_audiochannelcount;
    }

    const int DataSource::GetAudioSampleRate()
    {
        if (!HasAudio()) return -1;
        return m_audiocontext->sample_rate;
    }

    void DataSource::Play()
    {
        if ((HasVideo() || HasAudio()) && m_state != State::Playing)
        {
            m_eofreached = false;
			m_start = std::chrono::steady_clock::now();
            NotifyStateChanged(State::Playing);
            m_state = State::Playing;
        }
    }

    void DataSource::Pause()
    {
        if (m_state == State::Playing)
        {
            NotifyStateChanged(State::Paused);
            m_state = State::Paused;
        }
    }

    void DataSource::Stop()
    {
        if (m_state != State::Stopped)
        {
            m_eofreached = true;
            NotifyStateChanged(State::Stopped);
            m_state = State::Stopped;
            m_eofreached = false;
            SetPlayingOffset(std::chrono::microseconds(0));
        }
    }

    const std::chrono::microseconds DataSource::GetFileLength()
    {
		return m_filelength;
    }

    const std::chrono::microseconds DataSource::GetPlayingOffset()
    {

        return m_playingoffset;
    }

    void DataSource::SetPlayingOffset(std::chrono::microseconds PlayingOffset)
    {
        if (HasVideo() || HasAudio())
        {
            StopDecodeThread();
			m_playingoffset = PlayingOffset;
            m_playingtoeof = false;
            bool startplaying = m_state == State::Playing;
            if (m_state != State::Stopped)
            {
                m_eofreached = true;
                NotifyStateChanged(State::Stopped);
                m_state = State::Stopped;
                m_eofreached = false;
            }
			auto seconds = std::chrono::duration_cast<std::chrono::seconds>(PlayingOffset);
            if (HasVideo())
            {
                AVRational timebase = m_formatcontext->streams[m_videostreamid]->time_base;
                float ftb = (float)timebase.den / (float)timebase.num;
                int64_t pos = static_cast<int64_t>(seconds.count() * ftb);
                av_seek_frame(m_formatcontext, m_videostreamid, pos, AVSEEK_FLAG_ANY);
                avcodec_flush_buffers(m_videocontext);
            }
            if (HasAudio())
            {
                AVRational timebase = m_formatcontext->streams[m_audiostreamid]->time_base;
                float ftb = (float)timebase.den / (float)timebase.num;
				int64_t pos = static_cast<int64_t>(seconds.count() * ftb);
                av_seek_frame(m_formatcontext, m_audiostreamid, pos, AVSEEK_FLAG_ANY);
                avcodec_flush_buffers(m_audiocontext);
            }
            StartDecodeThread();
            if (startplaying) Play();
        }
    }

    void DataSource::NotifyStateChanged(State NewState)
    {
		std::lock_guard<std::mutex> lock(m_playbacklock);
        for (auto& videoplayback : m_videoplaybacks)
        {
            videoplayback->StateChanged(m_state, NewState);
        }
        for (auto& audioplayback : m_audioplaybacks)
        {
            audioplayback->StateChanged(m_state, NewState);
        }
    }

    void DataSource::Update()
    {
        if (m_playingoffset > m_filelength)
        {
            Stop();
            m_eofreached = true;
        }

		auto now = std::chrono::steady_clock::now();
		auto deltatime = now - m_start;
		m_start = now;
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(deltatime);

        if (m_state == State::Playing)
        {			
	        m_playingoffset += std::chrono::microseconds(duration);
        }

		std::lock_guard<std::mutex> lock(m_playbacklock);
        for (auto& videoplayback : m_videoplaybacks)
        {
            videoplayback->Update(duration);
        }
    }

    const float DataSource::GetPlaybackSpeed()
    {
        return m_playbackspeed;
    }

    void DataSource::SetPlaybackSpeed(float PlaybackSpeed)
    {
        m_playbackspeed = PlaybackSpeed;
		std::lock_guard<std::mutex> lock(m_playbacklock);
        for (auto& audioplayback : m_audioplaybacks)
        {
            //audioplayback->setPitch(PlaybackSpeed);
        }
    }

    void DataSource::StartDecodeThread()
    {
        if (m_shouldthreadrun) return;
        m_shouldthreadrun = true;
        m_decodethread.reset(new std::thread(&DataSource::DecodeThreadRun, this));
    }

    void DataSource::StopDecodeThread()
    {
        if (!m_shouldthreadrun) return;
        m_shouldthreadrun = false;
        if (m_decodethread->joinable()) m_decodethread->join();
        m_decodethread.reset(nullptr);
    }

    void DataSource::DecodeThreadRun()
    {
        while (m_shouldthreadrun)
        {
            bool isfull = IsFull();
            while (!isfull && m_shouldthreadrun && !m_playingtoeof)
            {
                bool validpacket = false;
                while (!validpacket && m_shouldthreadrun)
                {
                    AVPacket* packet;
                    packet = (AVPacket*)av_malloc(sizeof(*packet));
                    av_init_packet(packet);
                    if (av_read_frame(m_formatcontext, packet) == 0)
                    {
                        if (packet->stream_index == m_videostreamid)
                        {
                            int decoderesult = 0;
                            if (avcodec_decode_video2(m_videocontext, m_videorawframe, &decoderesult, packet) >= 0)
                            {
                                if (decoderesult)
                                {
                                    if (sws_scale(m_videoswcontext, m_videorawframe->data, m_videorawframe->linesize, 0, m_videocontext->height, m_videorgbaframe->data, m_videorgbaframe->linesize))
                                    {
                                        validpacket = true;
                                        priv::VideoPacketPtr packet(std::make_shared<priv::VideoPacket>(m_videorgbaframe->data[0], m_videosize.x, m_videosize.y));
                                        {
											std::lock_guard<std::mutex> lock(m_playbacklock);
                                            for (auto& videoplayback : m_videoplaybacks)
                                            {
												std::lock_guard<std::mutex> lock(videoplayback->m_protectionlock);
                                                videoplayback->m_queuedvideopackets.push(packet);
                                            }
                                        }
                                        isfull = IsFull();
                                    }
                                }
                            }
                        }
                        else if (packet->stream_index == m_audiostreamid)
                        {
                            int decoderesult = 0;
                            if (avcodec_decode_audio4(m_audiocontext, m_audiorawbuffer, &decoderesult, packet) > 0)
                            {
                                if (decoderesult)
                                {
                                    int convertlength = swr_convert(m_audioswcontext, &m_audiopcmbuffer, m_audiorawbuffer->nb_samples, (const uint8_t**)m_audiorawbuffer->extended_data, m_audiorawbuffer->nb_samples);
                                    if (convertlength > 0)
                                    {
                                        validpacket = true;
                                        priv::AudioPacketPtr packet(std::make_shared<priv::AudioPacket>(m_audiopcmbuffer, convertlength, m_audiochannelcount));
                                        {
											std::lock_guard<std::mutex> lock(m_playbacklock);
                                            for (auto& audioplayback : m_audioplaybacks)
                                            {
												std::lock_guard<std::mutex> lock(audioplayback->m_protectionlock);
                                                audioplayback->m_queuedaudiopackets.push(packet);
                                            }
                                        }
                                        isfull = IsFull();
                                    }
                                }
                            }
                        }
                        av_free_packet(packet);
                        av_free(packet);
                    }
                    else
                    {
                        m_playingtoeof = true;
                        validpacket = true;
                        av_free_packet(packet);
                        av_free(packet);
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    bool DataSource::IsFull()
    {
		std::lock_guard<std::mutex> lock(m_playbacklock);
        if (HasVideo())
        {
            for (auto& videoplayback : m_videoplaybacks)
            {
				std::lock_guard<std::mutex> lock(videoplayback->m_protectionlock);
                if (videoplayback->m_queuedvideopackets.size() < PACKET_QUEUE_AMOUNT)
                {
                    return false;
                }
            }
        }
        else if (HasAudio())
        {
            for (auto& audioplayback : m_audioplaybacks)
            {
				std::lock_guard<std::mutex> lock(audioplayback->m_protectionlock);
                if (audioplayback->m_queuedaudiopackets.size() < PACKET_QUEUE_AMOUNT)
                {
                    return false;
                }
            }
        }
        return true;
    }

    AVFrame* DataSource::CreatePictureFrame(AVPixelFormat SelectedPixelFormat, int Width, int Height, uint8_t*& PictureBuffer)
    {
        AVFrame *picture;
        picture = av_frame_alloc();
        if (!picture) return nullptr;
        int size = avpicture_get_size(SelectedPixelFormat, Width, Height);
        PictureBuffer = (uint8_t*)av_malloc(size);
        if (!PictureBuffer)
        {
            av_frame_free(&picture);
            return nullptr;
        }
        avpicture_fill((AVPicture*)picture, PictureBuffer, SelectedPixelFormat, Width, Height);
        return picture;
    }

    void DataSource::DestroyPictureFrame(AVFrame*& PictureFrame, uint8_t*& PictureBuffer)
    {
        av_free(PictureBuffer);
        av_frame_free(&PictureFrame);
        PictureBuffer = nullptr;
        PictureFrame = nullptr;
    }

    const bool DataSource::IsEndofFileReached()
    {
        return m_eofreached;
    }
}