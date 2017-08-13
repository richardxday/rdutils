
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include <map>
#include <vector>

#include <rdlib/StdFile.h>
#include <rdlib/strsup.h>
#include <rdlib/Recurse.h>

class TempFile
{
public:
	TempFile() {}
	~TempFile() {
		if (filename.Valid()) {
			remove(filename);
		}
	}

	TempFile& operator = (const AString& _filename) {filename = _filename; return *this;}
	
protected:
	AString filename;
};

AString GenerateTempFilename(const AString& prefix, const AString& suffix, bool autodelete = true)
{
	static std::map<AString,TempFile> tempfiles;
	AString tempname = AString("/tmp").CatPath(prefix + AString("-%08x").Arg(GetNanosecondTicks()) + suffix);

	if (autodelete) {
		tempfiles[tempname] = tempname;
	}
	
	return tempname;
}

AString ConvertPath(const AString& path)
{
#ifdef __CYGWIN__
	return (path[1] == ':') ? AString("/cygdrive").CatPath(path.Left(1).ToLower()).CatPath(path.Mid(2)) : path;
#else
	return path;
#endif
}

bool SameFile(const AString& file1, const AString& file2)
{
	struct stat stat1, stat2;

	return ((file1 == file2) ||
			((lstat(file1, &stat1) == 0) &&
			 (lstat(file2, &stat2) == 0) &&
			 (stat1.st_dev == stat2.st_dev) &&
			 (stat1.st_ino == stat2.st_ino)));
}

bool CopyFile(AStdData& fp1, AStdData& fp2)
{
	static uint8_t buffer[65536];
	slong_t sl = -1, dl = -1;

	while ((sl = fp1.readbytes(buffer, sizeof(buffer))) > 0) {
		if ((dl = fp2.writebytes(buffer, sl)) != sl) {
			fprintf(stderr, "Failed to write %ld bytes (%ld written)\n", sl, dl);
			break;
		}
	}

	return (sl <= 0);
}

bool CopyFile(const AString& src, const AString& dst, bool binary = true)
{
	AStdFile fp1, fp2;
	bool success = false;

	if (SameFile(src, dst)) {
		printf("File copy: '%s' and '%s' are the same file\n", src.str(), dst.str());
		success = true;
	}
	else if (fp1.open(src, binary ? "rb" : "r")) {
		if (fp2.open(dst, binary ? "wb" : "w")) {
			success = CopyFile(fp1, fp2);
			if (!success) fprintf(stderr, "Failed to copy '%s' to '%s': transfer failed\n", src.str(), dst.str());
		}
		else fprintf(stderr, "Failed to copy '%s' to '%s': failed to open '%s' for writing\n", src.str(), dst.str(), dst.str());
	}
	else fprintf(stderr, "Failed to copy '%s' to '%s': failed to open '%s' for reading\n", src.str(), dst.str(), src.str());

	return success;
}

typedef struct {
	AString   name;
	AString   path;
	AString   relpath;
	AString   backuppath;
	bool      deleted;
	FILE_INFO info;
} SOURCE;

int main(int argc, char *argv[])
{
	AString dbdir;
	AString rootpath   = "C:/Users/Frank/Pictures/PictureProject/";
	AString backuppath = "E:/Backup/Images/";
	int i;
	
	if (argc < 1) {
		fprintf(stderr, "Usage: picprojectdb <db-path>\n");
		exit(1);
	}

	for (i = 1; i < argc; i++) {
		if (stricmp(argv[i], "--dbdir") == 0) {
			dbdir = argv[++i];
		}
	}

	AString cmdfile     = GenerateTempFilename("picproject-cmd",".txt");
	AString resultsfile = GenerateTempFilename("picproject-results",".csv", false);
	AString cmd;
	AStdFile fp;
	if (fp.open(cmdfile, "w")) {
		fp.printf("select Name,Path,MemberIDs from SourceTable\n");
		fp.close();

		cmd.printf("mdb-sql -i %s -p -d ',' -F %s >%s",
				   cmdfile.str(),
				   dbdir.CatPath("Source.mdb").str(),
				   resultsfile.str());
				   
		if (system(cmd) == 0) {
			if (fp.open(resultsfile)) {
				std::map<AString,uint_t> columnnametoindex;
				std::map<uint_t,AString> columnindextoname;
				AString line;
				uint_t i, n;

				while ((line.ReadLn(fp) >= 0) && line.Empty()) ;

				n = line.CountColumns();
				if (n > 0) {
					for (i = 0; i < n; i++) {
						columnnametoindex[line.Column(i)] = i;
						columnindextoname[i] = line.Column(i);
					}

					std::vector<SOURCE> files;
					uint_t nbackup = 0;
					while (line.ReadLn(fp) >= 0) {
						SOURCE item;

						item.name    = line.Column(columnnametoindex["Name"]);
						item.path    = line.Column(columnnametoindex["Path"]).ForwardSlashes();
						item.deleted = line.Column(columnnametoindex["MemberIDs"]).Valid();

						if (item.path.StartsWithNoCase(rootpath)) {
							if (item.deleted || GetFileInfo(ConvertPath(item.path), &item.info)) {
								item.relpath    = item.path.Mid(rootpath.len());
								item.backuppath = backuppath.CatPath(item.relpath);
								
								files.push_back(item);

								nbackup += item.deleted ? 0 : 1;
							}
							else printf("Warning: '%s' does not exist\n", item.path.str());
						}
					}

					printf("%u files found (%u to backup)\n", (uint_t)files.size(), nbackup);

					uint_t pc = ~0;
					for (i = n = 0; i < (uint_t)files.size(); i++) {
						const SOURCE& item = files[i];
						FILE_INFO info;
						
						if (!item.deleted &&
							(!GetFileInfo(ConvertPath(item.backuppath), &info) ||
							 (info.FileSize != item.info.FileSize))) {
							printf("\r%s -> %s", item.path.str(), item.backuppath.str());
							fflush(stdout);

							AString dir = item.path.PathPart();
							if (!CreateDirectory(ConvertPath(dir))) {
								fprintf(stderr, "Failed to create directory '%s'\n", dir.str());
							}
							
							bool success = CopyFile(ConvertPath(item.path), ConvertPath(item.backuppath));
							printf("\r%s -> %s ... %s\n", item.path.str(), item.backuppath.str(), success ? "done" : "**ERROR**");

							n++;

							uint_t pc1 = (n * 100) / nbackup;
							if (pc1 != pc) {
								pc = pc1;
								printf("\rProcessed %u of %u (%u%%) done", n, nbackup, pc);
								fflush(stdout);
							}
						}
					}

					printf("\n");
				}
				
				fp.close();
			}
			else fprintf(stderr, "Failed to open results file '%s'\n", resultsfile.str());
		}
		else fprintf(stderr, "Command '%s' failed\n", cmd.str());
	}
	else fprintf(stderr, "Unable to create command file '%s'\n", cmdfile.str());

	return 0;
}
