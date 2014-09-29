
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

static volatile bool hupsignal = false;

static void detecthup(int sig)
{
	hupsignal |= (sig == SIGHUP);
}

class ImageDiffer {
public:
	ImageDiffer(const ASettingsHandler& _settings, ASettingsHandler& _stats, AStdFile& _log, uint_t _index) :
		global(_settings),
		stats(_stats),
		log(_log),
		index(_index),
		name(AString("imagediff%u").Arg(index)),
		settings(name, ~0),
		sample(0) {
		imglist.SetDestructor(&__DeleteImage);

		diffavg = (double)stats.Get(AString("avg%u").Arg(index), "0.0");
		diffsd  = (double)stats.Get(AString("sd%u").Arg(index),  "0.0");

		Configure();

		log.printf("%s[%u]: New differ\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index);
	}
	~ImageDiffer() {
		remove(tempfile);

		log.printf("%s[%u]: Shutting down\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index);
	}

	void Process(const ADateTime&dt, bool update);

	static void Delete(uptr_t item, void *context) {
		UNUSED(context);
		delete (ImageDiffer *)item;
	}

protected:
	typedef struct {
		AString	filename;
		AImage	image;
		ARect	rect;
	} IMAGE;

	static void __DeleteImage(uptr_t item, void *context) {
		UNUSED(context);

		delete (IMAGE *)item;
	}

	IMAGE *CreateImage(AStdData& log, const char *filename, const IMAGE *img0) {
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

	void Configure();

protected:
	const ASettingsHandler& global;
	ASettingsHandler&		stats;
	AStdFile&				log;
	uint_t					index;
	AString					name;
	ASettingsHandler 		settings;
	ADataList				imglist;
	AString   			  	wgetargs;
	AString   			  	camurl;
	AString   			  	tempfile;
	AString   			  	imagedir;
	AString   			  	imagefmt;
	AString   			  	detimgdir;
	AString   			  	detimgfmt;
	double    			  	diffavg;
	double    			  	diffsd;
	double    			  	coeff;
	double    			  	avgfactor;
	double    			  	sdfactor;
	double    			  	redscale;
	double    			  	grnscale;
	double    			  	bluscale;
	double    			  	threshold;
	std::vector<float>		matrix;
	float     			  	matmul;
	uint_t    			  	matwid, mathgt;
	uint_t				    subsample, sample;
	uint32_t				statswritetime;
};

void ImageDiffer::Configure()
{
	settings.Read();

	AString nstr = AString("%u").Arg(index);
	wgetargs  = settings.Get("wgetargs", global.Get("wgetargs"));
	camurl    = settings.Get("cameraurl", global.Get("cameraurl"));
	tempfile  = settings.Get("tempfile", global.Get("tempfile", AString("/home/%s/temp%n.jpg").Arg(getenv("LOGNAME")))).SearchAndReplace("%n", nstr);
	imagedir  = settings.Get("imagedir", global.Get("imagedir", "/media/cctv")).SearchAndReplace("%n", nstr);
	imagefmt  = settings.Get("filename", global.Get("filename", "%Y-%M-%D/%h/%n/Image-%Y-%M-%D-%h-%m-%s-%S")).SearchAndReplace("%n", nstr);
	detimgdir = settings.Get("detimagedir", global.Get("detimagedir")).SearchAndReplace("%n", nstr);
	detimgfmt = settings.Get("detfilename", global.Get("detfilename", "%Y-%M-%D/%h/%n/detection/Image-%Y-%M-%D-%h-%m-%s-%S")).SearchAndReplace("%n", nstr);
	coeff  	  = (double)settings.Get("coeff", global.Get("coeff", "1.0e-3"));
	avgfactor = (double)settings.Get("avgfactor", global.Get("avgfactor", "1.0"));
	sdfactor  = (double)settings.Get("sdfactor", global.Get("sdfactor", "2.0"));
	redscale  = (double)settings.Get("rscale", global.Get("rscale", "1.0"));
	grnscale  = (double)settings.Get("gscale", global.Get("gscale", "1.0"));
	bluscale  = (double)settings.Get("bscale", global.Get("bscale", "1.0"));
	threshold = (double)settings.Get("threshold", global.Get("threshold", "3000.0"));
	subsample = (uint_t)settings.Get("subsample", global.Get("subsample", "1"));
	matmul 	  = 1.f;
	matwid 	  = mathgt = 0;

	AString _matrix = settings.Get("matrix", global.Get("matrix", ""));
	if (_matrix.Valid()) {
		uint_t row, nrows = _matrix.CountLines(";");
		uint_t col, ncols = 1;
		int    p;

		if ((p = _matrix.Pos("*")) >= 0) {
			AString mul = _matrix.Mid(p + 1);
				
			_matrix = _matrix.Left(p);

			if ((p = mul.Pos("/")) >= 0) {
				matmul = (float)mul.Left(p) / (float)mul.Mid(p + 1);
			}
			else matmul = (float)mul;
		}
		else if ((p = _matrix.Pos("/")) >= 0) {
			AString mul = _matrix.Mid(p + 1);
				
			_matrix = _matrix.Left(p);

			matmul = 1.f / (float)mul;
		}

		for (row = 0; row < nrows; row++) {
			uint_t n = _matrix.Line(row, ";").CountLines(",");
			ncols = MAX(ncols, n);
		}

		nrows |= 1;
		ncols |= 1;

		matrix.resize(nrows * ncols);
		for (row = 0; row < nrows; row++) {
			AString line = _matrix.Line(row,";");
				
			for (col = 0; col < ncols; col++) matrix[col + row * ncols] = (float)line.Line(col, ",");
		}

		matwid = ncols;
		mathgt = nrows;

#if 0
		printf("Matrix is %u x %u:\n", matwid, mathgt);
		for (row = 0; row < nrows; row++) {
			for (col = 0; col < ncols; col++) printf("%8.3f", matrix[col + row * ncols]);
			printf("\n");
		}
		printf("Multiplier %0.6f\n", matmul);
#endif
	}
	else matrix.resize(0);
}

void ImageDiffer::Process(const ADateTime&dt, bool update)
{
	if (update || settings.HasFileChanged()) {
		log.printf("%s[%u]: Re-configuring\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index);
		Configure();
	}

	if (camurl.Valid() && ((++sample) >= subsample)) {
		AString cmd;
	
		cmd.printf("wget %s \"%s\" -O %s 2>/dev/null", wgetargs.str(), camurl.str(), tempfile.str());
		if (system(cmd) == 0) {
			IMAGE *img;

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
							p[0] 	 = ((float)pix1->r - (float)pix2->r) * redscale;
							p[1] 	 = ((float)pix1->g - (float)pix2->g) * grnscale;
							p[2] 	 = ((float)pix1->b - (float)pix2->b) * bluscale;

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
					for (y = 0, p = ptr, p2 = ptr; y < h; y++) {
						for (x = 0; x < w; x++, p += 3, p2++) {
							p[0] -= avg[0];
							p[1] -= avg[1];
							p[2] -= avg[2];
							p2[0] = sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
						}
					}

					double avg2 = 0.0, sd2 = 0.0;
					uint_t mx, my, cx = (matwid - 1) >> 1, cy = (mathgt - 1) >> 1;
					for (x = 0; x < w; x++) {
						for (y = 0; y < h; y++) {
							float val = 0.f;

							if (matwid && mathgt) {
								for (my = 0; my < mathgt; my++) {
									if (((y + my) >= cy) && ((y + my) < (h + cy))) {
										for (mx = 0; mx < matwid; mx++) {
											if (((x + mx) >= cx) && ((x + mx) < (h + cx))) {
												val += matrix[mx + my * matwid] * ptr[(x + mx - cx) + (y + my - cy) * w];
											}
										}
									}
								}

								val *= matmul;
							}
							else val = ptr[x + y * w];

							ptr[w * h + x + y * w] = (float)val;
							avg2 += val;
							sd2  += val * val;
						}
					}

					avg2 /= (double)len;
					sd2   = sqrt(sd2 / (double)len - avg2 * avg2);

					Interpolate(diffavg, avg2, coeff);
					Interpolate(diffsd,  sd2,  coeff);

					stats.Set(AString("avg%u").Arg(index), AString("%0.16le").Arg(diffavg));
					stats.Set(AString("sd%u").Arg(index),  AString("%0.16le").Arg(diffsd));

					double diff = avgfactor * diffavg + sdfactor * diffsd, total = 0.0;
					for (x = 0; x < len; x++) {
						ptr[x] = MAX(ptr[w * h + x] - diff, 0.0);
						total += ptr[x];
					}

					total = total * 1000.0 / ((double)w * (double)h);

					//log.printf("%s[%u]: Level = %0.1lf\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, total);

					stats.Set(AString("level%u").Arg(index), AString("%0.4lf").Arg(total));

					if (total >= threshold) {
						const TAG tags[] = {
							{AImage::TAG_JPEG_QUALITY, 95},
							{TAG_DONE},
						};

						if (detimgdir.Valid()) {
							AImage img;
							if (img.Create(rect.w, rect.h)) {
								const AImage::PIXEL *pixel1 = img1->image.GetPixelData();
								const AImage::PIXEL *pixel2 = img2->image.GetPixelData();
								AImage::PIXEL *pixel = img.GetPixelData();

								for (x = 0; x < len; x++, pixel++, pixel1++, pixel2++) {
									pixel->r = (uint8_t)LIMIT((double)MAX(pixel1->r, pixel2->r) * ptr[x] / 255.0, 0.0, 255.0);
									pixel->g = (uint8_t)LIMIT((double)MAX(pixel1->g, pixel2->g) * ptr[x] / 255.0, 0.0, 255.0);
									pixel->b = (uint8_t)LIMIT((double)MAX(pixel1->b, pixel2->b) * ptr[x] / 255.0, 0.0, 255.0);
								}

								AString filename = detimgdir.CatPath(dt.DateFormat(detimgfmt) + ".jpg");
								CreateDirectory(filename.PathPart());
								img.SaveJPEG(filename, tags);
							}
						}
						
						AString filename = imagedir.CatPath(dt.DateFormat(imagefmt) + ".jpg");
						CreateDirectory(filename.PathPart());
						log.printf("%s[%u]: Saving detection image in '%s'\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, filename.str());
						img2->image.SaveJPEG(filename, tags);
					}
					
					delete img1;
					imglist.Pop();
				}
			}
		}
		else log.printf("%s[%u]: Failed to fetch image using '%s'\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, cmd.str());

		log.flush();

		sample = 0;
	}
}

int main(int argc, char *argv[])
{
	AQuitHandler     quithandler;
	ADataList        differs;
	ASettingsHandler settings("imagediff", ~0);
	ASettingsHandler stats("imagediff-stats", 5000);
	AString  		 loglocation;
	AStdFile  		 log;
	uint64_t 		 delay;
	uint32_t  		 days    = 0;
	uint32_t		 statswritetime = GetTickCount();
	bool			 update  = true;
	bool			 startup = true;

	signal(SIGHUP, &detecthup);

	differs.SetDestructor(&ImageDiffer::Delete);

	while (!quithandler.HasQuit()) {
		ADateTime dt;
		uint32_t  days1;

		if (update) {
			settings.Read();

			loglocation = settings.Get("loglocation", "/var/log/imagediff");
			delay       = (uint64_t)(1000.0 * (double)settings.Get("delay", "1.0"));
			days        = 0;
		}

		if ((days1 = dt.GetDays()) != days) {
			days = days1;

			log.close();
			log.open(loglocation.CatPath(dt.DateFormat("imagediff-%Y-%M-%D.log")), "a");
				
			if (startup) {
				log.printf("%s[all]: Starting detection...\n", dt.DateFormat("%Y-%M-%D %h:%m:%s").str());
				startup = false;
			}
		}

		if (update) {
			uint_t n = (uint_t)settings.Get("threads", "1");

			while (differs.Count() > n) {
				delete (ImageDiffer *)differs.EndPop();
			}
			while (differs.Count() < n) {
				differs.Add(new ImageDiffer(settings, stats, log, differs.Count() + 1));
			}
		}

		uint_t i, n = differs.Count();
		ImageDiffer **list = (ImageDiffer **)differs.List();
		for (i = 0; i < n; i++) {
			list[i]->Process(dt, update);
		}


		if (update || ((GetTickCount() - statswritetime) >= 5000)) {
			stats.Write();
			statswritetime = GetTickCount();
		}

		uint64_t dt1    = dt;
		uint64_t dt2    = ADateTime();
		uint64_t msdiff = SUBZ(dt1 + delay, dt2);
		Sleep((uint_t)msdiff);

		if (hupsignal || settings.HasFileChanged()) {
			log.printf("%s[all]: Reloading configuration\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str());
			hupsignal = false;
			update    = true;
		}
		else update = false;
	}

	differs.DeleteList();

	log.close();

	return 0;
}
