
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>

#include <rdlib/StdFile.h>
#include <rdlib/Recurse.h>
#include <rdlib/Regex.h>

const uint32_t initvalue = 250;
const uint32_t incvalue  = 100;
const uint32_t minvalue  = initvalue + 2 * incvalue;

void extractwords(const AString& text, std::map<AString,uint32_t>& words, const std::map<AString,bool>& wordlist)
{
	int j, j0 = -1;

	for (j = 0; j <= text.len(); j++) {
		char c = text[j];

		if ((j0 < 0) && IsAlphaChar(c)) j0 = j;
		else if ((j0 >= 0) &&
				 !IsAlphaChar(c) &&
				 !(IsAlphaChar(text[j + 1]) &&
				   ((c == '\'') ||
					(c == '-')))) {
			AString word = text.Mid(j0, j - j0);

			if (word.len() > 1) {
				if (( word == word.ToUpper()) ||
					((word == word.ToLower().InitialCapitalCase()) &&
					 (wordlist.find(word)           == wordlist.end()) &&
					 (wordlist.find(word.ToLower()) != wordlist.end()))) {
					word = word.ToLower();
				}
				if (words[word] == 0) words[word]  = initvalue;
				else				  words[word] += incvalue;
			}

			j0 = -1;
		}
	}
}

int main(int argc, char *argv[])
{
	std::map<AString,bool> msglist;
	std::map<AString,bool> wordlist;
	std::map<AString,uint32_t> words;
	int i;

	if (argc < 2) {
		fprintf(stderr, "Usage: countsmswords <word-list> <files>...\n");
		exit(1);
	}

	{
		AStdFile fp;

		if (fp.open(argv[1])) {
			AString line;

			fprintf(stderr, "Processing '%s'...\n", argv[1]);

			while (line.ReadLn(fp) >= 0) {
				wordlist[line] = true;
			}

			fp.close();
		}
	}

	for (i = 2; i < argc; i++) {
		AString pattern = argv[i];
		AList   list;

		CollectFiles(pattern.PathPart(), pattern.FilePart(), 0, list);

		const AString *str = AString::Cast(list.First());
		while (str) {
			AStdFile fp;

			if (fp.open(*str)) {
				AString line;

				fprintf(stderr, "Processing '%s'...\n", str->str());

				while (line.ReadLn(fp) >= 0) {
					AString text, date;

					if (str->Suffix() == "xml") {
						if ((line.Pos("<sms ") >= 0) &&
							(line.Pos(" type=\"2\" ") >= 0) &&
							((date = line.GetField("date=\"", "\"")).Valid()) &&
							((text = line.GetField("body=\"", "\"")).Valid() ||
							 (text = line.GetField("body='", "'")).Valid())) {
							if (msglist.find(date + ":" + text) == msglist.end()) {
								msglist[date + ":" + text] = true;
								extractwords(text, words, wordlist);
							}
						}
					}
					else if (line.Pos("::") >= 0) {
						words[line.Line(0, "::", 0)] += (uint32_t)line.Line(1, "::", 0);
					}
					else if ((line != "MLK_UDic_Begin") &&
							 (line != "MLK_UDic_End")) {
						extractwords(line, words, wordlist);
					}
				}

				fp.close();
			}
			else fprintf(stderr, "Failed to open '%s' for reading\n", str->str());

			str = str->Next();
		}
	}

	printf("MLK_UDic_Begin\n");
	std::map<AString,uint32_t>::iterator it;
	for (it = words.begin(); it != words.end(); ++it) {
		if (it->second >= minvalue) {
			printf("%s::%u::en\n", it->first.str(), it->second);
		}
	}
	printf("MLK_UDic_End\n");

	return 0;
}
