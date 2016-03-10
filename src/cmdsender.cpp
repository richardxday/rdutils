
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/StdSocket.h>

int main(int argc, char *argv[])
{
	ASocketServer server;
	AStdSocket    socket(server);

	if (argc < 3) {
		fprintf(stderr, "Usage: cmdsender <host> <port>\n");
		exit(1);
	}
	
	if (socket.open("0.0.0.0",
					0,
					ASocketServer::Type_Datagram)) {
		socket.setdatagramdestination(argv[1], (uint_t)AString(argv[2]));
		
		AString line;
		while (line.ReadLn(Stdin) >= 0) {
			if (socket.printf("%s\n", line.str()) <= 0) break;
		}
		
		socket.close();
	}
	
	return 0;
}
