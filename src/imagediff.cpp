
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include <rdlib/strsup.h>
#include <rdlib/DataList.h>
#include <rdlib/BMPImage.h>
#include <rdlib/DateTime.h>
#include <rdlib/jpeginfo.h>
#include <rdlib/SettingsHandler.h>
#include <rdlib/QuitHandler.h>

typedef struct {
	AString	filename;
	AImage	image;
	ARect	rect;
} IMAGE;

static void __DeleteImage(uptr_t item, void *context)
{
	UNUSED(context);

	delete (IMAGE *)item;
}

IMAGE *CreateImage(const char *filename, const IMAGE *img0)
{
	IMAGE *img = NULL;

	if ((img = new IMAGE) != NULL) {
		JPEG_INFO info;
		AImage& image = img->image;

		if (ReadJPEGInfo(filename, info) && image.LoadJPEG(filename)) {
			img->filename = filename;
			img->rect     = image.GetRect();
			
			if (img0 && (img->rect != img0->rect)) {
				fprintf(stderr, "Image in file '%s' is %d x %d and not %d x %d\n", filename, img->rect.w, img->rect.h, img0->rect.w, img0->rect.h);
				delete img;
				img = NULL;
			}
		}
		else {
			fprintf(stderr, "Failed to load image from file '%s'\n", filename);
			remove(filename);
			delete img;
			img = NULL;
		}
	}

	return img;
}

int main(int argc, char *argv[])
{
	AQuitHandler     quithandler;
	ASettingsHandler stats("imagediff", 5000);
	ADataList imglist;
	IMAGE     *img;
	double    diffavg 	= (double)stats.Get("avg", "0.0");
	double    diffsd  	= (double)stats.Get("sd", "0.0");
	double    coeff     = (double)stats.Get("coeff", "1.0e-3");
	double    factor    = (double)stats.Get("factor", "2.0");
	double    threshold = (double)stats.Get("threshold", "4000.0");
	int    	  i;

	imglist.SetDestructor(&__DeleteImage);

	for (i = 1; i < argc; i++) {
		stats.CheckWrite();

		if (quithandler.HasQuit()) break;

		if (argv[i][0] == '-') {
		}
		else if ((img = CreateImage(argv[i], (const IMAGE *)imglist[0])) != NULL) {
			imglist.Add(img);
			
			if (imglist.Count() == 2) {
				IMAGE *img1 = (IMAGE *)imglist[0];
				IMAGE *img2 = (IMAGE *)imglist[1];
				const AImage::PIXEL *pix1 = img1->image.GetPixelData();
				const AImage::PIXEL *pix2 = img2->image.GetPixelData();
				const ARect& rect = img1->rect;
				std::vector<float> data;
				float *ptr, *p;
				double avg[3];
				uint_t x, y, w = rect.w, h = rect.h, w3 = w * 3, len = w * h, len3 = w3 * h, c;
				
				data.resize(len3);
				ptr = &data[0];

				memset(avg, 0, sizeof(avg));

				for (y = 0, p = ptr; y < h; y++) {
					double yavg[3];

					memset(yavg, 0, sizeof(yavg));

					for (x = 0; x < w; x++, p += 3, pix1++, pix2++) {
						p[0] 	 = (float)pix1->r - (float)pix2->r;
						p[1] 	 = (float)pix1->g - (float)pix2->g;
						p[2] 	 = (float)pix1->b - (float)pix2->b;

						yavg[0] += p[0];
						yavg[1] += p[1];
						yavg[2] += p[2];
					}

					yavg[0] /= (double)w;
					yavg[1] /= (double)w;
					yavg[2] /= (double)w;

					p -= 3 * w;

					for (x = 0; x < w; x++, p += 3) {
						p[0] -= yavg[0]; avg[0] += p[0];
						p[1] -= yavg[1]; avg[1] += p[1];
						p[2] -= yavg[2]; avg[2] += p[2];
					}
				}

				avg[0] /= (double)len;
				avg[1] /= (double)len;
				avg[2] /= (double)len;

				float *p2;
				double avg2 = 0.0, sd2 = 0.0;
				for (y = 0, p = ptr, p2 = ptr; y < h; y++) {
					for (x = 0; x < w; x++, p += 3, p2++) {
						p[0] -= avg[0];
						p[1] -= avg[1];
						p[2] -= avg[2];
						p2[0] = sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
						avg2 += p2[0];
						sd2  += p2[0] * p2[0];
					}
				}

				avg2 /= (double)len;
				sd2   = sqrt(sd2 / (double)len - avg2 * avg2);

				Interpolate(diffavg, avg2, coeff);
				Interpolate(diffsd,  sd2, coeff);

				double diff = diffavg + diffsd * factor, total = 0.0;
				for (x = 0; x < len; x++) {
					ptr[x] = MAX(ptr[x] - diff, 0.0);
					total += ptr[x];
				}

				total /= 255.0;

				printf("For %s, total = %0.1lf\n", img1->filename.str(), total);

				if (total >= threshold) {
					AImage img;
					if (img.Create(rect.w, rect.h)) {
						const AImage::PIXEL *opixel = img1->image.GetPixelData();
						AImage::PIXEL *pixel = img.GetPixelData();

						for (x = 0; x < len; x++, pixel++, opixel++) {
							pixel->r = (uint8_t)LIMIT((double)opixel->r * ptr[x] / 255.0, 0.0, 255.0);
							pixel->g = (uint8_t)LIMIT((double)opixel->g * ptr[x] / 255.0, 0.0, 255.0);
							pixel->b = (uint8_t)LIMIT((double)opixel->b * ptr[x] / 255.0, 0.0, 255.0);
						}

						const TAG tags[] = {
							{AImage::TAG_JPEG_QUALITY, 95},
							{TAG_DONE},
						};
						AString filename = "Processed-" + img2->filename.FilePart();
						fprintf(stderr, "Saving detection image in '%s'\n", filename.str());
						img.SaveJPEG(filename, tags);
					}
				}
				else remove(img1->filename);

				delete img1;
				imglist.Pop();
			}
		}
	}

	stats.Set("avg", AString("%0.16le").Arg(diffavg));
	stats.Set("sd",  AString("%0.16le").Arg(diffsd));

	return 0;
}
