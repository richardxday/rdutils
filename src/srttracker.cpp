
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

	AString   filename;
	uint_t    ln;
} RECORD;

typedef struct {
	AString filename;
	std::vector<RECORD> records;
} RECORDFILE;

static const double pi180		= M_PI / 180.0;
static const double EarthRadius = 6371.0e3;

bool compare(const RECORDFILE& rec1, const RECORDFILE& rec2)
{
	return (rec1.records[0].dt < rec2.records[0].dt);
}

uint32_t GetWeekNumber(const ADateTime& dt)
{
	static ADateTime firstdt = ADateTime::MinDateTime;
	static bool      inited  = false;

	if (!inited) {
		firstdt = dt;
		if (firstdt.GetWeekDay() != 0) {
			firstdt.PrevWeekDay(0);
		}
		firstdt.SetMS(0);

		//debug("First day is %s\n", firstdt.DateToStr().str());

		inited = true;
	}

	return (dt - firstdt).GetDays() / 7;
}

double CalcDistance(const RECORD& rec1, const RECORD& rec2)
{
	double  lat1	= rec1.lat * pi180;
	double  lng1	= rec1.lng * pi180;
	double  lat2	= rec2.lat * pi180;
	double  lng2	= rec2.lng * pi180;
	double  latdiff = (lat2 - lat1);
	double  lngdiff = (lng2 - lng1);
	double  a 		= sin(latdiff * .5) * sin(latdiff * .5) + cos(lat1) * cos(lat2) * sin(lngdiff * .5) * sin(lngdiff * .5);
	double  c 		= 2.0 * atan2(sqrt(a), sqrt(1.0 - a));

	return EarthRadius * c / 1604.0;
}

bool ReadFile(const AString& filename, RECORDFILE& file, std::vector<RECORD> *records = NULL)
{
	AStdFile fp;
	bool success = false;

	if (fp.open(filename)) {
		ADateTime dt;
		AString line;
		RECORD  record;
		uint_t  nitems = 0, ln = 0;
		uint_t  month, day, year;
		uint_t  ival, hrs1, mins1, secs1, ms1, hrs2, mins2, secs2, ms2;
		bool    timeset = false;

		file.filename = filename;

		if (sscanf(filename.FilePart(), "%u_%u_%u_%u_%u_%u", &month, &day, &year, &hrs1, &mins1, &secs1) == 6) {
			dt.Set(day, month, year + 2000, hrs1, mins1, secs1);
		}
		record.distance = record.totaldistance = 0.0;
		record.timediff = 0;

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
				record.filename = filename;
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
				uint64_t diff = std::max(record.end - record.start, (uint64_t)1000);
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

					if (file.records.size() < 2) file.records.push_back(record);
					else file.records[file.records.size() - 1] = record;

					if (records) {
						records->push_back(record);
					}
				}

				nitems = 0;
			}
		}

		fp.close();

		success = (file.records.size() >= 2);
	}

	return success;
}

