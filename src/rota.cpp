
#include <cassert>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <vector>
#include <map>

#include <rdlib/DateTime.h>

typedef struct
{
    ADateTime start, end;
} timespan_t;

typedef struct
{
    ADateTime              start;
    size_t                 weekindex;
    size_t                 index;
    uint32_t               startsegment;
    uint32_t               nsegments;
    uint32_t               nworksegments;
} workday_t;

typedef struct
{
    size_t                 personindex;
    size_t                 index;
    ADateTime              start;
    uint32_t               nsegments;
    uint32_t               nworksegments;
    std::vector<workday_t> days;
} workweek_t;

typedef struct
{
    AString                 name;
    size_t                  index;
    uint32_t                minsegmentspershift;
    uint32_t                maxsegmentsperweek;
    std::vector<uint32_t>   maxsegmentsperday;
    std::vector<timespan_t> holidays;
    std::vector<timespan_t> regularexclusions;
    std::vector<workweek_t> weeks;
} person_t;

typedef struct
{
    uint8_t                 minoccupancy;
    uint8_t                 maxoccupancy;
} occupancy_t;

typedef struct
{
    size_t                   index;
    timespan_t               timespan;
    uint32_t                 maxsegments;
    uint32_t                 shiftsize;
    std::vector<occupancy_t> occupancy;
} day_t;

typedef struct
{
    std::vector<day_t>    days;
} week_t;

typedef struct
{
    ADateTime             start;
    size_t                weekindex;
    size_t                index;
    std::vector<uint32_t> occupancy;
    uint32_t              nsegments;
    uint32_t              nworksegments;
} rotaday_t;

typedef struct
{
    ADateTime              start;
    size_t                 index;
    std::vector<rotaday_t> days;
    uint32_t               nsegments;
    uint32_t               nworksegments;
} rotaweek_t;

typedef struct
{
    ADateTime               start;
    std::vector<rotaweek_t> weeks;
    uint32_t                nsegments;
    uint32_t                nworksegments;
} rota_t;

static bool verbose = false;

static const uint32_t segmentsperhour = 4;

static uint32_t hourstosegments(double hours, bool roundup = false)
{
    return (uint32_t)(roundup ? ceil(hours * (double)segmentsperhour) : hours * (double)segmentsperhour);
}

static double segmentstohours(uint32_t segments)
{
    return (double)segments / (double)segmentsperhour;
}

static ADateTime segmentstodt(uint32_t segments)
{
    return ADateTime(((uint64_t)segments * (uint64_t)3600 * (uint64_t)1000) / (uint64_t)segmentsperhour);
}

static uint32_t dttosegments(const ADateTime& dt)
{
    const uint64_t div = (uint64_t)3600 * (uint64_t)1000 / (uint64_t)segmentsperhour;
    return (uint32_t)((uint64_t)dt / div);
}

static uint32_t occupysegmentstoworksegments(uint32_t n)
{
    if (n <= hourstosegments(6.0)) return n;
    if (n <  hourstosegments(11.0)) return n - 1;
    return n - 4;
}

static void populate(day_t& day, size_t index)
{
    day.index = index;
    day.timespan.start.SetDays((uint32_t)index);
    day.timespan.end.SetDays((uint32_t)index);
}

static void populate(week_t& week, day_t& day, size_t startindex, size_t endindex, uint8_t minoccupancy = 2, uint8_t maxoccupancy = 4)
{
    day.maxsegments = dttosegments(day.timespan.end - day.timespan.start);
    day.occupancy.resize(day.maxsegments);

    for (size_t i = 0; i < day.occupancy.size(); i++)
    {
        day.occupancy[i].minoccupancy = minoccupancy;
        day.occupancy[i].maxoccupancy = maxoccupancy;
    }

    for (size_t i = startindex; i < endindex; i++)
    {
        populate(day, i);
        week.days.push_back(day);
    }
}

