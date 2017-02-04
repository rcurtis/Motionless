#pragma once

#include <string>
#include <cstdint>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <queue>
#include <vector>
#include <iostream>
#include <thread>
#include <mutex>

#include "include/priv/AudioPacket.hpp"
#include "include/AudioPlayback.hpp"
#include "include/priv/VideoPacket.hpp"
#include "include/VideoPlayback.hpp"
#include "include/State.hpp"
#include "include/NonCopyable.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

namespace mt
{
    class VideoPlayback;
    class AudioPlayback;

	class Vector2
	{
	public:
		int x, y;
		Vector2(int width, int height)
		{
			x = width;
			y = height;
		}
	};

    class DataSource : private mt::NonCopyable
    {
        friend class VideoPlayback;
        friend class AudioPlayback;

    public:
		

    private:
        int m_videostreamid;
        int m_audiostreamid;
		std::chrono::steady_clock::time_point m_start;
		std::chrono::microseconds m_playingoffset;
		std::chrono::microseconds m_filelength;
        Vector2 m_videosize;
        int m_audiochannelcount;
        float m_playbackspeed;
        AVFormatContext* m_formatcontext;
        AVCodecContext* m_videocontext;
        AVCodecContext* m_audiocontext;
        AVCodec* m_videocodec;
        AVCodec* m_audiocodec;
        AVFrame* m_videorawframe;
        AVFrame* m_videorgbaframe;
        AVFrame* m_audiorawbuffer;
        uint8_t* m_videorawbuffer;
        uint8_t* m_videorgbabuffer;
        uint8_t* m_audiopcmbuffer;
        SwsContext* m_videoswcontext;
        SwrContext* m_audioswcontext;
        State m_state;
        std::unique_ptr<std::thread> m_decodethread;
        std::atomic<bool> m_shouldthreadrun;
        std::atomic<bool> m_eofreached;
        std::atomic<bool> m_playingtoeof;
        std::mutex m_playbacklock;
        std::vector<mt::VideoPlayback*> m_videoplaybacks;
        std::vector<mt::AudioPlayback*> m_audioplaybacks;

        AVFrame* CreatePictureFrame(AVPixelFormat SelectedPixelFormat, int Width, int Height, unsigned char*& PictureBuffer);
        void DestroyPictureFrame(AVFrame*& PictureFrame, unsigned char*& PictureBuffer);
        void Cleanup();
        void StartDecodeThread();
        void StopDecodeThread();
        void DecodeThreadRun();
        bool IsFull();
        void NotifyStateChanged(State NewState);

    public:
		typedef enum
		{
			mtStateStopped,
			mtStatePlaying,
			mtStatePaused
		} mtState;

        DataSource();
        ~DataSource();
        bool LoadFromFile(const std::string& Filename, bool EnableVideo = true, bool EnableAudio = true);
        void Play();
        void Pause();
        void Stop();
        const bool HasVideo();
        const bool HasAudio();
        const Vector2 GetVideoSize();
        const State GetState();
        const std::chrono::microseconds GetVideoFrameTime();
        const int GetAudioChannelCount();
        const int GetAudioSampleRate();
        const std::chrono::microseconds GetFileLength();
        const std::chrono::microseconds GetPlayingOffset();
        void SetPlayingOffset(std::chrono::microseconds PlayingOffset);
        void Update();
        const float GetPlaybackSpeed();
        void SetPlaybackSpeed(float PlaybackSpeed);
        const bool IsEndofFileReached();
    };
}