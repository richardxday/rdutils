
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/strsup.h>
#include <rdlib/Recurse.h>
#include <rdlib/Regex.h>
#include <rdlib/QuitHandler.h>

/* end of includes */

AQuitHandler QuitHandler;

typedef struct {
	AString Pattern;
	AString ExpandString;
	AString	PreMatch, PostMatch, PostLine, PostFile;
	AString NoMatch;
	bool    RegEx;
	bool    CaseInSensitive;
	bool    Multiple;
	bool    ShowFilename, ForceShowFilename;
	bool    ShowLineNumbers, ForceShowLineNumbers;
	bool    ShowFilePath;
	bool	ShowFilenameOnly;
	bool	ShowSearchDirs;
	bool	ShowSearchFiles;
	bool	DisplayNonMatching;
	bool	StopAfterFirstMatch;
	bool    ForcePostFile;
	char	EscapeChar;
	uint_t	LineMatches;
	uint_t	FileMatches;
	uint_t	MaxLines;
	uint_t	ContextLines;
} GREP_CONTEXT;

static bool readln(FILE *fp, AString& lines, uint_t& n, uint_t nlines)
{
	AString line;

	if (n > 0) {
		lines.CutLine(0, "\n", 0);
		n--;
	}
	while ((n < nlines) && (line.ReadLn(fp) >= 0) && !HasQuit()) {
		if (!n) lines  = line;
		else	lines += "\n" + line;
		n++;
	}

	return ((n > 0) && !HasQuit());
}

static void outputline(GREP_CONTEXT *p, const AString& header, const AString& str, bool force = false)
{
	static AString output;

	if (str.Valid()) {
		if (output.Empty()) output = header;
		output += p->PreMatch + str + p->PostMatch;
	}

	sint_t pos;
	while ((pos = output.PositionOfLine(1, "\n", 0)) < output.len()) {
		printf("%s%s", output.Line(0, "\n", 0).str(), p->PostLine.str());
		fflush(stdout);

		output = header + output.Mid(pos);
	}

	if (force && output.Valid()) {
		printf("%s%s", output.Line(0, "\n", 0).str(), p->PostLine.str());
		fflush(stdout);
		output.Delete();
	}
}

static AString generateheader(const GREP_CONTEXT *p, bool bIsFile, const char *filename, uint_t ln)
{
	bool bShowFilename    = (p->ShowFilename    && (p->ForceShowFilename    || bIsFile));
	bool bShowLineNumbers = (p->ShowLineNumbers && (p->ForceShowLineNumbers || bIsFile));
	AString header;

	if (bShowFilename && bShowLineNumbers) header.Format("%s:%u:", filename, ln);
	else if (bShowFilename)				   header.Format("%s:", filename);
	else if (bShowLineNumbers)             header.Format("%u:", ln);

	return header;
}

static void GrepFile(const char *filename, FILE *fp, GREP_CONTEXT *p)
{
	const AString& Pattern = p->Pattern;
	AString lines, header;
	bool bIsFile = (fp != stdin);
	bool bRegEx = p->RegEx, bCase = !p->CaseInSensitive, bNonMatching = p->DisplayNonMatching;
	bool bShowFilenameOnly = p->ShowFilenameOnly, bStopAfterFirstMatch = p->StopAfterFirstMatch;
	bool matched = false;
	char EscapeChar = p->EscapeChar;
	uint_t ln = 1, n = 0, MaxLines = p->MaxLines, ContextLines = p->ContextLines, i;

	while (readln(fp, lines, n, ContextLines)) {
		if ((( bRegEx && MatchRegex(lines, Pattern, bCase, EscapeChar)) ||
			 (!bRegEx &&  bCase && (lines.Pos(Pattern) >= 0)) ||
			 (!bRegEx && !bCase && (lines.PosNoCase(Pattern) >= 0))) != bNonMatching) {
			p->LineMatches++;
			matched = true;

			if (bShowFilenameOnly) {
				printf("%s\n", filename);
				fflush(stdout);
				break;
			}
			else {
				for (i = 0; i < ContextLines; i++) {
					header = generateheader(p, bIsFile, filename, ln + i);
					
					outputline(p, header, lines.Line(i, "\n", 0));
				}
			}
		}

		ln++;
		if (ln > MaxLines) break;

		if (bStopAfterFirstMatch && matched) break;
	}

	if (matched) p->FileMatches++;

	header = generateheader(p, bIsFile, filename, ln);
	
	outputline(p, header, "", true);

	if (matched || p->ForcePostFile) {
		if (!matched) printf("%s", p->NoMatch.str());
		printf("%s", p->PostFile.str());
		fflush(stdout);
	}
}

