#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <cstring>

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
    namespace priv
    {
        class VideoPacket
        {
        private:
            uint8_t* m_rgbabuffer;
        public:
            VideoPacket(uint8_t* RGBABufferSource, int Width, int Height);
            ~VideoPacket();
			VideoPacket(const VideoPacket& other);
            const uint8_t* GetRGBABuffer();
			int width, height;
        };

        typedef std::shared_ptr<mt::priv::VideoPacket> VideoPacketPtr;
    }
}