
#include <stdio.h>

#include <rdlib/StdUri.h>
#include <rdlib/StdSocket.h>
#include <rdlib/SocketServer.h>
#include <rdlib/QuitHandler.h>

#include "uri2uri_private.h"

AQuitHandler QuitHandler;

int main(int argc, char *argv[])
{
	AStdUri uri1, uri2, datauri;
	ASocketServer server;
	uint8_t   *uri1to2buf = NULL;
	uint8_t   *uri2to1buf = NULL;
	uint32_t updatetime = 500;
	uint_t   bufsize = 65536;
	uint_t   uri1bs = 1;
	uint_t   uri2bs = 1;
	bool   reconnect = false;

	int arg;
	for (arg = 1; arg < argc; arg++) {
		if		(stricmp(argv[arg], "-q")   == 0) updatetime = ~0;
		else if (stricmp(argv[arg], "-u")   == 0) updatetime = (uint32_t)AString(argv[++arg]);
		else if (stricmp(argv[arg], "-d")   == 0) {
			datauri.close();
			if (!datauri.open(argv[++arg], true)) {
				fprintf(stderr, "Warning: failed to open debug URI %s for writing\n", argv[arg]);
			}
		}
		else if (stricmp(argv[arg], "-b")   == 0) bufsize    = (uint_t)AString(argv[++arg]) * 1024;
		else if (stricmp(argv[arg], "-bs")  == 0) uri1bs 	 = uri2bs = (uint_t)AString(argv[++arg]);
		else if (stricmp(argv[arg], "-bs1") == 0) uri1bs 	 = (uint_t)AString(argv[++arg]);
		else if (stricmp(argv[arg], "-bs2") == 0) uri2bs 	 = (uint_t)AString(argv[++arg]);
		else if (stricmp(argv[arg], "-r")   == 0) reconnect	 = true;
		else break;
	}

	if ((arg + 2) > argc) {
		fprintf(stderr, "uri2uri " VER_STRING "\n");
		fprintf(stderr, "Usage: uri2uri [<options>] <uri1> <uri2>\n");
		fprintf(stderr, "Where <options> are:\n");
		fprintf(stderr, "\t-q\t\t\tDo not output traffic stats\n");
		fprintf(stderr, "\t-u <time-ms>\t\tUpdate time in ms (default %ums)\n", updatetime);
		fprintf(stderr, "\t-d <uri>\t\tOutput data as hex to this URI\n");
		fprintf(stderr, "\t-b <buffer-size-k>\tBuffer size in kb (default %ukb)\n", bufsize / 1024);
		fprintf(stderr, "\t-bs <block-size>\tRead/write block size in bytes for BOTH ur1 and uri2 (default %u)\n", uri1bs);
		fprintf(stderr, "\t-bs1 <block-size>\tRead/write block size in bytes for uri1 (default %u)\n", uri1bs);
		fprintf(stderr, "\t-bs2 <block-size>\tRead/write block size in bytes for uri2 (default %u)\n", uri2bs);
		fprintf(stderr, "\t-r\t\t\tAutomatically reconnect if connection lost\n");
		exit(1);
	}

	AStdUri::setglobaldefault("socketserver", (uptr_t)&server);
	
	if (((uri1to2buf = new uint8_t[bufsize]) != NULL) &&
		((uri2to1buf = new uint8_t[bufsize]) != NULL)) {
		do {
			if (uri1.open(argv[arg])) {
				if (uri2.open(argv[arg + 1], true)) {
					sint32_t n;
					uint_t   uri1to2pos = 0, uri2to1pos = 0;
					uint32_t uri1to2rd  = 0, uri1to2wr  = 0;
					uint32_t uri2to1rd  = 0, uri2to1wr  = 0;
					uint32_t lastupdate = ::GetTickCount();
					bool   uri1clients = false;
					bool   uri2clients = false;
					bool   updated = false;

					fprintf(stderr, "\nConnected!\n");

					while (!QuitHandler.HasQuit()) {
						AStdSocket *socket;
						bool wait = true;

						server.Process(0);

						if (((socket = AStdSocket::Cast(uri1.gethandler())) != NULL) && socket->isserverconnection()) {
							uri1clients |= socket->clientconnected();

							if (uri1clients && !socket->clientconnected() && reconnect) {
								fprintf(stderr, "\nStream 1 disconnected\n");
								break;
							}
						}

						if (((socket = AStdSocket::Cast(uri2.gethandler())) != NULL) && socket->isserverconnection()) {
							uri2clients |= socket->clientconnected();

							if (uri2clients && !socket->clientconnected() && reconnect) {
								fprintf(stderr, "\nStream 2 disconnected\n");
								break;
							}
						}

						if ((n = uri1.bytesavailable()) > 0) {
							n  = MIN(n, (sint32_t)(bufsize - uri1to2pos));
							n -= n % uri1bs;

							if ((n = uri1.readbytes(uri1to2buf + uri1to2pos, n)) > 0) {
								if (datauri.isopen()) {
									AString str;
									int i;

									str.printf("1->2: %08x:", uri1to2rd);
									for (i = 0; i < n; i++) {
										str.printf(" %02x", (uint_t)uri1to2buf[uri1to2pos + i]);
									}
									datauri.printf("%s\n", str.str());
									datauri.flush();
								}

								uri1to2pos += n;
								uri1to2rd  += n;
								updated = true;
								wait    = false;
							}
						}

						if (n < 0) {
							fprintf(stderr, "\nStream 1 closed\n");
							break;
						}

						if ((n = uri2.bytesavailable()) > 0) {
							n  = MIN(n, (sint32_t)(bufsize - uri2to1pos));
							n -= n % uri2bs;
				
							if ((n = uri2.readbytes(uri2to1buf + uri2to1pos, n)) > 0) {
								if (datauri.isopen()) {
									AString str;
									sint_t i;

									str.printf("2->1: %08x:", uri2to1rd);
									for (i = 0; i < n; i++) {
										str.printf(" %02x", (uint_t)uri2to1buf[uri2to1pos + i]);
									}
									datauri.printf("%s\n", str.str());
									datauri.flush();
								}

								uri2to1pos += n;
								uri2to1rd  += n;
								updated = true;
								wait    = false;
							}
						}

						if (n < 0) {
							fprintf(stderr, "S\ntream 2 closed\n");
							break;
						}

						if (uri1to2pos && (uri2.bytesqueued() == 0)) {
							n  = uri1to2pos;
							n -= n % uri2bs;

							if ((n = uri2.writebytes(uri1to2buf, n)) > 0) {
								uri1to2pos -= n;
								uri1to2wr  += n;
								if (uri1to2pos) memmove(uri1to2buf, uri1to2buf + n, uri1to2pos);
								updated = true;
								wait    = false;
							}

							if (n < 0) {
								fprintf(stderr, "Stream 2 closed\n");
								break;
							}
						}

						if (uri2to1pos && (uri1.bytesqueued() == 0)) {
							n  = uri2to1pos;
							n -= n % uri1bs;

							if ((n = uri1.writebytes(uri2to1buf, n)) > 0) {
								uri2to1pos -= n;
								uri2to1wr  += n;
								if (uri2to1pos) memmove(uri2to1buf, uri2to1buf + n, uri2to1pos);
								updated = true;
								wait    = false;
							}

							if (n < 0) {
								fprintf(stderr, "Stream 1 closed\n");
								break;
							}
						}

						if ((!wait || updated) && ((::GetTickCount() - lastupdate) >= updatetime)) {
							fprintf(stderr, "\r%u/%u <-> %u/%u", uri1to2rd, uri2to1wr, uri1to2wr, uri2to1rd);
							fflush(stderr);

							lastupdate = ::GetTickCount();
							updated    = false;
						}
						else if (wait) {
							Sleep(10);
						}
					}
			
					fprintf(stderr, "\r%u/%u <-> %u/%u\n", uri1to2rd, uri2to1wr, uri1to2wr, uri2to1rd);

					uri2.close();
				}
				else fprintf(stderr, "Failed to open stream %s\n", argv[arg + 1]);

				uri1.close();
			}
			else fprintf(stderr, "Failed to open stream %s\n", argv[arg]);
		}
		while (reconnect && !QuitHandler.HasQuit());
	}
	else fprintf(stderr, "Failed to allocate two buffers of %ukb\n", bufsize / 1024);

	datauri.close();

	if (uri1to2buf) delete[] uri1to2buf;
	if (uri2to1buf) delete[] uri2to1buf;

	return 0;
}

