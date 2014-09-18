
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include <rdlib/strsup.h>
#include <rdlib/DataList.h>
#include <rdlib/BMPImage.h>
#include <rdlib/DateTime.h>

typedef struct {
	ARect			   rect;
	std::vector<float> data;
} IMAGE;

static void __DeleteImage(uptr_t item, void *context)
{
	UNUSED(context);

	delete (IMAGE *)item;
}

IMAGE *CreateImage(const char *filename, const IMAGE *img0)
{
	AImage image;
	IMAGE  *img = NULL;

	if (image.LoadJPEG(filename)) {
		if ((img = new IMAGE) != NULL) {
			img->rect = image.GetRect();

			if (!img0 || (img->rect == img0->rect)) {
				const AImage::PIXEL *pixel = image.GetPixelData();
				float *ptr, *endptr, *p;
				float *yavg, *yp;
				uint_t x, y, w = img->rect.w, h = img->rect.h, len = w * h * 3;
				double avg[3], sd[3];
				const float yascl = 1.f / (float)w;

				img->data.resize(len + h * 3);
				ptr    = &img->data[0];
				endptr = ptr + len;
				yavg   = endptr;

				memset(ptr, 0, img->data.size() * sizeof(*ptr));

				for (y = 0, p = ptr, yp = yavg; y < h; y++, yp += 3) {
					for (x = 0; x < w; x++, p += 3, pixel++) {
						p[0]   = (float)pixel->r;
						p[1]   = (float)pixel->g;
						p[2]   = (float)pixel->b;
						yp[0] += p[0];
						yp[1] += p[1];
						yp[2] += p[2];
					}
				}

				memset(avg, 0, sizeof(avg));
				memset(sd,  0, sizeof(sd));

				for (y = 0, yp = yavg, p = ptr; y < h; y++, yp += 3) {
					yp[0] *= yascl;
					yp[1] *= yascl;
					yp[2] *= yascl;

					for (x = 0; x < w; x++, p += 3) {
						p[0]   -= yp[0];
						p[1]   -= yp[1];
						p[2]   -= yp[2];

						avg[0] += p[0];
						avg[1] += p[1];
						avg[2] += p[2];

						sd[0]  += p[0] * p[0];
						sd[1]  += p[1] * p[1];
						sd[2]  += p[2] * p[2];
					}
				}

				double scl = 1.0 / (double)len;
				for (y = 0; y < 3; y++) {
					avg[y] *= scl;
					sd[y]   = sqrt(sd[y] * scl - avg[y] * avg[y]);
					if (sd[y] == 0.0) sd[y] = 1.0;
					else			  sd[y] = 1.0 / sd[y];
				}

				for (p = ptr, y = 0; p < endptr; p++, y = (y + 1) % 3) {
					p[0] = (p[0] - avg[y]) * sd[y];
				}
			}
			else {
				fprintf(stderr, "Image in file '%s' is %d x %d and not %d x %d\n", filename, img->rect.w, img->rect.h, img0->rect.w, img0->rect.h);
				delete img;
				img = NULL;
			}
		}
	}
	else {
		fprintf(stderr, "Failed to load image from file '%s'\n", filename);
	}

	return img;
}

int main(int argc, char *argv[])
{
	ADataList imglist;
	IMAGE  *img;
	double factor = 3.0;
	bool   detection = false;
	int    i;

	imglist.SetDestructor(&__DeleteImage);

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (strcmp(argv[i], "-factor") == 0) factor = (double)AString(argv[++i]);
		}
		else if ((img = CreateImage(argv[i], (const IMAGE *)imglist[0])) != NULL) {
			imglist.Add(img);
		}
	}

	if (imglist.Count() > 1) {
		IMAGE **images = (IMAGE **)imglist.List();
		const ARect& rect = images[0]->rect;
		uint_t len = rect.w * rect.h * 3;
		float  *ptr = &images[0]->data[0], *endptr = ptr + len, *p;
		uint_t i, n = imglist.Count() - 1;

		for (i = 1; i < n; i++) {
			const float *ptr2 = &images[i]->data[0], *p2;

			for (p = ptr, p2 = ptr2; p < endptr; p++, p2++) p[0] += p2[0];
		}

		if (n > 1) {
			float scl = 1.f / (float)n;
			for (p = ptr; p < endptr; p++) p[0] *= scl;
		}

		{
			const float *ptr2 = &images[i]->data[0], *p2;
			double sum = 0.0;

			for (p = ptr, p2 = ptr2; p < endptr; p++, p2++) {
				p[0] = (p[0] - p2[0]) * factor;
				sum += p[0] * p[0];
			}

			sum = sqrt(sum / (double)len);
			detection = (sum >= 1.0);
			fprintf(stderr, "Diff = %0.6lf\n", sum);
		}

		if (detection) {
			AImage img;
			if (img.Create(rect.w, rect.h)) {
				AImage::PIXEL *pixel = img.GetPixelData();

				for (p = ptr; p < endptr; p += 3, pixel++) {
					pixel->r = (uint8_t)LIMIT(127.5 + 127.5 * p[0], 0.0, 255.0);
					pixel->g = (uint8_t)LIMIT(127.5 + 127.5 * p[1], 0.0, 255.0);
					pixel->b = (uint8_t)LIMIT(127.5 + 127.5 * p[2], 0.0, 255.0);
				}

				const TAG tags[] = {
					{AImage::TAG_JPEG_QUALITY, 95},
					{TAG_DONE},
				};
				AString filename = ADateTime().DateFormat("Detection-%Y-%M-%D-%h-%m-%s.jpg");
				fprintf(stderr, "Saving detection image in '%s'\n", filename.str());
				img.SaveJPEG(filename, tags);
			}
		}
	}

	return detection ? 1 : 0;
}

