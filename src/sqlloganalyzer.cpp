
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/strsup.h>
#include <rdlib/StdFile.h>

int main(int argc, char *argv[])
{
    AStdFile fp;
    int i;

    for (i = 1; i < argc; i++) {
        if (fp.open(argv[i])) {
            AString line;
            AString newreceiver;
            AString newdevice;
            AString deletedevice;
            uint_t  ln = 1;

            while (line.ReadLn(fp) >= 0) {
                if (line.StartsWith("  insert into Devices ")) {
                    newdevice = line.Words(0);
                }
                else if (newdevice.Valid() && line.StartsWith("  ")) {
                    newdevice += " " + line.Words(0);
                }
                else if (newdevice.Valid()) {
                    AString receiver = newdevice.GetField("URI=E'", "'");
                    AString device   = newdevice.GetField("receiver.ID,", ",''");
                    AString devindex = device.Column(0);
                    AString devtype  = device.Column(1);
                    AString serno    = device.Column(2);

                    printf("%s:%u: New device %s:%s:%s receiver %s\n", argv[i], ln, devindex.str(), devtype.str(), serno.str(), receiver.str());

                    newdevice.Delete();
                }

                if (line.StartsWith("  insert into Receivers ")) {
                    newreceiver = line.Words(0);
                }
                else if (newreceiver.Valid() && line.StartsWith("  ")) {
                    newreceiver += " " + line.Words(0);
                }
                else if (newreceiver.Valid()) {
                    AString receiver = newreceiver.GetField("values (E'", "'");

                    printf("%s:%u: New receiver %s\n", argv[i], ln, receiver.str());

                    newreceiver.Delete();
                }

                if (line == "  update Devices set Valid=0") {
                    deletedevice = line.Words(0);
                }
                else if (deletedevice.Valid() && line.StartsWith("  ")) {
                    deletedevice += " " + line.Words(0);
                }
                else if (deletedevice.Valid()) {
                    AString receiver    = deletedevice.GetField("URI=E'", "'");
                    AString devindex    = deletedevice.GetField("DeviceIndex=", " ");
                    AString notdevindex = deletedevice.GetField("DeviceIndex<>", ")");
                    AString devtype     = deletedevice.GetField("DeviceType=", " ");
                    AString serno       = deletedevice.GetField("SerialNumber=", " ");

                    if (devindex.Valid())    printf("%s:%u: Delete device %s receiver %s\n", argv[i], ln, devindex.str(), receiver.str());
                    if (notdevindex.Valid()) printf("%s:%u: Delete device %s:%s receiver<>%s devindex<>%s\n", argv[i], ln, devtype.str(), serno.str(), receiver.str(), notdevindex.str());

                    deletedevice.Delete();
                }

                ln++;
            }

            fp.close();
        }
    }

    return 0;
}