static bool __Grep(const FILE_FIND *File, void *Context)
{
	GREP_CONTEXT *p = (GREP_CONTEXT *)Context;
	FILE *fp;

	if (!(File->Attrib & FILE_FLAG_IS_DIR)) {
		if (p->ShowSearchFiles) {
			printf("Searching File %s...\n", (const char *)File->FileName);
			fflush(stdout);
		}

		if ((fp = fopen(File->FileName, "r")) != NULL) {
			GrepFile(p->ShowFilePath ? File->FileName : File->FileName.FilePart(), fp, p);
			fclose(fp);
		}
		else fprintf(stderr, "Failed to open file %s\n", (const char *)File->FileName);
	}
	else if (p->ShowSearchDirs) {
		printf("Searching Dir %s...\n", (const char *)File->FileName);
	}

	return !HasQuit();
}

static void GrepExpandFile(const char *filename, FILE *fp, GREP_CONTEXT *p)
{
	const AString& Pattern      = p->Pattern;
	const AString& ExpandString = p->ExpandString;
	ADataList Regions;
	AString lines, header;
	bool bIsFile = (fp != stdin);
	bool bCase = !p->CaseInSensitive, bNonMatching = p->DisplayNonMatching, bMultiple = p->Multiple;
	bool bShowFilenameOnly = p->ShowFilenameOnly, bStopAfterFirstMatch = p->StopAfterFirstMatch;
	bool matched = false;
	char EscapeChar = p->EscapeChar;
	uint_t ln = 1, n = 0, MaxLines = p->MaxLines, ContextLines = p->ContextLines;

	while (readln(fp, lines, n, ContextLines)) {
		header = generateheader(p, bIsFile, filename, ln);

		AString lines1 = lines;
		while (lines1.Valid()) {
			Regions.DeleteList();

			if (MatchRegex(lines1, Pattern, Regions, bCase, EscapeChar) != bNonMatching) {
				p->LineMatches++;
				matched = true;

				if (bShowFilenameOnly) {
					printf("%s\n", filename);
					fflush(stdout);
					break;
				}
				else {
					outputline(p, header, ExpandRegexRegions(lines1, ExpandString, Regions));

					if (bMultiple && (Regions.Count() > 0)) {
						uint_t i, pos = 0;
						for (i = 0; i < Regions.Count(); i++) {
							const REGEXREGION *reg = (const REGEXREGION *)Regions[i];
							pos = MAX(pos, reg->pos + reg->len);
						}
						if (pos > 0) lines1 = lines1.Mid(pos);
						else break;
					}
					else break;
				}
			}
			else break;
		}

		Regions.DeleteList();

		ln++;
		if (ln > MaxLines) break;

		if (bStopAfterFirstMatch && matched) break;
	}

	if (matched) p->FileMatches++;

	header = generateheader(p, bIsFile, filename, ln);
	
	outputline(p, header, "", true);

	if (matched || p->ForcePostFile) {
		if (!matched) printf("%s", p->NoMatch.str());
		printf("%s", p->PostFile.str());
		fflush(stdout);
	}
}

static bool __GrepExpand(const FILE_FIND *File, void *Context)
{
	GREP_CONTEXT *p = (GREP_CONTEXT *)Context;
	FILE *fp;

	if (!(File->Attrib & FILE_FLAG_IS_DIR)) {
		if (p->ShowSearchFiles) {
			printf("Searching File %s...\n", (const char *)File->FileName);
		}

		if ((fp = fopen(File->FileName, "r")) != NULL) {
			GrepExpandFile(p->ShowFilePath ? File->FileName : File->FileName.FilePart(), fp, p);
			fclose(fp);
		}
		else fprintf(stderr, "Failed to open file %s\n", (const char *)File->FileName);
	}
	else if (p->ShowSearchDirs) {
		printf("Searching Dir %s...\n", (const char *)File->FileName);
	}

	return !HasQuit();
}

bool parselogic(const char *arg, const char *str, bool& flag)
{
	bool found = false;
	uint_t l = strlen(str);

	if (strncmp(arg, str, l) == 0) {
		switch (arg[l]) {
			case 0:
			case '+':
				flag  = true;
				found = true;
				break;

			case '-':
				flag  = false;
				found = true;
				break;

			default:
				break;
		}
	}

	return found;
}

