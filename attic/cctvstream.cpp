
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <rdlib/QuitHandler.h>
#include <rdlib/SettingsHandler.h>
#include <rdlib/Recurse.h>

#include "MotionDetector.h"

static volatile bool hupsignal = false;

static void detecthup(int sig)
{
	hupsignal |= (sig == SIGHUP);
}

int main(void)
{
	static const AString appname = "cctvstream";
	AQuitHandler     quithandler;
	ASocketServer 	 server;
	ASettingsHandler settings(appname, ~0);
	ASettingsHandler stats(appname + "-stats", 5000);
	AString  		 loglocation;
	AStdFile  		 log;
	uint32_t  		 days    = 0;
	uint32_t		 statswritetime = GetTickCount();
	bool			 update  = true;
	bool			 startup = true;
	MotionDetector   detector(server, settings, stats, log, 0);

	signal(SIGHUP, &detecthup);
	
	while (!quithandler.HasQuit() && !detector.GetStream().StreamComplete()) {
		ADateTime dt;
		uint32_t  days1;

		if (detector.GetStream().IsStreaming()) Sleep(200);
		
		if (update) {
			settings.Read();
			loglocation = settings.Get("loglocation", "/var/log/" + appname);
			CreateDirectory(loglocation);
			days        = 0;
			detector.Configure();
		}

		if ((days1 = dt.GetDays()) != days) {
			days = days1;

			log.close();
			log.open(loglocation.CatPath(dt.DateFormat(appname + "-%Y-%M-%D.log")), "a");
				
			if (startup) {
				log.printf("%s[all]: Starting detection...\n", dt.DateFormat("%Y-%M-%D %h:%m:%s.%S").str());
				startup = false;
			}
		}

		server.Process(1000);
		
		if (update || ((GetTickCount() - statswritetime) >= 2000)) {
			stats.Write();
			statswritetime = GetTickCount();
		}

		if (hupsignal || settings.HasFileChanged()) {
			log.printf("%s[all]: Reloading configuration\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s.%S").str());
			hupsignal = false;
			update    = true;
		}
		else update = false;

		log.flush();
	}

	log.close();
	
	return 0;
}
