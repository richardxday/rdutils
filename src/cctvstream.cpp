
#include <rdlib/QuitHandler.h>

#include "CameraStream.h"

AQuitHandler quithandler;

int main(int argc, char *argv[])
{
	ASocketServer server;
	CameraStream  stream(&server);

	if (argc < 3) {
		fprintf(stderr, "Usage: cctvstream <host> <port>\n");
		exit(1);
	}
	
	stream.Open(argv[1], (uint_t)AString(argv[2]));

	while (!quithandler.HasQuit() && !stream.StreamComplete()) {
		server.Process(2000);
	}
	
	return 0;
}
