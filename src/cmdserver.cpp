
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/QuitHandler.h>
#include <rdlib/SettingsHandler.h>
#include <rdlib/StdSocket.h>

#include "cmdserver_private.h"

#define DEBUG_PROCESS 0

AQuitHandler quithandler;

int main(int argc, char *argv[])
{
    AString filename = AString(getenv("HOME")).CatPath("cmdserver");
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) filename = AString(argv[i]).Prefix();
    }

    ASettingsHandler settings(filename, false);
    ASocketServer    server;
    AStdSocket       socket(server);

    if (socket.open(settings.Get("addr", "0.0.0.0"),
                    (uint_t)settings.Get("port", "1722"),
                    ASocketServer::Type_Datagram)) {
        while (!HasQuit()) {
            if (settings.CheckRead()) {
                printf("Configuration updated\n");
                settings.Read();
            }

#if DEBUG_PROCESS
            uint32_t tick = GetTickCount(), diff;
#endif

            int res = server.Process(1000);
            if (res < 0) break;

#if DEBUG_PROCESS
            if ((diff = GetTickCount() - tick) < 1000) {
                debug("Process took %ums\n", diff);
            }
#endif

            AString line;
            while ((socket.bytesavailable() > 0) && (line.ReadLn(socket) >= 0)) {
                AString cmd;

                if ((cmd = settings.Get(line)).Valid()) {
                    cmd = (cmd
                           .SearchAndReplace("{date}", ADateTime().DateFormat("%Y-%M-%D"))
                           .SearchAndReplace("{user}", getenv("LOGNAME"))
                           .SearchAndReplace("{home}", getenv("HOME")));
                    if (system(cmd + " &") != 0) {
                        fprintf(stderr, "Command '%s' failed\n", line.str());
                    }
                }
                else fprintf(stderr, "Unrecognized command '%s'\n", line.str());
            }

            fflush(stdout);
        }

        socket.close();
    }

    return 0;
}
