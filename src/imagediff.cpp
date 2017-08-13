
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <vector>
#include <algorithm>

#include <rdlib/strsup.h>
#include <rdlib/QuitHandler.h>

#include "imagediff_private.h"

#include "ImageDiffer.h"

AQuitHandler quithandler;

volatile bool hupdetected = false;

#ifdef __LINUX__
static void detecthup(int sig)
{
	hupdetected |= (sig == SIGHUP);
}
#endif

int main(int argc, char *argv[])
{
	int  i;
	bool run = true;
	
#ifdef __LINUX__
	signal(SIGHUP, &detecthup);
#endif
	
	for (i = 1; i < argc; i++) {
		if ((stricmp(argv[i], "-help") == 0) || (stricmp(argv[i], "-h") == 0)) {
			fprintf(stderr, "imagediff " VER_STRING "\n");
			printf("Usage: imagediff [<options>]\n");
			printf("Where <options> is one or more of:\n");
			printf("  -h or -help\t\thelp text (this)\n");
			printf("  -cmp <index> <jpeg-1> <jpeg-2> <det-jpeg>\tRun single round of differ <index> on pictures <jpeg-1> and <jpeg-2> and save the detection data to <det-jpeg>n");
			run = false;
		}
		else if (stricmp(argv[i], "-cmp") == 0) {
			ImageDiffer differ(atoi(argv[++i]));
			const char  *file1 = argv[++i];
			const char  *file2 = argv[++i];
			const char  *file3 = argv[++i];

			differ.Compare(file1, file2, file3);
			run = false;
		}
	}
		
	if (run) {
		ADataList differs;
		
		differs.SetDestructor(&ImageDiffer::Delete);

		uint_t i, ndiffers = (uint_t)ImageDiffer::GetGlobalSetting("sources", "1");
		for (i = 0; i < ndiffers; i++) {
			ImageDiffer *differ;

			if ((differ = new ImageDiffer(1 + i)) != NULL) {
				differs.Add((uptr_t)differ);
				differ->Start();
			}
		}
	
		while (!quithandler.HasQuit() && !hupdetected && (ImageDiffer::DiffersRunning() > 0)) {
			Sleep(100);
		}

		differs.DeleteList();
	}
	
	return 0;
}
