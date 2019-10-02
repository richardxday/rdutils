
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

	if (argc < 2) {
		fprintf(stderr, "Usage: multilogger <file-pattern>\n");
		exit(1);
	}

	AString pattern = argv[1], datestr;
	AString str;
	while (!HasQuit() && (str.ReadLn(Stdin) >= 0)) {
		ADateTime dt = ADateTime().TimeStamp();
		AString newdatestr = dt.DateFormat("%Y-%M-%D");

		if (!fp.isopen() || (newdatestr != datestr)) {
			fp.close();

			datestr = newdatestr;
			AString filename = pattern.SearchAndReplace("{date}", datestr);

			CreateDirectory(filename.PathPart());
			if (!fp.open(filename, "a")) {
				fprintf(stderr, "Failed to open file '%s' for writing\n", filename.str());
				break;
			}

			fp.printf("%s: %s\n", dt.DateFormat("%h:%m:%s").str(), str.str());
			fp.flush();
		}
	}

	fp.close();

	return 0;
}
