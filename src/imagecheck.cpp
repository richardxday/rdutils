
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rdlib/StdFile.h>
#include <rdlib/strsup.h>
#include <rdlib/exif.h>
#include <rdlib/BMPImage.h>

int main(int argc, char *argv[])
{
	int i;

	for (i = 1; i < argc; i++) {
		AStdFile fp;

		if (fp.open(argv[i])) {
			Exif exif;
			
			if (exif.DecodeExif(fp.GetFile())) {
				const EXIFINFO *exifinfo = exif.m_exifinfo;

				if (exifinfo && (exifinfo->CameraMake[0] || exifinfo->CameraModel[0])) {
					AString cameramake(exifinfo->CameraMake, sizeof(exifinfo->CameraMake));
					AString cameramodel(exifinfo->CameraModel, sizeof(exifinfo->CameraModel));

					printf("%s: '%s' / '%s'\n", argv[i], cameramake.str(), cameramodel.str());
				}
			}			
			
			fp.close();
		}
	}

	return 0;
}