static bool addshift(rota_t& rota, person_t& person, size_t weekindex, size_t dayindex, uint32_t startsegment, uint32_t nsegments)
{
    auto& workweek = person.weeks[weekindex];
    auto& workday  = workweek.days[dayindex];
    auto& rotaweek = rota.weeks[weekindex];
    auto& rotaday  = rotaweek.days[dayindex];
    bool  success  = false;

    if ((startsegment < rotaday.occupancy.size()) &&
        ((nsegments = std::min(nsegments, (uint32_t)rotaday.occupancy.size() - startsegment)) > 0) &&
        ((startsegment + nsegments) > (workday.startsegment + workday.nsegments)) &&
        (workday.nworksegments < person.maxsegmentsperday[dayindex]) &&
        (workweek.nworksegments < person.maxsegmentsperweek))
    {
        uint32_t weeksegments = 0;
        for (size_t i = 0 ; i < workweek.days.size(); i++)
        {
            assert(i < workweek.days.size());
            if (i != dayindex)
            {
                weeksegments += workweek.days[i].nworksegments;
            }
        }

        if ((workday.nsegments > 0) &&
            (startsegment != (workday.startsegment + workday.nsegments)))
        {
            nsegments    = (startsegment + nsegments) - (workday.startsegment + workday.nsegments);
            startsegment = workday.startsegment + workday.nsegments;
        }

        while (nsegments > 0)
        {
            uint32_t newdaysegments = occupysegmentstoworksegments(workday.nsegments + nsegments);

            if ((newdaysegments                  <= person.maxsegmentsperday[dayindex]) &&
                ((weeksegments + newdaysegments) <= person.maxsegmentsperweek))
                {
                    break;
                }

            nsegments--;
        }

        if ((workday.nsegments + nsegments) >= person.minsegmentspershift)
        {
            if (workday.nsegments == 0)
            {
                workday.startsegment = startsegment;
            }

            rota.nworksegments     -= workday.nworksegments;
            rotaweek.nworksegments -= workday.nworksegments;
            rotaday.nworksegments  -= workday.nworksegments;
            workweek.nworksegments -= workday.nworksegments;
            workday.nsegments      += nsegments;
            workday.nworksegments   = occupysegmentstoworksegments(workday.nsegments);
            workweek.nsegments     += nsegments;
            workweek.nworksegments += workday.nworksegments;

            rotaday.nsegments      += nsegments;
            rotaday.nworksegments  += workday.nworksegments;
            rotaweek.nsegments     += nsegments;
            rotaweek.nworksegments += workday.nworksegments;
            rota.nsegments         += nsegments;
            rota.nworksegments     += workday.nworksegments;

            for (uint32_t i = 0; i < nsegments; i++)
            {
                assert((startsegment + i) < (uint32_t)rotaday.occupancy.size());
                rotaday.occupancy[startsegment + i]++;

                if (verbose)
                {
                    printf("Segment %u/%u time %s occupancy %u (%s)\n",
                           i, nsegments,
                           ADateTime(rotaday.start + segmentstodt(startsegment + i)).DateFormat("%d %M/%M/%Y %h:%m").str(),
                           rotaday.occupancy[startsegment + i],
                           person.name.str());
                }
            }

            if (verbose)
            {
                printf("Adding '%s' for shift %s - %s (%u/%u day segments, %u/%u week segments)\n",
                       person.name.str(),
                       (workday.start + segmentstodt(startsegment)).DateToStr().str(),
                       (workday.start + segmentstodt(startsegment + nsegments)).DateToStr().str(),
                       workday.nworksegments, person.maxsegmentsperday[dayindex],
                       workweek.nworksegments, person.maxsegmentsperweek);
            }

            success = true;
        }
    }

    return success;
}

