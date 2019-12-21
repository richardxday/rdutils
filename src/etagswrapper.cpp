
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <string>
#include <vector>

int main(int argc, char *argv[])
{
    std::vector<std::string> filelist;
    std::string cmd = "etags";
    std::string files;
    FILE *ifp, *ofp;
    int i, j;

    for (j = argc; j > 0;) {
        if ((ifp = fopen(argv[--j], "r")) != NULL) {
            std::string filename = argv[j];
            std::string filename2;
            size_t p;

            if ((p = filename.rfind(".")) != std::string::npos) {
                filename2 = filename.substr(0, p) + std::string(".tmp") + filename.substr(p);
            }
            else filename2 = filename + ".tmp";

            if ((ofp = fopen(filename2.c_str(), "wb")) != NULL) {
                std::vector<uint8_t> buffer(4096);
                size_t n;

                while ((n = fread(&buffer[0], 1, buffer.size(), ifp)) > 0) {
                    size_t k;

                    for (k = 0; k < n; k++) {
                        if (buffer[k] == '\r') {
                            buffer.erase(buffer.begin() + k);
                            n--;
                        }
                    }

                    fwrite(&buffer[0], 1, n, ofp);
                }

                fclose(ofp);
            }

            fclose(ifp);

            filelist.push_back(filename2);
            files += std::string(" \"") + filename2 + std::string("\"");
        }
        else break;
    }

    for (i = 1; i <= j; i++) {
        cmd += std::string(" ") + std::string(argv[i]);
    }

    cmd += files;

    int rc = system(cmd.c_str());

    size_t k;
    for (k = 0; k < filelist.size(); k++) {
        remove(filelist[k].c_str());
    }

    return rc;
}
