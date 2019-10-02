
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/StdFile.h>
#include <rdlib/QuitHandler.h>
#include <rdlib/DateTime.h>
#include <rdlib/Recurse.h>

int main(int argc, const char *argv[])
{
	AStdFile fp;
	int arg;

	if (argc < 2) {
		fprintf(stderr, "Usage: multilogger [{options}] <file-pattern>\n");
		fprintf(stderr, "Where {options} is one or more of:\n");
		fprintf(stderr, "\t--time\t\t\tUse time as header for log lines\n");
		fprintf(stderr, "\t--datetime\t\tUse date and time as header for log lines (ISO-8601 format)\n");
		fprintf(stderr, "\t--format <format>\tUse specified date format as header for log lines\n");
		fprintf(stderr, "\t--fileformat <format>\tUse specified date format as format for filename\n");
		exit(1);
	}

	AString dateformat;
	AString filedateformat = "%Y-%M-%D";
	for (arg = 1; arg < (argc - 1); arg++) {
		if (stricmp(argv[arg], "--time") == 0) {
			dateformat = "%h:%m:%s";
		}
		else if (stricmp(argv[arg], "--datetime") == 0) {
			dateformat = "%Y-%M-%D %h:%m:%s";
		}
		else if (stricmp(argv[arg], "--format") == 0) {
			dateformat = argv[++arg];
		}
		else if (stricmp(argv[arg], "--fileformat") == 0) {
			filedateformat = argv[++arg];
		}
		else {
			fprintf(stderr, "Unknown option '%s'\n", argv[arg]);
			exit(1);
		}
	}

	AString pattern = argv[arg];
	AString str, datestr;
	while (!HasQuit() && (str.ReadLn(Stdin) >= 0)) {
		ADateTime dt = ADateTime().TimeStamp();
		AString newdatestr = dt.DateFormat(filedateformat);

		if (!fp.isopen() || (newdatestr != datestr)) {
			fp.close();

			datestr = newdatestr;
			AString filename = pattern.SearchAndReplace("{date}", datestr);

			CreateDirectory(filename.PathPart());
			if (!fp.open(filename, "a")) {
				fprintf(stderr, "Failed to open file '%s' for writing\n", filename.str());
				break;
			}
		}

		fp.printf("%s%s%s\n", dt.DateFormat(dateformat).str(), dateformat.Valid() ? ": " : "", str.str());
		fp.flush();
	}

	fp.close();

	return 0;
}
