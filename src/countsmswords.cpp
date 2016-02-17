
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/strsup.h>
#include <rdlib/StdFile.h>
#include <rdlib/Recurse.h>
#include <rdlib/Hash.h>

bool Output(const char *key, uptr_t value, void *context)
{
	uint_t count = (uint_t)value;

	(void)context;

	printf("%s\t%u\ten_GB\n", key, count);
	
	return true;
}
	
int main(int argc, char *argv[])
{
	AHash words(100);
	int i;

	if (argc < 2) {
		fprintf(stderr, "Usage: countsmswords <predictions> <sms-xml-files>\n");
		exit(1);
	}
	
	words.EnableCaseInSensitive(true);

	{
		AStdFile fp;

		if (fp.open(argv[1])) {
			AString line;

			while (line.ReadLn(fp) >= 0) {
				if (line.CountWords() >= 3) {
					words.Insert(line.Word(0), 100);
				}
			}
			
			fp.close();
		}
		else fprintf(stderr, "Failed to open '%s' for reading\n", argv[1]);
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

				while (line.ReadLn(fp) >= 0) {
					AString sms = (line.GetField("body=\"", "\"")
								   .SearchAndReplace(".", "")
								   .SearchAndReplace(",", "")
								   .SearchAndReplace("!", "")
								   .SearchAndReplace("?", "")
								   .SearchAndReplace(" '", " ")
								   .SearchAndReplace("' ", " "));

					if (sms.Valid()) {
						uint_t j, n = sms.CountWords();

						for (j = 0; j < n; j++) {
							AString word = sms.Word(j);

							if (words.Exists(word)) words.Insert(word, words.Read(word) + 10);
						}
					}
				}
				
				fp.close();
			}
			else fprintf(stderr, "Failed to open '%s' for reading\n", str->str());
			
			str = str->Next();
		}
	}

	printf("MLK_UDic_Begin\n");
	words.Traverse(&Output);
	printf("MLK_UDic_End\n");
	
	return 0;
}