int main(int argc, char *argv[])
{
    week_t                 week;
    std::vector<person_t>  persons;
    rota_t                 rota;

    for (int i = 1; i < argc; i++)
    {
        if (stricmp(argv[i], "-v") == 0) verbose = true;
    }

    {
        day_t day;

        day.timespan.start = ADateTime("8am", ADateTime::Time_Absolute);
        day.timespan.end   = ADateTime("7:30pm", ADateTime::Time_Absolute);
        day.shiftsize      = hourstosegments(6.0);

        populate(week, day, 0, 5, 2);

        day.timespan.start = ADateTime("8:30am", ADateTime::Time_Absolute);
        day.timespan.end   = ADateTime("5pm", ADateTime::Time_Absolute);
        day.shiftsize      = hourstosegments(3.0);

        populate(week, day, 5, 7, 1);

        for (size_t i = 0; i < 4; i++)
        {
            week.days[5].occupancy[i].minoccupancy++;
        }
    }

    {
        person_t person;

        person.name                = "GE";
        person.index               = persons.size();
        person.minsegmentspershift = hourstosegments(6.0);
        person.maxsegmentsperweek  = hourstosegments(35.0);

        person.maxsegmentsperday.push_back(hourstosegments(12.0));
        person.maxsegmentsperday.push_back(hourstosegments(6.0));
        person.maxsegmentsperday.push_back(hourstosegments(6.0));
        person.maxsegmentsperday.push_back(hourstosegments(12.0));
        person.maxsegmentsperday.push_back(hourstosegments(6.0));
        person.maxsegmentsperday.push_back(hourstosegments(12.0));
        person.maxsegmentsperday.push_back(hourstosegments(12.0));

        persons.push_back(person);
    }
    {
        person_t person;

        person.minsegmentspershift = hourstosegments(3.0);
        person.maxsegmentsperweek  = hourstosegments(35.0);

        person.maxsegmentsperday.push_back(hourstosegments(6.0));
        person.maxsegmentsperday.push_back(hourstosegments(6.0));
        person.maxsegmentsperday.push_back(hourstosegments(6.0));
        person.maxsegmentsperday.push_back(hourstosegments(6.0));
        person.maxsegmentsperday.push_back(hourstosegments(6.0));
        person.maxsegmentsperday.push_back(hourstosegments(12.0));
        person.maxsegmentsperday.push_back(hourstosegments(12.0));

        for (size_t i = 0; i < 4; i++)
        {
            person.name  = AString("R%").Arg(1 + i);
            person.index = persons.size();
            persons.push_back(person);
        }
    }

    rota.start = ADateTime("now");
    rota.start.SetDays(rota.start.GetDays() - rota.start.GetWeekDay() + 7);
    rota.start.SetMS(0);
    rota.nsegments = 0;
    rota.nworksegments = 0;

    const uint32_t minoverlap = hourstosegments(.5);
    for (size_t weekindex = 0; weekindex < 1; weekindex++)
    {
        size_t i;

        {
            workday_t  workday;
            workweek_t workweek;
            rotaday_t  rotaday;
            rotaweek_t rotaweek;

            rotaweek.start         = rota.start;
            rotaweek.index         = weekindex;
            rotaweek.nsegments     = 0;
            rotaweek.nworksegments = 0;

            rotaday.nsegments      = 0;
            rotaday.nworksegments  = 0;

            for (i = 0; i < week.days.size(); i++)
            {
                rotaday.start     = rotaweek.start + week.days[i].timespan.start;
                rotaday.weekindex = rotaweek.index;
                rotaday.index     = i;
                rotaday.occupancy.resize(week.days[i].occupancy.size());
                rotaweek.days.push_back(rotaday);
            }

            rota.weeks.push_back(rotaweek);

            workweek.start         = rotaweek.start;
            workweek.index         = rotaweek.index;
            workweek.nsegments     = 0;
            workweek.nworksegments = 0;

            workday.startsegment   = 0;
            workday.nsegments      = 0;
            workday.nworksegments  = 0;

            for (i = 0; i < week.days.size(); i++)
            {
                workday.start     = workweek.start + week.days[i].timespan.start;
                workday.weekindex = workweek.index;
                workday.index     = i;
                workweek.days.push_back(workday);
            }

            for (i = 0; i < persons.size(); i++)
            {
                auto& person = persons[i];

                workweek.personindex = i;
                person.weeks.push_back(workweek);
            }
        }

        assert(weekindex < rota.weeks.size());
        assert(rota.weeks[weekindex].days.size() == week.days.size());

        for (size_t dayindex = 0; dayindex < week.days.size(); dayindex++)
        {
            auto& rotaday               = rota.weeks[weekindex].days[dayindex];
            auto& occupancy             = rotaday.occupancy;
            const auto& occupancylimits = week.days[dayindex].occupancy;
            const uint32_t dayshiftsize = week.days[dayindex].shiftsize;
            bool  failed                = false;

            assert(occupancy.size() == occupancylimits.size());

            while (!failed)
            {
                uint32_t segment;

                for (segment = 0; (segment < occupancy.size()) && (occupancy[segment] >= occupancylimits[segment].minoccupancy); segment++) ;

                if (segment == rotaday.occupancy.size())
                {
                    break;
                }

                size_t rnd;
                for (rnd = 0; rnd < 2; rnd++)
                {
                    for (i = 0; i < persons.size(); i++)
                    {
                        auto&    person    = persons[i];
                        uint32_t overlap   = (rnd > 0) ? std::min(segment, minoverlap) : 0;
                        uint32_t shiftsize = std::min(dayshiftsize, (uint32_t)occupancy.size() - (segment - overlap));

                        assert(weekindex < person.weeks.size());
                        assert(person.weeks[weekindex].days.size() == week.days.size());

                        if (((rnd > 0) || (person.weeks[weekindex].days[dayindex].nsegments > 0)) &&
                            addshift(rota, person, weekindex, dayindex, segment - overlap, shiftsize))
                        {
                            break;
                        }
                    }

                    if (i < persons.size()) break;
                }

                if ((rnd == 2) && (i == persons.size()))
                {
                    uint32_t shiftsize = std::min(dayshiftsize, (uint32_t)occupancy.size() - segment);
                    printf("Failed to find someone for shift %s-%s (segments %u-%u)\n",
                           ADateTime(rotaday.start + segmentstodt(segment)).DateFormat("%d %h:%m").str(),
                           ADateTime(rotaday.start + segmentstodt(segment + shiftsize)).DateFormat("%h:%m").str(),
                           segment,
                           segment + shiftsize);
                    failed = true;
                    break;
                }
            }
        }
    }

    for (size_t weekindex = 0; weekindex < rota.weeks.size(); weekindex++)
    {
        const auto& rotaweek = rota.weeks[weekindex];
        size_t j, k;

        printf("\nWeek beginning %s:\n", rotaweek.start.DateToStr().str());

        std::vector<std::vector<AString>> lines(1 + persons.size());

        for (j = 0; j < lines.size(); j++)
        {
            lines[j].resize(2 + week.days.size());
        }

        int maxwidth0 = 0, maxwidth = 0, maxwidth1 = 0;
        lines[0][0] = "Who";
        maxwidth0 = std::max(maxwidth0, lines[0][0].len());

        lines[0][1 + week.days.size()] = "Hours";
        maxwidth1 = std::max(maxwidth1, lines[0][1 + week.days.size()].len());

        for (j = 0; j < week.days.size(); j++)
        {
            ADateTime dt = rota.start + ADateTime((uint32_t)j, 0U);

            lines[0][1 + j] = dt.DateFormat("%d %D/%M");
            maxwidth = std::max(maxwidth, lines[0][1 + j].len());
        }

        for (j = 0; j < persons.size(); j++)
        {
            const auto& person   = persons[j];
            const auto& workweek = person.weeks[weekindex];

            lines[1 + j][0] = person.name;
            maxwidth0 = std::max(maxwidth0, lines[1 + j][0].len());

            for (k = 0; k < workweek.days.size(); k++)
            {
                const auto& workday = workweek.days[k];

                if (workday.nsegments > 0)
                {
                    ADateTime start = workday.start + segmentstodt(workday.startsegment);
                    ADateTime end   = start + segmentstodt(workday.nsegments);
                    lines[1 + j][1 + k] = AString("%-%(%.2)").Arg(start.DateFormat("%h:%m")).Arg(end.DateFormat("%h:%m")).Arg(segmentstohours(workday.nworksegments));
                    maxwidth = std::max(maxwidth, lines[1 + j][1 + k].len());
                }
            }

            if (workweek.nworksegments > 0)
            {
                lines[1 + j][1 + k] = AString("%.2").Arg(segmentstohours(workweek.nworksegments));
                maxwidth1 = std::max(maxwidth1, lines[1 + j][1 + k].len());
            }
        }

        maxwidth0 += 2;
        maxwidth  += 2;
        maxwidth1 += 2;
        AString fmt0, fmt, fmt1;
        fmt0.printf("|%%-%us", (uint32_t)maxwidth0);
        fmt.printf("|%%-%us", (uint32_t)maxwidth);
        fmt1.printf("|%%-%us", (uint32_t)maxwidth1);

        for (j = 0; j < lines.size(); j++)
        {
            const auto& line = lines[j];

            for (k = 0; k < line.size(); k++)
            {
                const auto& str = line[k];

                if (k == 0)
                {
                    printf(fmt0.str(), (AString(" ").Copies((maxwidth0 - str.len()) / 2) + str).str());
                }
                else if (k < (line.size() - 1))
                {
                    printf(fmt.str(), (AString(" ").Copies((maxwidth - str.len()) / 2) + str).str());
                }
                else
                {
                    printf(fmt1.str(), (AString(" ").Copies((maxwidth1 - str.len()) / 2) + str).str());
                }
            }

            printf("|\n");

            if (j == 0)
            {
                for (k = 0; k < line.size(); k++)
                {
                    if (k == 0)
                    {
                        printf(fmt0.str(), AString("-").Copies(maxwidth0).str());
                    }
                    else if (k < (line.size() - 1))
                    {
                        printf(fmt.str(), AString("-").Copies(maxwidth).str());
                    }
                    else
                    {
                        printf(fmt1.str(), AString("-").Copies(maxwidth1).str());
                    }
                }

                printf("|\n");
            }
        }
    }

    return 0;
}
