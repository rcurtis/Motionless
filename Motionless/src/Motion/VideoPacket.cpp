#ifndef MOTION_VIDEOPACKET_CPP
#define MOTION_VIDEOPACKET_CPP


#include "include/priv/VideoPacket.hpp"


namespace mt
{
    namespace priv
    {
        VideoPacket::VideoPacket(uint8_t* RGBABufferSource, int Width, int Height) :
            m_rgbabuffer(new uint8_t[Width * Height * 4]), width(Width), height(Height)
        {
            std::memcpy(m_rgbabuffer, RGBABufferSource, Width * Height * 4);
        }

		VideoPacket::VideoPacket(const VideoPacket& other)
        {
			m_rgbabuffer = new uint8_t[other.width * other.height * 4];
			width = other.width;
			height = other.height;
			std::memcpy(this->m_rgbabuffer ,other.m_rgbabuffer, other.width * other.height * 4);
        }

        VideoPacket::~VideoPacket()
        {
            delete[] m_rgbabuffer;
        }

        const uint8_t* VideoPacket::GetRGBABuffer()
        {
            return m_rgbabuffer;
        }

    }
}

#endif
