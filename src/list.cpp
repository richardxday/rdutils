
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rdlib/Recurse.h>
#include <rdlib/QuitHandler.h>

/* end of includes */

typedef struct {
	AQuitHandler QuitHandler;
	AString		 FormatString;
	ADateTime    FromTime;
	ADateTime    ToTime;
	uint_t		 Index;
	bool		 bFromTimeValid, bToTimeValid;
	bool		 bIncludeFiles, bIncludeDirs;
	bool		 bAutoLineFeed;
} LIST_CONTEXT;

static double frandom()
{
	static uint32_t Seed = 0;
	static bool configured = false;

	if (!configured) {
		Seed = time(NULL) ^ 0xdeadf1d0;

		configured = true;
	}

	Seed = Seed * 1664525 + 1013904223;

	return (double)Seed / 4294967295.0;
}

static bool FindEndBrace(const AString& str, uint_t& n)
{
	bool success = false;

	assert(str[n] == '{');
	n++;

	while (str[n] && (str[n] != '}')) {
		if (str[n] == '{') {
			if (!FindEndBrace(str, n)) break;
		}
		else n++;
	}

	if (str[n] == '}') {
		n++;
		success = true;
	}

	return success;
}

static bool GetParts(const AString& str, const AString& match, AString& part1, AString& part2, AString& part3)
{
	sint_t p = 0, pos = -1;

	while ((pos = str.Pos(match, p)) >= 0) {
		if (str[pos + match.GetLength()] != '{') {
			uint_t pos1 = pos;
			
			if (FindEndBrace(str, pos1)) {
				part1 = str.Left(pos);
				part2 = str.Mid(pos + match.GetLength(), pos1 - 1 - (pos + match.GetLength()));
				part3 = str.Mid(pos1);
			}
			break;
		}
		else p = pos + 1;
	}

	return (pos >= 0);
}

//static bool GetSubPartsDebug = false;
static uint_t GetSubParts(const AString& str, const AString& tokens, AString *strs, uint_t maxstrs)
{
	sint_t p = 0;
	uint_t i, j, n = 0;

	for (i = j = 0; str[i] && (n < maxstrs); i++) {
		char c = str[i];

		for (j = n; tokens[j] && (j < maxstrs); j++) {
			if (c == tokens[j]) {
				strs[n] = str.Mid(p, i - p);

#if 0
				if (GetSubPartsDebug) {
					printf("str = '%s', tokens = '%s', i = %u, j = %u, p = %d, n = %u, strs[%u] = '%s', new p = %d, new n = %u\n", 
						   str.str(), tokens.str(), i, j, p, n, n, strs[n].str(), i + 1, j + 1);
				}
#endif

				p = i + 1;
				n = j + 1;
				break;
			}
		}
	}

	if (n < maxstrs) {
		strs[n++] = str.Mid(p);
	}

	//GetSubPartsDebug = false;

	return n;
}

