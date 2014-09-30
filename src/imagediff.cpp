
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <vector>

#include <rdlib/strsup.h>
#include <rdlib/QuitHandler.h>

#include "ImageDiffer.h"

static volatile bool hupsignal = false;

static void detecthup(int sig)
{
	hupsignal |= (sig == SIGHUP);
}

int main(int argc, char *argv[])
{
	AQuitHandler     quithandler;
	ADataList        differs;
	ASettingsHandler settings("imagediff", ~0);
	ASettingsHandler stats("imagediff-stats", 5000);
	AString  		 loglocation;
	AStdFile  		 log;
	uint64_t 		 delay;
	uint32_t  		 days    = 0;
	uint32_t		 statswritetime = GetTickCount();
	bool			 update  = true;
	bool			 startup = true;

	signal(SIGHUP, &detecthup);

	differs.SetDestructor(&ImageDiffer::Delete);

	while (!quithandler.HasQuit()) {
		ADateTime dt;
		uint32_t  days1;

		if (update) {
			settings.Read();

			loglocation = settings.Get("loglocation", "/var/log/imagediff");
			delay       = (uint64_t)(1000.0 * (double)settings.Get("delay", "1.0"));
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

		uint64_t dt1    = dt;
		uint64_t dt2    = ADateTime();
		uint64_t msdiff = SUBZ(dt1 + delay, dt2);
		Sleep((uint_t)msdiff);

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
