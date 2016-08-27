
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <vector>
#include <algorithm>

#include <rdlib/strsup.h>
#include <rdlib/QuitHandler.h>

#include "ImageDiffer.h"

static volatile bool hupsignal = false;

static void detecthup(int sig)
{
	hupsignal |= (sig == SIGHUP);
}

int main(void)
{
	AQuitHandler     quithandler;
	ADataList        differs;
	ASettingsHandler settings("imagediff", ~0);
	ASettingsHandler stats("imagediff-stats", 5000);
	AString  		 loglocation;
	AStdFile  		 log;
	uint32_t 		 delay   = 0;
	uint32_t  		 days    = 0;
	uint32_t		 statswritetime = GetTickCount();
	bool			 update  = true;
	bool			 startup = true;

	signal(SIGHUP, &detecthup);

	differs.SetDestructor(&ImageDiffer::Delete);

	while (!quithandler.HasQuit()) {
		ADateTime dt;
		uint32_t  tick = GetTickCount();
		uint32_t  days1;

		if (update) {
			settings.Read();

			loglocation = settings.Get("loglocation", "/var/log/imagediff");
			delay       = (uint32_t)(1000.0 * (double)settings.Get("delay", "1.0"));
			days        = 0;
		}

		if ((days1 = dt.GetDays()) != days) {
			days = days1;

			log.close();
			log.open(loglocation.CatPath(dt.DateFormat("imagediff-%Y-%M-%D.log")), "a");
				
			if (startup) {
				log.printf("%s[all]: Starting detection...\n", dt.DateFormat("%Y-%M-%D %h:%m:%s").str());
				startup = false;
			}
		}

		if (update) {
			uint_t n = (uint_t)settings.Get("threads", "1");

			while (differs.Count() > n) {
				delete (ImageDiffer *)differs.EndPop();
			}
			while (differs.Count() < n) {
				differs.Add(new ImageDiffer(settings, stats, log, differs.Count() + 1));
			}
		}

		uint_t i, n = differs.Count();
		ImageDiffer **list = (ImageDiffer **)differs.List();
		for (i = 0; i < n; i++) {
			list[i]->Process(dt, update);
		}

		if (update || ((GetTickCount() - statswritetime) >= 2000)) {
			stats.Write();
			statswritetime = GetTickCount();
		}

		uint32_t ticks = GetTickCount() - tick;
		uint32_t diff  = SUBZ(delay, ticks);
		Sleep((uint_t)std::max(diff, 1U));

		if (hupsignal || settings.HasFileChanged()) {
			log.printf("%s[all]: Reloading configuration\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str());
			hupsignal = false;
			update    = true;
		}
		else update = false;
	}

	differs.DeleteList();

	log.close();

	return 0;
}