static bool __List(const FILE_FIND *file, void *Context)
{
	LIST_CONTEXT *p = (LIST_CONTEXT *)Context;
	bool isdir = ((file->Attrib & FILE_FLAG_IS_DIR) != 0);

	if (((p->bIncludeFiles && !isdir) || (p->bIncludeDirs && isdir)) &&
		(!p->bFromTimeValid || (file->WriteTime >= p->FromTime)) &&
		(!p->bToTimeValid   || (file->WriteTime <= p->ToTime))) {
		AString str = p->FormatString, ostr, str2;

		do {
			ostr = str;

			str = str.SearchAndReplace("{filename}", file->FileName.FilePart());
			str = str.SearchAndReplace("{path}", file->FileName);
			str = str.SearchAndReplace("{ext}", file->FileName.Suffix());
			str = str.SearchAndReplace("{mainname}", file->FileName.FilePart().Prefix());
			str = str.SearchAndReplace("{dir}", file->FileName.PathPart());
			str2.Format("%" FMT64 "u", file->FileSize); str = str.SearchAndReplace("{size}", str2);
			str2.Format("%" FMT64 "u", (file->FileSize + 1023) / 1024); str = str.SearchAndReplace("{size-kb}", str2);
			str2.Format("%" FMT64 "u", (file->FileSize + (1024 * 1024) - 1) / (1024 * 1024)); str = str.SearchAndReplace("{size-mb}", str2);
			str2.Format("%u", file->FileName.FilePart().GetLength()); str = str.SearchAndReplace("{filename-length}", str2);
			str2.Format("%u", file->FileName.GetLength()); str = str.SearchAndReplace("{path-length}", str2);
			str2 = file->WriteTime.DateToStr(); str = str.SearchAndReplace("{date}", str2);
			str2 = file->WriteTime.DateFormat("%Y-%M-%D"); str = str.SearchAndReplace("{isodate}", str2);
			str2 = file->WriteTime.DateFormat("%h-%m-%s"); str = str.SearchAndReplace("{time}", str2);
			str2 = file->WriteTime.DateFormat("%Y-%M-%D-%h-%m-%s"); str = str.SearchAndReplace("{timestamp}", str2);
			str2 = file->WriteTime.DateFormat("%Y"); str = str.SearchAndReplace("{year}", str2);
			str2 = file->WriteTime.DateFormat("%M"); str = str.SearchAndReplace("{month}", str2);
			str2 = file->WriteTime.DateFormat("%D"); str = str.SearchAndReplace("{day}", str2);
			str2 = file->WriteTime.DateFormat("%h"); str = str.SearchAndReplace("{hour}", str2);
			str2 = file->WriteTime.DateFormat("%m"); str = str.SearchAndReplace("{minute}", str2);
			str2 = file->WriteTime.DateFormat("%s"); str = str.SearchAndReplace("{second}", str2);

			str2.Format("<a href=\"%s\">%s</a><br>", file->FileName.ForwardSlashes().str(), file->FileName.FilePart().str());
			str = str.SearchAndReplace("{html-link}", str2);

			str2.Format("<a href=\"%s\">%s</a>", file->FileName.ForwardSlashes().str(), file->FileName.FilePart().str());
			str = str.SearchAndReplace("{html-link-no-br}", str2);

			str2.Format("<p><anchor>%s<go href=\"%s\"/></anchor></p>", file->FileName.FilePart().str(), file->FileName.ForwardSlashes().str());
			str = str.SearchAndReplace("{wml-link}", str2);

			str2.Format("<anchor>%s<go href=\"%s\"/></anchor>", file->FileName.FilePart().str(), file->FileName.ForwardSlashes().str());
			str = str.SearchAndReplace("{wml-link-no-p}", str2);

			str = str.SearchAndReplace("{cr}", "\r");
			str = str.SearchAndReplace("{crlf}", "\r\n");
			str = str.SearchAndReplace("{lf}", "\n");
			str = str.SearchAndReplace("{tab}", "\t");
			str = str.SearchAndReplace("{quote}", "\'");
			str = str.SearchAndReplace("{quotes}", "\"");
			str = str.SearchAndReplace("{q}", "\"");

			AString part1, part2, part3;
			while (GetParts(str, "{index:", part1, part2, part3)) {
				AString strs[2];
				AString format;
				uint_t fs = 1, offset = 0;

				GetSubParts(part2, ":", strs, NUMBEROF(strs));
				if (strs[0].Valid()) fs     = (uint_t)strs[0];
				if (strs[1].Valid()) offset = (uint_t)strs[1];

				format.Format("%%0%uu", fs);
				str2.Format(format, offset + p->Index);

				str = part1 + str2 + part3;
			}

			while (GetParts(str, "{html-link:", part1, part2, part3)) {
				str2.Format("<a href=\"%s\">%s</a><br>", file->FileName.ForwardSlashes().str(), part2.str());
				str = part1 + str2 + part3;
			}

			while (GetParts(str, "{wml-link:", part1, part2, part3)) {
				str2.Format("<p><anchor>%s<go href=\"%s\"/></anchor></p>", part2.str(), file->FileName.ForwardSlashes().str());
				str = part1 + str2 + part3;
			}

			while (GetParts(str, "{escape:", part1, part2, part3)) {
				str = part1 + part2.Escapify() + part3;
			}

			while (GetParts(str, "{upper:", part1, part2, part3)) {
				str = part1 + part2.ToUpper() + part3;
			}

			while (GetParts(str, "{lower:", part1, part2, part3)) {
				str = part1 + part2.ToLower() + part3;
			}

			while (GetParts(str, "{filepart:", part1, part2, part3)) {
				str = part1 + part2.FilePart() + part3;
			}

			while (GetParts(str, "{pathpart:", part1, part2, part3)) {
				str = part1 + part2.PathPart() + part3;
			}

			while (GetParts(str, "{prefix:", part1, part2, part3)) {
				str = part1 + part2.Prefix() + part3;
			}

			while (GetParts(str, "{suffix:", part1, part2, part3)) {
				str = part1 + part2.Suffix() + part3;
			}

			while (GetParts(str, "{random:", part1, part2, part3)) {
				AString format;
				uint_t    fs = (uint_t)part2;
				double  range = pow(10.0, fs);

				format.Format("%%0%uu", (uint_t)part2);
				str2.Format(format, (uint_t)(range * frandom()));

				str = part1 + str2 + part3;
			}

			while (GetParts(str, "{strlen:", part1, part2, part3)) {
				str2.Format("%u", part2.GetLength());
				str = part1 + str2 + part3;
			}

			while (GetParts(str, "{add:", part1, part2, part3)) {
				AString strs[2];

				GetSubParts(part2, ":", strs, NUMBEROF(strs));
			
				str2.Format("%u", (uint_t)strs[0] + (uint_t)strs[1]);
				str = part1 + str2 + part3;
			}

			while (GetParts(str, "{sub:", part1, part2, part3)) {
				AString strs[2];

				GetSubParts(part2, ":", strs, NUMBEROF(strs));
			
				str2.Format("%u", (uint_t)strs[0] - (uint_t)strs[1]);
				str = part1 + str2 + part3;
			}

			while (GetParts(str, "{mul:", part1, part2, part3)) {
				AString strs[2];

				GetSubParts(part2, ":", strs, NUMBEROF(strs));
			
				str2.Format("%u", (uint_t)strs[0] * (uint_t)strs[1]);
				str = part1 + str2 + part3;
			}

			while (GetParts(str, "{leftstr:", part1, part2, part3)) {
				AString strs[2];

				GetSubParts(part2, ":", strs, NUMBEROF(strs));
				uint_t len = strs[0].Valid() ? (uint_t)strs[0] : 0;

				str = part1 + strs[1].Left(len) + part3;
			}

			while (GetParts(str, "{rightstr:", part1, part2, part3)) {
				AString strs[2];

				GetSubParts(part2, ":", strs, NUMBEROF(strs));
				uint_t len = strs[0].Valid() ? (uint_t)strs[0] : 0;

				str = part1 + strs[1].Right(len) + part3;
			}

			while (GetParts(str, "{midstr:", part1, part2, part3)) {
				AString strs[3];

				GetSubParts(part2, ",:", strs, NUMBEROF(strs));
				uint_t pos = strs[0].Valid() ? (uint_t)strs[0] : 0;
				uint_t len = strs[1].Valid() ? (uint_t)strs[1] : ~0;

				str = part1 + strs[2].Mid(pos, len) + part3;
			}

			static const struct {
				const char *Text;
				uint_t	   Length;
				uint_t	   Mul;
			} Fields[] = {
				{"{left:",   strlen("{left:"),   0},
				{"{centre:", strlen("{centre:"), 1},
				{"{mid:",    strlen("{mid:"),    1},
				{"{right:",  strlen("{right:"),  2},
			};
			uint_t i, j;
			for (i = 0; str[i];) {
				if (str[i] == '{') {
					uint_t i1 = i;

					if (FindEndBrace(str, i1)) {
						for (j = 0; j < NUMBEROF(Fields); j++) {
							if (CompareCaseN(str.str() + i, Fields[j].Text, Fields[j].Length) == 0) {
								uint_t k;
								uint_t fieldsize = (uint_t)(sint64_t)str.EvalNumber(i + Fields[j].Length, &k, false);

								if (str[k] == ':') {
									k++;

									assert(k <= i1);

									uint_t textsize   = i1 - 1 - k;
									uint_t fieldsize1 = MAX(fieldsize, textsize);

									str2  = str.Mid(k, textsize);
									str2  = AString(" ").Copies(((fieldsize1 - str2.GetLength()) * Fields[j].Mul) / 2) + str2;
									str2 += AString(" ").Copies(fieldsize1 - str2.GetLength());

									str2  = str2.Abbreviate(fieldsize);

									str = str.Left(i) + str2 + str.Mid(i1);
								}
								else i++;
								break;
							}
						}

						if (j == NUMBEROF(Fields)) i++;
					}
					else i++;
				}
				else i++;
			}

			while (GetParts(str, "{replace:", part1, part2, part3)) {
				AString strs[3];

				GetSubParts(part2, "::", strs, NUMBEROF(strs));

				//printf("strs=['%s', '%s', '%s']\n", strs[0].str(), strs[1].str(), strs[2].str());
				str = part1 + strs[2].SearchAndReplace(strs[0], strs[1]) + part3;
			}

			str = str.SearchAndReplace("{-", "{");
			str = str.SearchAndReplace("{open-brace}", "{");
			str = str.SearchAndReplace("{close-brace}", "}");

			//printf("[%s] [%s]\n", ostr.str(), str.str());
		} while (str != ostr);

		if (p->bAutoLineFeed) printf("%s\n", str.str());
		else				  printf("%s", str.str());

		p->Index++;
	}

	return !p->QuitHandler.HasQuit();
}

