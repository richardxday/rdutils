
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <vector>

#include <rdlib/StdFile.h>
#include <rdlib/crc32.h>

int main(int argc, char *argv[])
{
    std::map<uint32_t, bool> map;
    int rc = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: mergelines <destination-file> {<source-file> ...}\n");
        exit(-1);
    }

    AStdFile ofp;
    if (ofp.open(argv[1], "w")) {
        for (int i = 2; i < argc; i++) {
            AStdFile fp;

            if (fp.open(argv[i])) {
                AString str;

                while (str.ReadLn(fp)) {
                    auto crc = CRC32((const uint8_t *)str.str(), (uint32_t)str.len());
                    if (map.find(crc) == map.end())
                    {
                        ofp.printf("%s\n", str.str());
                        map[crc] = true;
                    }
                }

                fp.close();
            }
            else
            {
                fprintf(stderr, "Failed to open file '%s' for reading\n", argv[i]);
                rc = -2;
            }
        }
        ofp.close();
    }
    else
    {
        fprintf(stderr, "Failed to open file '%s' for writing\n", argv[1]);
        rc = -3;
    }

    return rc;
}