int main(int argc, char *argv[])
{
	GREP_CONTEXT Context;
	int i, res = 0;
	bool bMatchBeginning = false;
	bool bMatchEnd = false;
	bool bSubDirs  = false;
	bool bExpand   = false;
	bool bReturnLineMatches = false;
	bool bReturnFileMatches = false;
	
	Context.PreMatch				= "";
	Context.PostMatch				= "\n";
	Context.PostLine				= "\n";
	Context.RegEx           		= true;
	Context.CaseInSensitive 		= false;
	Context.Multiple        		= false;
	Context.ShowFilename    		= true;
	Context.ForceShowFilename  		= false;
	Context.ShowFilePath    		= true;
	Context.ShowLineNumbers 		= true;
	Context.ForceShowLineNumbers	= false;
	Context.ShowFilenameOnly 		= false;
	Context.ShowSearchDirs  		= false;
	Context.ShowSearchFiles 		= false;
	Context.DisplayNonMatching	    = false;
	Context.StopAfterFirstMatch	    = false;
	Context.ForcePostFile           = false;
	Context.EscapeChar				= '\\';
	Context.LineMatches				= 0;
	Context.FileMatches				= 0;
	Context.MaxLines				= MAX_UNSIGNED(uint_t);
	Context.ContextLines			= 1;
	bSubDirs                	    = true;

	if (argc < 3) {
		fprintf(stderr, "agrep [<options>] <pattern> <files...>|-\n");
		fprintf(stderr, "Options (enabled by '+', disabled by '-'):\n");
		fprintf(stderr, "\t-d\t\tRecurse sub-directories (default: %s)\n", bSubDirs ? "On" : "Off");
		fprintf(stderr, "\t-r\t\tPattern is regex (default: %s)\n", Context.RegEx ? "On" : "Off");
		fprintf(stderr, "\t-i\t\tIgnore case (default: %s)\n", Context.CaseInSensitive ? "On" : "Off");
		fprintf(stderr, "\t-l\t\tShow only filename of matching files (default: %s)\n", Context.ShowFilenameOnly ? "On" : "Off");
		fprintf(stderr, "\t-b\t\tMatch beginning of line (default: %s)\n", bMatchBeginning ? "On" : "Off");
		fprintf(stderr, "\t-e\t\tMatch end of line (default: %s)\n", bMatchEnd ? "On" : "Off");
		fprintf(stderr, "\t-f\t\tShow filename (default: %s)\n", Context.ShowFilename ? "On" : "Off");
		fprintf(stderr, "\t-n\t\tShow line numbers (default: %s)\n", Context.ShowLineNumbers ? "On" : "Off");
		fprintf(stderr, "\t-o\t\tShow filename and line numbers (default: %s)\n", (Context.ShowFilename && Context.ShowLineNumbers) ? "On" : "Off");
		fprintf(stderr, "\t-p\t\tShow file path (default: %s)\n", Context.ShowFilePath ? "On" : "Off");
		fprintf(stderr, "\t-D\t\tShow directories that are searched (default: %s)\n", Context.ShowSearchDirs ? "On" : "Off");
		fprintf(stderr, "\t-F\t\tShow files that are searched (default: %s)\n", Context.ShowSearchFiles ? "On" : "Off");
		fprintf(stderr, "\t-x <string>\tExpand regions with <string> (default: %s)\n", bExpand ? "On" : "Off");
		fprintf(stderr, "\t-m\t\tMatch multiple times for expand (default: %s)\n", Context.Multiple ? "On" : "Off");
		fprintf(stderr, "\t-P <string>\tPre-display string (default: '%s')\n", Context.PreMatch.Escapify().str());
		fprintf(stderr, "\t-T <string>\tPost-match string (default: '%s')\n", Context.PostMatch.Escapify().str());
		fprintf(stderr, "\t-TL <string>\tPost-line string (default: '%s')\n", Context.PostLine.Escapify().str());
		fprintf(stderr, "\t-TF <string>\tPost-file string (default: '%s')\n", Context.PostFile.Escapify().str());
		fprintf(stderr, "\t-TFF <string>\tForced Post-file string (default: '%s', %s)\n", Context.PostFile.Escapify().str(), Context.ForcePostFile ? "On" : "Off");
		fprintf(stderr, "\t-N\t\tDisplay NON-MATCHING lines (default: %s)\n", Context.DisplayNonMatching ? "On" : "Off");
		fprintf(stderr, "\t-E <char>\tSet <char> as escape character (default: %c)\n", Context.EscapeChar);
		fprintf(stderr, "\t-RL\t\tReturn number of total lines matched (default: %s)\n", bReturnLineMatches ? "On" : "Off");
		fprintf(stderr, "\t-RF\t\tReturn number of total files matched (default: %s)\n", bReturnFileMatches ? "On" : "Off");
		fprintf(stderr, "\t-M <lines>\tMaximum lines to check in each file (default: %u)\n", Context.MaxLines);
		fprintf(stderr, "\t-C <lines>\tNumber of context lines to test (default: %u)\n", Context.ContextLines);
		fprintf(stderr, "\t-S\t\tStop after first match (default: %s)\n", Context.StopAfterFirstMatch ? "On" : "Off");
		fprintf(stderr, "\t--\t\tMark end of options\n");
		exit(20);
	}

#if 0
	for (i = 1; (i < argc); i++) {
		printf("argv[%d] = '%s'\n", i, argv[i]);
	}
#endif

	for (i = 1; (i < argc) && (argv[i][0] == '-'); i++) {
		if		(parselogic(argv[i], "-i", Context.CaseInSensitive)) ;
		else if (parselogic(argv[i], "-d", bSubDirs)) ;
		else if (parselogic(argv[i], "-r", Context.RegEx)) ;
		else if (parselogic(argv[i], "-l", Context.ShowFilenameOnly)) ;
		else if (parselogic(argv[i], "-b", bMatchBeginning)) ;
		else if (parselogic(argv[i], "-e", bMatchEnd)) ;
		else if (parselogic(argv[i], "-f", Context.ShowFilename)) Context.ForceShowFilename |= Context.ShowFilename;
		else if (parselogic(argv[i], "-p", Context.ShowFilePath)) ;
		else if (parselogic(argv[i], "-n", Context.ShowLineNumbers)) Context.ForceShowLineNumbers |= Context.ShowLineNumbers;
		else if (parselogic(argv[i], "-o", Context.ShowFilename)) {
			Context.ShowLineNumbers = Context.ShowFilename;
			Context.ForceShowFilename    |= Context.ShowFilename;
			Context.ForceShowLineNumbers |= Context.ShowFilename;
		}
		else if (strcmp(argv[i], "-x") == 0) {
			Context.RegEx		 = true;
			Context.ExpandString = argv[++i];
			bExpand = true;
		}
		else if (parselogic(argv[i], "-m", Context.Multiple)) ;
		else if (parselogic(argv[i], "-D", Context.ShowSearchDirs)) ;
		else if (parselogic(argv[i], "-F", Context.ShowSearchFiles)) ;
		else if (strcmp(argv[i], "-P")   == 0) Context.PreMatch = AString(argv[++i]).DeEscapify();
		else if (strcmp(argv[i], "-TL")  == 0) Context.PostLine = AString(argv[++i]).DeEscapify();
		else if (strcmp(argv[i], "-TF")  == 0) Context.PostFile = AString(argv[++i]).DeEscapify();
		else if (strcmp(argv[i], "-TFF") == 0) {
			Context.ForcePostFile = true;
			Context.PostFile = AString(argv[++i]).DeEscapify();
		}
		else if (strcmp(argv[i], "-T")   == 0) Context.PostMatch = AString(argv[++i]).DeEscapify();
		else if (parselogic(argv[i], "-N", Context.DisplayNonMatching)) ;
		else if (strcmp(argv[i], "-E")   == 0) Context.EscapeChar = AString(argv[++i]).DeEscapify().FirstChar();
		else if (parselogic(argv[i], "-L", bReturnLineMatches)) ;
		else if (parselogic(argv[i], "-S", Context.StopAfterFirstMatch)) ;
		else if (strcmp(argv[i], "-M")   == 0) Context.MaxLines = (uint_t)AString(argv[++i]);
		else if (strcmp(argv[i], "-C")   == 0) Context.ContextLines = (uint_t)AString(argv[++i]);
		else if (strcmp(argv[i], "--")   == 0) {
			i++;
			break;
		}
		else {
			fprintf(stderr, "Unknown option '%s'\n", argv[i]);
			break;
		}
	}

	if (i < argc) {
		if (Context.RegEx) {
			AString errors;

			if (!bMatchBeginning) Context.Pattern += "*(";
			else				  Context.Pattern += "(";
			Context.Pattern += argv[i++];
			if (!bMatchEnd)       Context.Pattern += ")$*";
			else				  Context.Pattern += ")";

			Context.Pattern = ParseRegex(Context.Pattern, errors, Context.EscapeChar);
			if (errors.Valid()) {
				fprintf(stderr, "Error(s) found during regex parsing:%c%s", (errors.CountLines() == 1) ? ' ' : '\n', errors.str());
				exit(-1);
			}
		}
		else Context.Pattern = argv[i++];
	}

	if (bExpand) {
		for (; (i < argc) && !HasQuit(); i++) {
			if (CompareNoCase(argv[i], "-") == 0) GrepExpandFile("stdin", stdin, &Context);
			else								  Recurse(AString(argv[i]), RECURSE_SUBDIRS(bSubDirs), &__GrepExpand, &Context);
		}
	}
	else {
		for (; (i < argc) && !HasQuit(); i++) {
			if (CompareNoCase(argv[i], "-") == 0) GrepFile("stdin", stdin, &Context);
			else								  Recurse(AString(argv[i]), RECURSE_SUBDIRS(bSubDirs), &__Grep, &Context);
		}
	}

	if (bReturnLineMatches) res = Context.LineMatches;
	if (bReturnFileMatches) res = Context.FileMatches;

	return res;
}
