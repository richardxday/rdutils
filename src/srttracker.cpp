
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include <rdlib/StdFile.h>
#include <rdlib/DateTime.h>
#include <rdlib/Recurse.h>

typedef struct {
	uint_t    index;
	uint64_t  start, end;
	double    lat, lng;
	uint_t    speed;
	ADateTime dt;
	AString   address;

	double    xpos, ypos;
	double    distance, totaldistance;
	uint64_t  timediff;
	uint_t    journey;

	AString   filename;
	uint_t    ln;
} RECORD;

bool compare(const RECORD& rec1, const RECORD& rec2)
{
	return (rec1.dt < rec2.dt);
}

int main(int argc, char *argv[])
{
	static const double pi180		= M_PI / 180.0;
	static const double EarthRadius = 6371.0e3;
	AStdFile fp;
	AList    files;
	AString  datfilename;
	AString  kmlfilename = "journeys.kml";
	std::vector<RECORD> records;
	int i;

	for (i = 1; i < argc; i++) {
		if		(stricmp(argv[i], "-dat") == 0) datfilename = argv[++i];
		else if (stricmp(argv[i], "-kml") == 0) kmlfilename = argv[++i];
		else CollectFiles(argv[i], "*.srt", 0, files);
	}

	const AString *file = AString::Cast(files.First());
	while (file) {
		if (fp.open(*file)) {
			ADateTime dt;
			AString line;
			RECORD  record;
			uint_t  nitems = 0, ln = 0;
			uint_t  month, day, year;
			uint_t  ival, hrs1, mins1, secs1, ms1, hrs2, mins2, secs2, ms2;
			bool    timeset = false;
			
			if (sscanf(file->FilePart(), "%u_%u_%u_%u_%u_%u", &month, &day, &year, &hrs1, &mins1, &secs1) == 6) {
				dt.Set(day, month, year + 2000, hrs1, mins1, secs1);
			}
			record.distance = 0.0;
			record.timediff = 0;
			record.journey  = 0;
			
			while (line.ReadLn(fp) >= 0) {
				double fval1, fval2;
				AString word1 = line.Word(0).ToLower();

				ln++;
				
				if (line.Empty()) {
					nitems = 0;
				}
				else if (sscanf(line.str(), "%u:%u:%u,%u --> %u:%u:%u,%u", &hrs1, &mins1, &secs1, &ms1, &hrs2, &mins2, &secs2, &ms2) == 8) {
					record.start  = hrs1;
					record.start *= 60;
					record.start += mins1;
					record.start *= 60;
					record.start += secs1;
					record.start *= 1000;
					record.start += ms1;

					record.end    = hrs2;
					record.end   *= 60;
					record.end   += mins2;
					record.end   *= 60;
					record.end   += secs2;
					record.end   *= 1000;
					record.end   += ms2;

					nitems++;
				}
				else if ((sscanf(line.str(), "%u", &ival) == 1) && (ival > 0)) {
					record.index    = ival;
					record.filename = *file;
					record.ln       = ln;
					nitems++;
				}
				else if ((word1 == "location:") && (sscanf(line.Words(1).str(), "%lf,%lf", &fval1, &fval2) == 2)) {
					record.lat = fval1;
					record.lng = fval2;
					nitems++;
				}
				else if ((word1 == "speed:") && (sscanf(line.Words(1).str(), "%u", &ival) == 1)) {
					record.speed = ival;
					nitems++;
				}
				else if (word1 == "time:") {
					AString   timestr = line.Word(4) + line.Word(5).SearchAndReplace(".", "") + " " + line.Word(2).SearchAndReplace(",", "") + "-" + line.Word(1) + "-" + line.Word(3);
					ADateTime dt2;
					
					dt2.StrToDate(timestr, ADateTime::Time_Absolute);
					if (!timeset) {
						if (dt2.GetHours() > dt.GetHours()) {
							dt += 12 * 3600 * 1000;
						}
						timeset = true;
					}

					if (dt2 > dt) dt = dt2;
					
					record.dt  = dt;
					uint64_t diff = std::max(record.end - record.start, 1000UL);
					dt        += diff;
					nitems++;
				}
				else if (word1 == "address:") {
					record.address = line.Words(1);
					nitems++;
				}

				if (nitems == 6) {
					if ((record.lat != 0.0) || (record.lng != 0.0)) {
						record.xpos	= EarthRadius * sin(record.lng * pi180) * cos(record.lat * pi180);
						record.ypos	= EarthRadius * sin(record.lat * pi180);
						
						records.push_back(record);
					}
					
					nitems = 0;
				}
			}

			fp.close();
		}

		file = file->Next();
	}

	std::sort(records.begin(), records.end(), compare);

	if (records.size() > 1) {
		double totaldistance = 0.0;
		size_t i;
		uint_t journey = 0;
	
		for (i = 1; i < records.size(); i++) {
			const RECORD& lastrecord = records[i  -1];
			RECORD& record 			 = records[i];
			double  lat1			 = lastrecord.lat * pi180;
			double  lng1			 = lastrecord.lng * pi180;
			double  lat2			 = record.lat * pi180;
			double  lng2			 = record.lng * pi180;
			double  latdiff          = (lat2 - lat1);
			double  lngdiff          = (lng2 - lng1);
			double  a = sin(latdiff * .5) * sin(latdiff * .5) + cos(lat1) * cos(lat2) * sin(lngdiff * .5) * sin(lngdiff * .5);
			double  c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

			record.distance = EarthRadius * c / 1604.0;
			record.timediff = (uint64_t)(record.dt - lastrecord.dt);

			totaldistance  += record.distance;

			if ((record.timediff >= 120000) || (record.distance >= 4.0)) {
				debug("Journey %u distance %0.1lf miles\n", journey + 1, lastrecord.totaldistance);
				totaldistance = 0.0;
				journey++;
			}

			record.journey       = journey;
			record.totaldistance = totaldistance;
		}
	}
	
	if (records.size() && datfilename.Valid() && fp.open(datfilename, "w")) {
		const ADateTime& starttime   = records[0].dt;
		ADateTime        journeytime = starttime;
		size_t i;
		uint_t journey = 0;
		
		for (i = 0; i < records.size(); i++) {
			const RECORD& record = records[i];

			if (record.journey > journey) {
				journey     = record.journey;
				journeytime = record.dt;
				fp.printf("\n\n");
			}

			fp.printf("%0.14le %0.14le %u %u %0.14le %0.14le %0.14lf %0.14lf %0.3lf %0.3lf %0.3lf '%s' '%s' '%s' %u %u\n",
					  record.lat, record.lng, record.speed, record.journey,
					  record.xpos, record.ypos, record.distance, record.totaldistance,
					  (double)record.timediff * .001,
					  (double)((uint64_t)record.dt - (uint64_t)journeytime) * .001,
					  (double)((uint64_t)record.dt - (uint64_t)starttime) * .001,
					  record.dt.DateToStr().str(),
					  record.address.str(),
					  record.filename.FilePart().str(),
					  record.ln,
					  record.index);
		}

		fp.close();
	}

	if (records.size() && kmlfilename.Valid() && fp.open(kmlfilename, "w")) {
		size_t i, j;
		uint_t journey = 0;

		fp.printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
		fp.printf("<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
		fp.printf("  <Document>\n");
		fp.printf("    <name>SRT Tracks</name>\n");
		fp.printf("    <description>SRT Tracks</description>\n");
		fp.printf("    <Style id=\"tracklines\">\n");
		fp.printf("      <LineStyle>\n");
		fp.printf("        <color>7fff00ff</color>\n");
		fp.printf("        <width>2</width>\n");
		fp.printf("      </LineStyle>\n");
		fp.printf("      <PolyStyle>\n");
		fp.printf("        <color>7fff0000</color>\n");
		fp.printf("      </PolyStyle>\n");
		fp.printf("    </Style>\n");

		bool started = false;
		for (i = 0; i < records.size(); i++) {
			const RECORD& record = records[i];

			if (started && (record.journey != journey)) {
				fp.printf("        </coordinates>\n");
				fp.printf("      </LineString>\n");
				fp.printf("    </Placemark>\n");
				started = false;
			}

			if (!started) {
				for (j = i + 1; (j < records.size()) && (records[j].journey == records[i].journey); j++) ;

				const RECORD& endrecord = records[j - 1];
				
				fp.printf("    <Placemark>\n");
				fp.printf("      <name>Journey %u: start %s</name>\n", record.journey + 1, record.dt.DateToStr().str());
				fp.printf("      <description>Start at %s</description>\n", record.address.str());
				fp.printf("      <Point>\n");
				fp.printf("        <coordinates>\n");
				fp.printf("          %0.14le,%0.14le,%u\n", record.lng, record.lat, record.speed);
				fp.printf("        </coordinates>\n");
				fp.printf("      </Point>\n");
				fp.printf("    </Placemark>\n");

				fp.printf("    <Placemark>\n");
				fp.printf("      <name>Journey %u: end %s</name>\n", endrecord.journey + 1, endrecord.dt.DateToStr().str());
				fp.printf("      <description>End at %s</description>\n", endrecord.address.str());
				fp.printf("      <Point>\n");
				fp.printf("        <coordinates>\n");
				fp.printf("          %0.14le,%0.14le,%u\n", endrecord.lng, endrecord.lat, endrecord.speed);
				fp.printf("        </coordinates>\n");
				fp.printf("      </Point>\n");
				fp.printf("    </Placemark>\n");

				fp.printf("    <Placemark>\n");
				fp.printf("      <name>Journey %u: %s - %s</name>\n", record.journey + 1, record.dt.DateToStr().str(), endrecord.dt.DateToStr().str());
				fp.printf("      <description>Total %0.3lf miles</description>\n", endrecord.totaldistance);
				fp.printf("      <styleUrl>#tracklines</styleUrl>\n");
				fp.printf("      <LineString>\n");
				fp.printf("        <extrude>1</extrude>\n");
				fp.printf("        <tessellate>1</tessellate>\n");
				fp.printf("        <altitudeMode>relativeToGround</altitudeMode>\n");
				fp.printf("        <coordinates>\n");
				
				started = true;
				journey = record.journey;
			}
				
			fp.printf("            %0.14le,%0.14le,%u\n", record.lng, record.lat, record.speed);
		}

		if (started) {
			fp.printf("        </coordinates>\n");
			fp.printf("      </LineString>\n");
			fp.printf("    </Placemark>\n");
		}
		
		fp.printf("  </Document>\n");
		fp.printf("</kml>\n");
		fp.close();
	}
	
	return 0;
}
