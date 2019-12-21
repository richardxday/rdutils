
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/StdSocket.h>

#include "cmdsender_private.h"

int main(int argc, char *argv[])
{
    ASocketServer server;
    AStdSocket    socket(server);

    if (argc < 2) {
        fprintf(stderr, "cmdsender " VER_STRING "\n");
        fprintf(stderr, "Usage: cmdsender <host> [<port>]\n");
        exit(1);
    }

    if (socket.open("0.0.0.0",
                    0,
                    ASocketServer::Type_Datagram)) {
        uint_t port = 1722;

        if (argc >= 3) port = (uint_t)AString(argv[2]);

        if (socket.setdatagramdestination(argv[1], port)) {
            AString line;
            while (line.ReadLn(Stdin) >= 0) {
                if (socket.printf("%s\n", line.str()) <= 0) break;
            }
        }

        socket.close();
    }

    return 0;
}