void WriteRecords(const AString& kmldir, const AString& jrnfilename, const AString& datfilename, std::vector<RECORD>& records)
{
	if (records.size() >= 2) {
		AString  filename = kmldir.CatPath(jrnfilename.Prefix() + records[0].dt.DateFormat("-%Y-%M-%D-%h-%m-%s") + "." + jrnfilename.Suffix());
		AStdFile fp;
		size_t   i;

		for (i = 1; i < records.size(); i++) {
			const RECORD& rec1 = records[i - 1];
			RECORD&       rec2 = records[i];

			rec2.distance      = CalcDistance(rec1, rec2);
			rec2.totaldistance = rec1.totaldistance + rec2.distance;
			rec2.timediff      = (uint64_t)(rec2.dt - rec1.dt);
		}

		CreateDirectory(filename.PathPart());

		if (fp.open(filename, "w")) {
			const RECORD& record1 = records[0];
			const RECORD& record2 = records[records.size() - 1];

			debug("New file '%s'\n", filename.str());

			fp.printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
			fp.printf("<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
			fp.printf("  <Document>\n");
			fp.printf("    <name>Journey %s</name>\n", record1.dt.DateToStr().str());
			fp.printf("    <description>Started %s at %s, ended %s at %s, total %0.1lf miles</description>\n", record1.dt.DateToStr().str(), record1.address.str(), record2.dt.DateToStr().str(), record2.address.str(), record2.totaldistance);
			fp.printf("    <Style id=\"tracklines\">\n");
			fp.printf("      <LineStyle>\n");
			fp.printf("        <color>7fff00ff</color>\n");
			fp.printf("        <width>2</width>\n");
			fp.printf("      </LineStyle>\n");
			fp.printf("      <PolyStyle>\n");
			fp.printf("        <color>7fff0000</color>\n");
			fp.printf("      </PolyStyle>\n");
			fp.printf("    </Style>\n");

			fp.printf("    <Placemark>\n");
			fp.printf("      <name>Journey start %s</name>\n", record1.dt.DateToStr().str());
			fp.printf("      <description>Start at %s</description>\n", record1.address.str());
			fp.printf("      <Point>\n");
			fp.printf("        <coordinates>\n");
			fp.printf("          %0.14le,%0.14le,%u\n", record1.lng, record1.lat, record1.speed);
			fp.printf("        </coordinates>\n");
			fp.printf("      </Point>\n");
			fp.printf("    </Placemark>\n");

			fp.printf("    <Placemark>\n");
			fp.printf("      <name>Journey end %s</name>\n", record2.dt.DateToStr().str());
			fp.printf("      <description>End at %s</description>\n", record2.address.str());
			fp.printf("      <Point>\n");
			fp.printf("        <coordinates>\n");
			fp.printf("          %0.14le,%0.14le,%u\n", record2.lng, record2.lat, record2.speed);
			fp.printf("        </coordinates>\n");
			fp.printf("      </Point>\n");
			fp.printf("    </Placemark>\n");

			fp.printf("    <Placemark>\n");
			fp.printf("      <name>Journey: %s - %s</name>\n", record1.dt.DateToStr().str(), record2.dt.DateToStr().str());
			fp.printf("      <description>Total %0.3lf miles</description>\n", record2.totaldistance);
			fp.printf("      <styleUrl>#tracklines</styleUrl>\n");
			fp.printf("      <LineString>\n");
			fp.printf("        <extrude>1</extrude>\n");
			fp.printf("        <tessellate>1</tessellate>\n");
			fp.printf("        <altitudeMode>relativeToGround</altitudeMode>\n");
			fp.printf("        <coordinates>\n");

			for (i = 0; i < records.size(); i++) {
				fp.printf("            %0.14le,%0.14le,%u\n", records[i].lng, records[i].lat, records[i].speed);
			}

			fp.printf("        </coordinates>\n");
			fp.printf("      </LineString>\n");
			fp.printf("    </Placemark>\n");

			fp.printf("  </Document>\n");
			fp.printf("</kml>\n");
			fp.close();
		}
		else {
			fprintf(stderr, "Failed to open file '%s' for writing\n", filename.str());
		}

		if (datfilename.Valid() && fp.open(datfilename, "a")) {
			static uint64_t starttime = 0;
			uint64_t journeytime = records[0].dt;
			size_t i;

			if (!starttime) starttime = journeytime;

			for (i = 0; i < records.size(); i++) {
				const RECORD& record = records[i];

				fp.printf("%0.14le %0.14le %u %0.14le %0.14le %0.14lf %0.14lf %0.3lf %0.3lf %0.3lf '%s' '%s' '%s' %u %u\n",
						  record.lat, record.lng, record.speed,
						  record.xpos, record.ypos, record.distance, record.totaldistance,
						  (double)record.timediff * .001,
						  (double)((uint64_t)record.dt - journeytime) * .001,
						  (double)((uint64_t)record.dt - starttime) * .001,
						  record.dt.DateToStr().str(),
						  record.address.str(),
						  record.filename.FilePart().str(),
						  record.ln,
						  record.index);
			}

			fp.printf("\n\n");
			
			fp.close();
		}
		else if (datfilename.Valid()) {
			fprintf(stderr, "Failed to open file '%s' for writing\n", datfilename.str());
		}
	}

	records.clear();
}

void MoveFiles(std::vector<AString>& filenames, const AString& olddir)
{
	size_t i;
	
	CreateDirectory(olddir);
	for (i = 0; i < filenames.size(); i++) {
		rename(filenames[i], olddir.CatPath(filenames[i].FilePart()));
	}
						
	filenames.clear();
}

int main(int argc, char *argv[])
{
	AStdFile fp;
	AList    filelist;
	AString  datfilename;
	AString  kmldir      = "kml";
	AString  olddir      = "old";
	AString  jrnfilename = "journey.kml";
	int i;

	for (i = 1; i < argc; i++) {
		if		(stricmp(argv[i], "-dat") 	  == 0) datfilename = argv[++i];
		else if (stricmp(argv[i], "-kmldir")  == 0) kmldir      = argv[++i];
		else if (stricmp(argv[i], "-olddir")  == 0) olddir      = argv[++i];
		else if (stricmp(argv[i], "-journey") == 0) jrnfilename = argv[++i];
		else CollectFiles(argv[i], "*.srt", 0, filelist);
	}

	std::vector<RECORDFILE> files;
	const AString *file = AString::Cast(filelist.First());
	while (file) {
		RECORDFILE _file;

		if (ReadFile(*file, _file, NULL)) {
			files.push_back(_file);
		}

		file = file->Next();
	}

	std::sort(files.begin(), files.end(), compare);

	{
		std::vector<RECORD>  records;
		std::vector<AString> filenames;
		size_t fn;

		if (datfilename.Valid()) remove(datfilename);

		for (fn = 0; fn < files.size(); fn++) {
			if (ReadFile(files[fn].filename, files[fn], &records)) {
				filenames.push_back(files[fn].filename);
				
				if (fn < (files.size() - 1)) {
					const RECORD& rec1 = files[fn    ].records[files[fn].records.size() - 1];
					const RECORD& rec2 = files[fn + 1].records[0];
					double   dist = CalcDistance(rec1, rec2);
					uint64_t time = (uint64_t)(rec2.dt - rec1.dt);

					if ((time >= 120000) || (dist >= 4.0)) {
						WriteRecords(kmldir, jrnfilename, datfilename, records);
						MoveFiles(filenames, olddir);
					}
				}
			}
		}

		WriteRecords(kmldir, jrnfilename, datfilename, records);
	}

	return 0;
}
