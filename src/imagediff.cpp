
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <vector>
#include <algorithm>

#include <rdlib/strsup.h>
#include <rdlib/QuitHandler.h>

#include "ImageDiffer.h"

AQuitHandler quithandler;

volatile bool hupdetected = false;
static void detecthup(int sig)
{
	hupdetected |= (sig == SIGHUP);
}

int main(void)
{
	ADataList differs;

	signal(SIGHUP, &detecthup);
	
	differs.SetDestructor(&ImageDiffer::Delete);

	uint_t i, ndiffers = (uint_t)ImageDiffer::GetGlobalSetting("sources", "1");
	for (i = 0; i < ndiffers; i++) {
		differs.Add((uptr_t)new ImageDiffer(1 + i));
	}
	
	while (!quithandler.HasQuit() && !hupdetected) {
		Sleep(100);
	}

	differs.DeleteList();

	return 0;
}