int main(int argc, char *argv[])
{
	LIST_CONTEXT context;
	AList   list;
	AString header, footer;
	AString title;
	bool html = false, wml = false;
	int i;

	if (argc < 2) {
		fprintf(stderr, "Usage: list [<options>] <file-spec> {[<options>] [<file-spec>] ...}\n");
		fprintf(stderr, "\t-r\t\t\tRecurse sub-directories\n");
		//fprintf(stderr, "\t-r+\t\t\tRecurse sub-directories\n");
		//fprintf(stderr, "\t-r-\t\t\tDon't recurse sub-directories\n");
		fprintf(stderr, "\t-H <string>\t\tSet header (output before listing)\n");
		fprintf(stderr, "\t-F <string>\t\tSet footer (output after listing):\n");
		fprintf(stderr, "\t-d\t\t\tProcess directory names\n");
		fprintf(stderr, "\t-D\t\t\tProcess ONLY directory names\n");
		fprintf(stderr, "\t-A <time>\t\tProcess files/directories AFTER <time>\n");
		fprintf(stderr, "\t-B <time>\t\tProcess files/directories BEFORE <time>\n");
		fprintf(stderr, "\t-i <index>\t\tInitial index\n");
		fprintf(stderr, "\t-html <title>\t\tOutput as HTML\n");
		fprintf(stderr, "\t-wml <title>\t\tOutput as WML\n");
		fprintf(stderr, "\t-cd <dir>\t\tChange to <dir> before processing\n");
		fprintf(stderr, "\t-lf\t\t\tDo NOT add linefeed to every line\n");
		fprintf(stderr, "\t-f <format-string>\tSet format string:\n");
		fprintf(stderr, "\t    {filename}\t\t\tFilename\n");
		fprintf(stderr, "\t    {path}\t\t\tFilename including directory\n");
		fprintf(stderr, "\t    {ext}\t\t\tFilename extension\n");
		fprintf(stderr, "\t    {mainname}\t\t\tFilename without extension\n");
		fprintf(stderr, "\t    {dir}\t\t\tDirectory\n");
		fprintf(stderr, "\t    {size}\t\t\tFile size in bytes\n");
		fprintf(stderr, "\t    {size-kb}\t\t\tFile size in kbytes\n");
		fprintf(stderr, "\t    {size-mb}\t\t\tFile size in Mbytes\n");
		fprintf(stderr, "\t    {date}\t\t\tFile date\n");
		fprintf(stderr, "\t    {left:<fs>:<text>}\t\tLeft justify text\n");
		fprintf(stderr, "\t    {mid:<fs>:<text>}\t\tCentre justify text\n");
		fprintf(stderr, "\t    {centre:<fs>:<text>}\tCentre justify text\n");
		fprintf(stderr, "\t    {right:<fs>:<text>}\t\tRight justify text\n");
		fprintf(stderr, "\t    {leftstr:<len>:<text>}\tExtract leftmost <len> characters of <text>\n");
		fprintf(stderr, "\t    {midstr:<pos>[,<len>]:<text>}\tExtract middle <len> characters of <text>, starting at <pos>\n");
		fprintf(stderr, "\t    {rightstr:<len>:<text>}\tExtract rightmost <len> characters of <text>\n");
		fprintf(stderr, "\t    {replace:<search>:<replace>:<text>}\tReplace occurances of <search> with <replace> in <text>\n");
		fprintf(stderr, "\t    {html-link}\t\t\tHTML link\n");
		fprintf(stderr, "\t    {html-link-no-br}\t\t\tHTML link (without <br>)\n");
		fprintf(stderr, "\t    {wml-link}\t\t\tWML link\n");
		fprintf(stderr, "\t    {wml-link-no-p}\t\t\tWML link (without <p>)\n");
		fprintf(stderr, "\t    {html-link:<text>}\t\tHTML link using <text> as link text\n");
		fprintf(stderr, "\t    {wml-link:<text>}\t\tWML link using <text> as link text\n");
		fprintf(stderr, "\t    {cr}\t\t\tCarriage return\n");
		fprintf(stderr, "\t    {crlf}\t\t\tCarriage return, linefeed\n");
		fprintf(stderr, "\t    {lf}\t\t\tLinefeed\n");
		fprintf(stderr, "\t    {tab}\t\t\tTab\n");
		fprintf(stderr, "\t    {quote}\t\t\tSingle quote\n");
		fprintf(stderr, "\t    {quotes}\t\t\tDouble quotes\n");
		fprintf(stderr, "\t    {q}\t\t\t\tDouble quotes\n");
		fprintf(stderr, "\t    {index:<fs>:<offset>}\tIndex\n");

		exit(20);
	}

	context.FormatString   = "{left:20:{path}} {right:10:{size-kb}k} {date}";
	context.bFromTimeValid = context.bToTimeValid = false;
	context.bIncludeFiles  = true;
	context.bIncludeDirs   = false;
	context.bAutoLineFeed  = true;
	context.Index          = 0;

	AString patterns;
	bool bSubDirs = false;
	for (i = 1; i < argc; i++) {
		if      (strcmp(argv[i], "-r")    == 0) bSubDirs = true;
		//else if (strcmp(argv[i], "-r+")   == 0) bSubDirs = true;
		//else if (strcmp(argv[i], "-r-")   == 0) bSubDirs = false;
		else if (strcmp(argv[i], "-f")    == 0) context.FormatString = AString(argv[++i]);
		else if (strcmp(argv[i], "-d")    == 0) context.bIncludeDirs  = true;
		else if (strcmp(argv[i], "-D")    == 0) {context.bIncludeFiles = false; context.bIncludeDirs  = true;}
		else if (strcmp(argv[i], "-A")    == 0) {context.FromTime.StrToDate(argv[++i]); context.bFromTimeValid = true;}
		else if (strcmp(argv[i], "-B")    == 0) {context.ToTime.StrToDate(argv[++i]);   context.bToTimeValid   = true;}
		else if (strcmp(argv[i], "-H")    == 0) header = AString(argv[++i]).DeEscapify();
		else if (strcmp(argv[i], "-F")    == 0) footer = AString(argv[++i]).DeEscapify();
		else if (strcmp(argv[i], "-html") == 0) {html = true; wml  = false; title = argv[++i];}
		else if (strcmp(argv[i], "-wml")  == 0) {wml  = true; html = false; title = argv[++i];}
		else if (strcmp(argv[i], "-cd")   == 0) {
			if (chdir(argv[++i]) != 0) {
				fprintf(stderr, "Failed to change directory to '%s'", argv[i]);
				exit(1);
			}
		}
		else if (strcmp(argv[i], "-lf")   == 0) context.bAutoLineFeed = false;
		else if (strcmp(argv[i], "-i")    == 0) context.Index = (uint_t)AString(argv[++i]);
		else {
			if (strcmp(argv[i], "--") == 0) i++;
			patterns += AString(argv[i]) + "\n";
		}
	}

	if (html) {
		printf("<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n");
		printf("<html>\n");
		printf("<head>\n");
		printf("<meta http-equiv=\"content-type\"\n");
		printf(" content=\"text/html; charset=ISO-8859-1\">\n");
		printf(" <title>%s</title>\n", title.str());
		printf("</head>\n");
		printf("<body>\n");
	}
	if (wml) {
		printf("<?xml version=\"1.0\"?>\n");
		printf("<!DOCTYPE wml PUBLIC \"-//WAPFORUM//DTD WML 1.1//EN\"\n");
		printf(" \"http://www.wapforum.org/DTD/wml_1.1.xml\">\n");
		printf("<wml>\n");
		printf("<card id=\"main\" title=\"%s\">\n", title.str());
	}
	printf("%s", header.str());

	int n = patterns.CountLines();
	for (i = 0; i < n; i++) {
		list.Delete();
		
		AString pathpattern = patterns.Line(i, "\n", 0);

		uint16_t mask = (context.bIncludeDirs ^   context.bIncludeFiles) ? FILE_FLAG_IS_DIR : 0;
		uint16_t cmp  = (context.bIncludeDirs && !context.bIncludeFiles) ? FILE_FLAG_IS_DIR : 0;
		
		::CollectFiles(pathpattern.PathPart(), pathpattern.FilePart(), RECURSE_SUBDIRS(bSubDirs), list, mask, cmp, &context.QuitHandler);
		::TraverseFiles(list, &__List, &context);
		
		if (context.QuitHandler.HasQuit()) break;
	}
	
	printf("%s", footer.str());
	if (wml)  printf("</card>\n</wml>\n");
	if (html) printf("</body>\n</html>\n");
		
	return 0;
}
