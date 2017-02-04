Motionless

FFMPEG powered video/audio streaming C++ library.  This library is based on the excellent Motion library by zsb (https://github.com/zsbzsb/Motion).  This version has no ties to SFML (game library) or C exports and only relies on FFMPEG.  Audio is currently disabled (crudely commented out).  I simply have no need for it in my current application, but pull requests are welcome.

Basic usage:
Add the Motionless/Motionless dir to your include path.
Add the Motionless.lib output path to your linker path and link against Motionless.lib
Add Motionless.lib to your working directory.

Main.cpp example:

#include "include/Motion.hpp"

....

int main()
{
	// Open a window using your prefered graphics library

	mt::DataSource data;
	data.LoadFromFile(PATH_TO_VIDEO_FILE);
	mt::VideoPlayback player(data);
	player.play();

	while(UPDATE_LOOP_RUNNING)
	{
		player.Update();

		auto lastPacket = player.GetLastPacket();
		if(!lastPacket)
			return;

		auto size = lastPacket->width * lastPacket->height * 4 // Motion will spit out a 4 channel RGBA buffer.

		// This is a fictional function that will update a texture or destroy the old one and create a 
		// new one ever frame.  OpenGL and DirectX will have varying performance characteristics for
		// this operation which are outside the domain of this library.
		UpdateTexture(tex, lastPacket->width, lastPacket->height, 4, lastPacket->GetRGBABuffer());

		Draw(tex);
	}

}