
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <vector>

#include <rdlib/strsup.h>
#include <rdlib/DataList.h>
#include <rdlib/BMPImage.h>
#include <rdlib/DateTime.h>
#include <rdlib/Recurse.h>
#include <rdlib/jpeginfo.h>
#include <rdlib/SettingsHandler.h>
#include <rdlib/QuitHandler.h>

typedef struct {
	AString	filename;
	AImage	image;
	ARect	rect;
} IMAGE;

static volatile bool hupsignal = false;

static void detecthup(int sig)
{
	hupsignal |= (sig == SIGHUP);
}

static void __DeleteImage(uptr_t item, void *context)
{
	UNUSED(context);

	delete (IMAGE *)item;
}

IMAGE *CreateImage(AStdData& log, const char *filename, const IMAGE *img0)
{
	IMAGE *img = NULL;

	if ((img = new IMAGE) != NULL) {
		JPEG_INFO info;
		AImage& image = img->image;

		if (ReadJPEGInfo(filename, info) && image.LoadJPEG(filename)) {
			img->filename = filename;
			img->rect     = image.GetRect();
			
			if (img0 && (img->rect != img0->rect)) {
				delete img;
				img = NULL;
			}
		}
		else {
			log.printf("%s: Failed to load image '%s''\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), filename);
			delete img;
			img = NULL;
		}
	}

	return img;
}

int main(int argc, char *argv[])
{
	AQuitHandler     quithandler;
	ASettingsHandler stats("imagediff-stats", 5000);
	ADataList imglist;
	AStdFile  log;
	IMAGE     *img;
	uint32_t  days = 0;
	int    	  i;
	bool	  startup = true;

	imglist.SetDestructor(&__DeleteImage);

	signal(SIGHUP, &detecthup);

	while (!quithandler.HasQuit()) {
		ASettingsHandler settings("imagediff");
		AString   wgetargs    = settings.Get("wgetargs");
		AString   camurl      = settings.Get("cameraurl");
		AString   tempfile    = settings.Get("tempfile", "/tmp/tempfs/temp.jpg");
		AString   imagedir    = settings.Get("imagedir", "/media/cctv");
		AString   imagefmt    = settings.Get("filename", "%Y-%M-%D/%h/Image-%Y-%M-%D-%h-%m-%s-%S");
		AString   detimgdir   = settings.Get("detimagedir", "");
		AString   detimgfmt   = settings.Get("detfilename", imagefmt);
		AString   loglocation = settings.Get("loglocation", "/var/log/imagediff");
		double    diffavg 	  = (double)stats.Get("avg", "0.0");
		double    diffsd  	  = (double)stats.Get("sd", "0.0");
		double    coeff       = (double)settings.Get("coeff", "1.0e-3");
		double    factor      = (double)settings.Get("factor", "2.0");
		double    threshold   = (double)settings.Get("threshold", "3000.0");
		uint64_t  delay       = (uint64_t)(1000.0 * (double)settings.Get("delay", "1.0"));

		hupsignal = false;
		while (!hupsignal && !quithandler.HasQuit()) {
			ADateTime dt;
			uint32_t days1;

			if ((days1 = dt.GetDays()) != days) {
				days = days1;

				log.close();
				log.open(loglocation.CatPath(dt.DateFormat("imagediff-%Y-%M-%D.log")), "a");
				
				if (startup) {
					log.printf("%s: Starting detection...\n", dt.DateFormat("%Y-%M-%D %h:%m:%s").str());
					startup = false;
				}
			}

			stats.CheckWrite();
		
			AString cmd;
			cmd.printf("wget %s \"%s\" -O %s 2>/dev/null", wgetargs.str(), camurl.str(), tempfile.str());
			if (system(cmd) == 0) {
				if ((img = CreateImage(log, tempfile, (const IMAGE *)imglist[0])) != NULL) {
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

						stats.Set("avg", AString("%0.16le").Arg(diffavg));
						stats.Set("sd",  AString("%0.16le").Arg(diffsd));

						double diff = diffavg + diffsd * factor, total = 0.0;
						for (x = 0; x < len; x++) {
							ptr[x] = MAX(ptr[x] - diff, 0.0);
							total += ptr[x];
						}

						total = total * 1000.0 / ((double)w * (double)h);

						log.printf("%s: Level = %0.1lf\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), total);

						if (total >= threshold) {
							const TAG tags[] = {
								{AImage::TAG_JPEG_QUALITY, 95},
								{TAG_DONE},
							};

							if (detimgdir.Valid()) {
								AImage img;
								if (img.Create(rect.w, rect.h)) {
									const AImage::PIXEL *opixel = img1->image.GetPixelData();
									AImage::PIXEL *pixel = img.GetPixelData();

									for (x = 0; x < len; x++, pixel++, opixel++) {
										pixel->r = (uint8_t)LIMIT((double)opixel->r * ptr[x] / 255.0, 0.0, 255.0);
										pixel->g = (uint8_t)LIMIT((double)opixel->g * ptr[x] / 255.0, 0.0, 255.0);
										pixel->b = (uint8_t)LIMIT((double)opixel->b * ptr[x] / 255.0, 0.0, 255.0);
									}

									AString filename = detimgdir.CatPath(dt.DateFormat(detimgfmt) + ".jpg");
									CreateDirectory(filename.PathPart());
									img.SaveJPEG(filename, tags);
								}
							}
						
							AString filename = imagedir.CatPath(dt.DateFormat(imagefmt) + ".jpg");
							CreateDirectory(filename.PathPart());
							log.printf("%s: Saving detection image in '%s'\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), filename.str());
							img1->image.SaveJPEG(filename, tags);
						}
					
						delete img1;
						imglist.Pop();
					}
				}
			}
			else log.printf("%s: Failed to fetch image using '%s'\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), cmd.str());

			log.flush();

			uint64_t dt1    = dt;
			uint64_t dt2    = ADateTime();
			uint64_t msdiff = SUBZ(dt1 + delay, dt2);
			Sleep((uint_t)msdiff);
		}

		if (hupsignal) log.printf("%s: Re-loading configuration...\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str());

		remove(tempfile);
	}

	log.close();

	return 0;
}
