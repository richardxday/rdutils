
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/strsup.h>
#include <rdlib/StdFile.h>
#include <rdlib/DateTime.h>

int main(void)
{
    AStdData  *fp = Stdin;
    AStdFile  ofp;
    AString   dir = "/media/data/richard/pingchecker";
    AString   logfilename = "pingchecker-%Y-%M.txt";
    AString   datfilename = "pingtimes-%Y-%M.dat";
    ADateTime dt  = ADateTime::MinDateTime;
    ADateTime gap(0, 5000);
    AString   str = "time=";
    AString   line;
    uint32_t  minute    = ~0;
    double    pingtime  = 0.0;
    uint_t    pingcount = 0;

    while (line.ReadLn(fp) >= 0) {
        double t;
        int p;

        if (((p = line.Pos(str)) >= 0) &&
            (sscanf(line.str() + p + str.len(), "%lf", &t) > 0)) {
            ADateTime dt1;

            pingtime += t;
            pingcount++;

            if ((dt1 - dt) >= gap) {
                if (ofp.open(dir.CatPath(dt1.DateFormat(logfilename)), "a")) {
                    if (dt > ADateTime::MinDateTime) {
                        ofp.printf("%s: %lus since last ping\n", dt1.DateFormat("%Y-%M-%D %h:%m:%s").str(), (ulong_t)((uint64_t)(dt1 - dt) / 1000));
                    }
                    else {
                        ofp.printf("%s: started\n", dt1.DateFormat("%Y-%M-%D %h:%m:%s").str());
                    }
                    ofp.close();
                }
            }

            if ((dt1.GetMS() / 60000) != minute) {
                pingtime /= (double)pingcount;

                AStdFile dfp;
                if (dfp.open(dir.CatPath(dt1.DateFormat(datfilename)), "a")) {
                    static uint8_t lastday = 0;

                    if (dt1.GetDay() != lastday) dfp.printf("\n");
                    dfp.printf("%u %0.6lf %u %lf\n", (uint_t)dt1.GetDay(), (double)dt1.GetMS() / (3600.0 * 1000.0), pingcount, pingtime);
                    lastday = dt1.GetDay();
                    dfp.close();
                }

                pingtime  = 0.0;
                pingcount = 0;
                minute    = dt1.GetMS() / 60000;
            }

            dt = dt1;
        }
    }

    return 0;
}
