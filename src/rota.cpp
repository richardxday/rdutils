
#include <cassert>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <vector>
#include <map>

#include <rdlib/StdFile.h>
#include <rdlib/DateTime.h>

typedef struct {
    std::vector<bool> present;
    size_t            count;
} staffday_t;
typedef struct {
    std::map<AString, staffday_t> staff;
    std::vector<size_t>           counts;
    size_t                        count;
} day_t;
typedef struct {
    std::vector<day_t> days;
} rota_t;

AString& getline(std::vector<AString>& lines, size_t ln)
{
    if (ln >= lines.size()) lines.resize(lines.size() + 1);
    return lines[ln];
}

int main(int argc, char *argv[])
{
    AStdFile fp;

    if ((argc > 1) && fp.open(argv[1])) {
        const sint_t daystart = (sint_t)( 8.0 * 2.0);
        const sint_t dayend   = (sint_t)(19.5 * 2.0);
        rota_t  rota;
        AString line;

        while (line.ReadLn(fp) >= 0) {
            uint_t  dayidx = (uint_t)line.Word(0);
            AString person = line.Word(1);
            sint_t  time   = (sint_t)((double)line.Word(2) * 2.0);
            sint_t  hours  = (sint_t)((double)line.Word(3) * 2.0);
            sint_t  start  = (hours >= 0) ? time         : time + hours;
            sint_t  end    = (hours >= 0) ? time + hours : time;

            start = std::max(start, daystart);
            end   = std::min(end,   dayend);

            if (end > start) {
                size_t i;

                start -= daystart;
                end   -= daystart;

                if ((size_t)dayidx >= rota.days.size()) rota.days.resize((size_t)dayidx + 1);

                auto& staffday = rota.days[dayidx].staff[person];
                staffday.present.resize(dayend - daystart);

                for (i = (size_t)start; i < (size_t)end; i++) staffday.present[i] = true;

                for (i = staffday.count = 0; i < staffday.present.size(); i++) {
                    staffday.count += staffday.present[i] ? 1 : 0;
                }
            }
        }

        fp.close();

        std::vector<AString> lines;
        size_t ln = 0, nlines = 0, ncolumns = 0;
        for (size_t dayidx = 0; dayidx < rota.days.size(); dayidx++) {
            auto& day = rota.days[dayidx];

            day.counts.resize(dayend - daystart);

            getline(lines, ln).printf("%-8s | ", AString::Formatify("Week %u", (uint_t)(dayidx / 7)).str());
            for (size_t segidx = 0; segidx < day.counts.size(); segidx++) {
                getline(lines, ln).printf("%u", (uint_t)((segidx + daystart) / 20));
            }
            getline(lines, ln++).printf(" |");

            getline(lines, ln).printf("%-8s | ", "");
            for (size_t segidx = 0; segidx < day.counts.size(); segidx++) {
                getline(lines, ln).printf("%u", (uint_t)(((segidx + daystart) / 2) % 10));
            }
            getline(lines, ln++).printf(" |");

            getline(lines, ln).printf("%-8s | ", AString::Formatify("Day  %u", (uint_t)(dayidx % 7)).str());
            for (size_t segidx = 0; segidx < day.counts.size(); segidx++) {
                getline(lines, ln).printf("%u", ((segidx + daystart) & 1) ? 3 : 0);
            }
            getline(lines, ln++).printf(" |");

            getline(lines, ln).printf("%-8s | ", "");
            getline(lines, ln).printf("%s", AString("0").Copies((int)day.counts.size()).str());
            getline(lines, ln++).printf(" |");

            getline(lines, ln++).printf("%s-+-%s-+%s", AString("-").Copies(8).str(), AString("-").Copies((int)day.counts.size()).str(), AString("-").Copies(5).str());

            auto ln2 = ln;
            for (auto it2 = day.staff.begin(); it2 != day.staff.end(); ++it2) {
                auto& staffday = it2->second;

                getline(lines, ln2).printf("%-8s | ", it2->first.str());

                for (size_t segidx = 0; segidx < staffday.present.size(); segidx++) {
                    size_t inc = staffday.present[segidx] ? 1 : 0;

                    getline(lines, ln2).printf("%c", staffday.present[segidx] ? '*' : ' ');

                    day.counts[segidx] += inc;
                    day.count          += inc;
                }

                getline(lines, ln2++).printf(" | %4.1f", (double)staffday.count / 2.0);
            }

            ln = ln2;
            getline(lines, ln++).printf("%s-+-%s-+%s", AString("-").Copies(8).str(), AString("-").Copies((int)day.counts.size()).str(), AString("-").Copies(5).str());

            getline(lines, ln).printf("%-8s | ", "Levels");
            for (size_t segidx = 0; segidx < day.counts.size(); segidx++) {
                getline(lines, ln).printf("%u", (uint_t)day.counts[segidx]);
            }
            getline(lines, ln).printf(" | %4.1f", (double)day.count / 2.0);
            ln += 2;

            getline(lines, ln);

            if (nlines == 0) {
                for (size_t i = 0; i < nlines; i++) {
                    ncolumns = std::max(ncolumns, (size_t)lines[i].len());
                }
                nlines = ln;
            }
        }

        for (size_t i = 0; i < lines.size(); i++) {
            printf("%s\n", lines[i].str());
        }
    }

    return 0;
}
