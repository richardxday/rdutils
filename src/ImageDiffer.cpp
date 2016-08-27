
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <algorithm>

#include <rdlib/Recurse.h>
#include <rdlib/jpeginfo.h>

#include "ImageDiffer.h"

ImageDiffer::ImageDiffer(const ASettingsHandler& _settings, ASettingsHandler& _stats, AStdFile& _log, uint_t _index) :
	global(_settings),
	stats(_stats),
	log(_log),
	index(_index),
	name(AString("imagediff%").Arg(index)),
	settings(name, ~0),
	sample(0),
	verbose(0)
{
	imglist.SetDestructor(&__DeleteImage);
	
	diffavg = (double)stats.Get(AString("avg%").Arg(index), "0.0");
	diffsd  = (double)stats.Get(AString("sd%").Arg(index),  "0.0");

	Configure();

	log.printf("%s[%u]: New differ\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index);
}

ImageDiffer::~ImageDiffer()
{
	remove(tempfile);

	log.printf("%s[%u]: Shutting down\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index);
}

void ImageDiffer::Configure()
{
	settings.Read();

	AString nstr = AString("%").Arg(index);
	wgetargs  	 = settings.Get("wgetargs",  		 global.Get("wgetargs"));
	cameraurl    = settings.Get("cameraurl", 		 global.Get("cameraurl"));
	videosrc     = settings.Get("videosrc",			 global.Get("videosrc"));
	streamerargs = settings.Get("streamerargs",		 global.Get("streamerargs", "-s 640x480"));
	capturecmd   = settings.Get("capturecmd",  		 global.Get("capturecmd")).DeEscapify();
	tempfile  	 = settings.Get("tempfile", 		 global.Get("tempfile", AString("/home/%/temp{index}.jpg").Arg(getenv("LOGNAME")))).SearchAndReplace("{index}", nstr);
	imagedir  	 = settings.Get("imagedir", 		 global.Get("imagedir", "/media/cctv")).SearchAndReplace("{index}", nstr);
	imagefmt  	 = settings.Get("filename", 		 global.Get("filename", "%Y-%M-%D/%h/{index}/Image-%Y-%M-%D-%h-%m-%s-%S")).SearchAndReplace("{index}", nstr);
	detimgdir 	 = settings.Get("detimagedir", 	  	 global.Get("detimagedir")).SearchAndReplace("{index}", nstr);
	detimgfmt 	 = settings.Get("detfilename", 	  	 global.Get("detfilename", "%Y-%M-%D/%h/{index}/detection/Image-%Y-%M-%D-%h-%m-%s-%S")).SearchAndReplace("{index}", nstr);
	detcmd    	 = settings.Get("detcommand",  	  	 global.Get("detcommand", "")).SearchAndReplace("{index}", nstr);
	detstartcmd  = settings.Get("detstartcommand",   global.Get("detstartcommand", "")).SearchAndReplace("{index}", nstr);
	detendcmd    = settings.Get("detendcommand",  	 global.Get("detendcommand",   "")).SearchAndReplace("{index}", nstr);
	nodetcmd  	 = settings.Get("nodetcommand",  	 global.Get("nodetcommand", "")).SearchAndReplace("{index}", nstr);
	coeff  	  	 = (double)settings.Get("coeff", 	 global.Get("coeff", "1.0e-3"));
	avgfactor 	 = (double)settings.Get("avgfactor", global.Get("avgfactor", "1.0"));
	sdfactor  	 = (double)settings.Get("sdfactor",  global.Get("sdfactor", "2.0"));
	redscale  	 = (double)settings.Get("rscale", 	 global.Get("rscale", "1.0"));
	grnscale  	 = (double)settings.Get("gscale", 	 global.Get("gscale", "1.0"));
	bluscale  	 = (double)settings.Get("bscale", 	 global.Get("bscale", "1.0"));
	threshold 	 = (double)settings.Get("threshold", global.Get("threshold", "3000.0"));
	subsample 	 = (uint_t)settings.Get("subsample", global.Get("subsample", "1"));
	verbose   	 = (uint_t)settings.Get("verbose",   global.Get("verbose", "0"));

	detcount     = 0;

	gainimage.Delete();
	AString gainfilename = settings.Get("gainimage", global.Get("gainimage", ""));
	if (gainfilename.Valid()) {
		if (gainimage.Load(gainfilename)) {
			log.printf("%s[%u]: Loaded gain image '%s'\n",
					   ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(),
					   index,
					   gainfilename.str());   
		}
		else {
			log.printf("%s[%u]: Failed to load gain image '%s'\n",
					   ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(),
					   index,
					   gainfilename.str());   
		}
	}
	gaindata.resize(0);
	
	predetectionimages  = (uint_t)settings.Get("predetectionimages",  global.Get("predetectionimages",  "2"));
	postdetectionimages = (uint_t)settings.Get("postdetectionimages", global.Get("postdetectionimages", "2"));
	forcesavecount    = 0;
	
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
			ncols = std::max(ncols, n);
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

ImageDiffer::IMAGE *ImageDiffer::CreateImage(AStdData& log, const char *filename, const IMAGE *img0)
{
	IMAGE *img = NULL;

	if ((img = new IMAGE) != NULL) {
		JPEG_INFO info;
		AImage& image = img->image;

		if (ReadJPEGInfo(filename, info) && image.LoadJPEG(filename)) {
			img->filename = filename;
			img->rect     = image.GetRect();
			img->saved    = false;
			
			if (img0 && (img0->rect != img->rect)) {
				log.printf("%s[%u]: Images are different sizes (%dx%d -> %dx%d), deleting image list\n",
						   ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(),
						   index,
						   img0->rect.w, img0->rect.h, img->rect.w, img->rect.h);

				imglist.DeleteList();
			}
		}
		else {
			log.printf("%s[%u]: Failed to load image '%s''\n",
					   ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(),
					   index,
					   filename);
			delete img;
			img = NULL;
		}
	}

	return img;
}

void ImageDiffer::Process(const ADateTime& dt, bool update)
{
	if (update || settings.HasFileChanged()) {
		log.printf("%s[%u]: Re-configuring\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index);
		Configure();
	}

	if ((++sample) >= subsample) {
		AString cmd;

		if (cameraurl.Valid()) {
			cmd.printf("wget %s \"%s\" -O %s 2>/dev/null", wgetargs.str(), cameraurl.str(), tempfile.str());
		}
		else if (videosrc.Valid()) {
			cmd.printf("streamer -c /dev/%s %s -o %s 2>/dev/null", videosrc.str(), streamerargs.str(), tempfile.str());
		}
		else if (capturecmd.Valid()) {
			cmd.printf("%s 2>/dev/null", capturecmd.SearchAndReplace("{file}", tempfile).str());
		}

		if (cmd.Valid()) {
			if (system(cmd) == 0) {
				IMAGE *img;

				if ((img = CreateImage(log, tempfile, (const IMAGE *)imglist[0])) != NULL) {
					img->dt = dt;
				
					imglist.Add(img);

					// strip unneeded images off start of image list
					// need to keep at least 2 and at least predetectionimages+1 images
					while ((imglist.Count() > 2) && (imglist.Count() > (predetectionimages + 1))) {
						delete (IMAGE *)imglist[0];
						imglist.Pop();
					}

					// if there are enough images to compare
					if (imglist.Count() >= 2) {
						const IMAGE *img1 = (const IMAGE *)imglist[imglist.Count() - 2];
						IMAGE *img2       = (IMAGE       *)imglist[imglist.Count() - 1];
						const AImage::PIXEL *pix1 = img1->image.GetPixelData();
						const AImage::PIXEL *pix2 = img2->image.GetPixelData();
						const ARect& rect = img1->rect;
						std::vector<float> data;
						float *ptr, *p;
						double avg[3];
						uint_t x, y, w = rect.w, h = rect.h, w3 = w * 3, len = w * h, len3 = w3 * h;
				
						data.resize(len3);
						ptr = &data[0];

						memset(avg, 0, sizeof(avg));

						// find difference between the two images and
						// normalize against average on each line per component
						for (y = 0, p = ptr; y < h; y++) {
							double yavg[3];

							// reset average
							memset(yavg, 0, sizeof(yavg));

							// subtract pixels and update per-component average
							for (x = 0; x < w; x++, p += 3, pix1++, pix2++) {
								p[0]     = ((float)pix1->r - (float)pix2->r) * redscale;
								p[1]     = ((float)pix1->g - (float)pix2->g) * grnscale;
								p[2]     = ((float)pix1->b - (float)pix2->b) * bluscale;

								yavg[0] += p[0];
								yavg[1] += p[1];
								yavg[2] += p[2];
							}

							yavg[0] /= (double)w;
							yavg[1] /= (double)w;
							yavg[2] /= (double)w;

							// subtract average from each pixel
							// and update total average
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

						// update gaindata array from gainimage - gaindata array is same size as the incoming images
						// whatever the size of the original gainimage is
						// an invalid gainimage results in a single value (white) used throughout gaindata
						{
							const uint_t gainwid = gainimage.GetRect().w;
							const uint_t gainhgt = gainimage.GetRect().h;
							uint_t i = 0, n = w * h * 3;

							// resize and recalculate gaindata array if necessary
							if (n != gaindata.size()) {
								static const AImage::PIXEL white = {255, 255, 255, 0};
								const AImage::PIXEL *gainptr = gainimage.GetPixelData() ? gainimage.GetPixelData() : &white;
								float sum = 0.f;
								
								gaindata.resize(n);

								for (y = 0; y < h; y++) {
									// convert detection y into gainimage y
									const uint_t y2 = (y * gainhgt + h / 2) / h;

									for (x = 0; x < w; x++) {
										// convert detection x into gainimage x
										const uint_t x2 = (x * gainwid + w / 2) / w;
										// get ptr to pixel data
										const AImage::PIXEL *p = gainptr + x2 + y2 * gainwid;

										// convert pixel into RGB floating point values 0-1
										gaindata[i] = (float)p->r / 255.f; sum += gaindata[i++];
										gaindata[i] = (float)p->g / 255.f; sum += gaindata[i++];
										gaindata[i] = (float)p->b / 255.f; sum += gaindata[i++];
									}
								}

								// scale all gain data up so it sums to w * h * 3 
								if (sum > 0.f) {
									sum = (float)n / sum;

									for (i = 0; i < n; i++) gaindata[i] *= sum;
								}
							}
						}

						// subtract overall average from pixel data, scale by gain image and
						// calculate modulus
						float *p2, *p3;
						for (y = 0, p = ptr, p2 = ptr, p3 = &gaindata[0]; y < h; y++) {
							for (x = 0; x < w; x++, p += 3, p2++, p3 += 3) {
								p[0] -= avg[0];
								p[1] -= avg[1];
								p[2] -= avg[2];
								p[0] *= p3[0];
								p[1] *= p3[1];
								p[2] *= p3[2];
								p2[0] = sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
							}
						}

						// find average and SD of resultant difference data with matrix applied
						double avg2 = 0.0, sd2 = 0.0;
						uint_t mx, my, cx = (matwid - 1) >> 1, cy = (mathgt - 1) >> 1;
						for (x = 0; x < w; x++) {
							for (y = 0; y < h; y++) {
								float val = 0.f;

								// apply matrix to data
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
								// or just use original if no matrix
								else val = ptr[x + y * w];

								// store data somewhere else (beyond original data) to prevent alteration of data used by matrix calculation
								ptr[w * h + x + y * w] = (float)val;

								// update average and SD
								avg2 += val;
								sd2  += val * val;
							}
						}

						// calculate final average and SD
						avg2 /= (double)len;
						sd2   = sqrt(sd2 / (double)len - avg2 * avg2);

						// filter values
						Interpolate(diffavg, avg2, coeff);
						Interpolate(diffsd,  sd2,  coeff);

						// store values in settings handler
						stats.Set(AString("avg%").Arg(index), AString("%0.16e").Arg(diffavg));
						stats.Set(AString("sd%").Arg(index),  AString("%0.16e").Arg(diffsd));

						// calculate minimum level based on average and SD values, individual levels must exceed this
						double diff = avgfactor * diffavg + sdfactor * diffsd, total = 0.0;
						// find total = sum of levels above minimum level
						for (x = 0; x < len; x++) {
							ptr[x] = std::max(ptr[w * h + x] - diff, 0.0);
							total += ptr[x];
						}

						// divide by area of image and multiply up to make values arbitarily scaled
						total = total * 1000.0 / ((double)w * (double)h);

						if (verbose) log.printf("%s[%u]: Level = %0.1lf\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, total);

						stats.Set(AString("level%").Arg(index), AString("%0.4").Arg(total));

						// create detection image, if detection image directory valid
						if (detimgdir.Valid() && img2->detimage.Create(rect.w, rect.h)) {
							const AImage::PIXEL *pixel1 = img1->image.GetPixelData();
							const AImage::PIXEL *pixel2 = img2->image.GetPixelData();
							AImage::PIXEL *pixel = img2->detimage.GetPixelData();

							// use individual level from above and scale and max RGB values from original images
							for (x = 0; x < len; x++, pixel++, pixel1++, pixel2++) {
								pixel->r = (uint8_t)limit((double)std::max(pixel1->r, pixel2->r) * ptr[x] / 255.0, 0.0, 255.0);
								pixel->g = (uint8_t)limit((double)std::max(pixel1->g, pixel2->g) * ptr[x] / 255.0, 0.0, 255.0);
								pixel->b = (uint8_t)limit((double)std::max(pixel1->b, pixel2->b) * ptr[x] / 255.0, 0.0, 255.0);
							}
						}

						// should image(s) be saved?
						if ((total >= threshold) || forcesavecount) {
							uint_t i;

							// save predetectionimages plus current image from image list (if they have not already been saved)

							// calculate starting position in list
							if (imglist.Count() >= (predetectionimages + 1)) i = imglist.Count() - (predetectionimages + 1);
							else										     i = 0;

							// save any unsaved images, including latest
							for (; i < imglist.Count(); i++) {
								SaveImage((IMAGE *)imglist[i]);
							}

							// if a detection has been found, force the next postdetectionimages to be saved
							if		(total >= threshold) forcesavecount = postdetectionimages;
							// else decrement forcesavecount
							else if (forcesavecount)	 forcesavecount--;
						}

						if (total >= threshold) {
							// start if detection?
							if (!detcount && detstartcmd.Valid()) {
								AString cmd = detstartcmd.SearchAndReplace("{level}", AString("%0.4").Arg(total));
								if (system(cmd) != 0) {
									log.printf("%s[%u]: Detection start command '%s' failed\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, cmd.str());
								}
							}

							// increment detection count
							detcount++;

							// run detection command
							if (detcmd.Valid()) {
								AString cmd = detcmd.SearchAndReplace("{level}", AString("%0.4").Arg(total)).SearchAndReplace("{detcount}", AString("%").Arg(detcount));
								if (system(cmd) != 0) {
									log.printf("%s[%u]: Detection command '%s' failed\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, cmd.str());
								}
							}
						}
						else {
							// if there's been some detections, run detection end command
							if (detcount && detendcmd.Valid()) {
								AString cmd = detendcmd.SearchAndReplace("{level}", AString("%0.4").Arg(total)).SearchAndReplace("{detcount}", AString("%").Arg(detcount));
								if (system(cmd) != 0) {
									log.printf("%s[%u]: Detection end command '%s' failed\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, cmd.str());
								}
							}

							// reset detection count
							detcount = 0;
						
							// if not a detection, run non-detection command
							if (nodetcmd.Valid()) {
								AString cmd = nodetcmd.SearchAndReplace("{level}", AString("%0.4").Arg(total));
								if (system(cmd) != 0) {
									log.printf("%s[%u]: No-detection command '%s' failed\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, cmd.str());
								}
							}
						}
					}
				}
			}
			else log.printf("%s[%u]: Failed to fetch image using '%s'\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, cmd.str());
		}
		
		log.flush();

		sample = 0;
	}
}

void ImageDiffer::SaveImage(IMAGE *img)
{
	// only save image(s) if not already done so
	if (!img->saved) {
		const ADateTime& dt = img->dt;	// filenames are made up date and time when image was created
		const TAG tags[] = {
			{AImage::TAG_JPEG_QUALITY, 95},
			{TAG_DONE, 0},
		};

		// save detection image, if possible
		if (detimgdir.Valid() && img->detimage.Valid()) {
			AString filename = detimgdir.CatPath(dt.DateFormat(detimgfmt) + ".jpg");
			CreateDirectory(filename.PathPart());
			img->detimage.SaveJPEG(filename, tags);
		}

		// save main image
		AString filename = imagedir.CatPath(dt.DateFormat(imagefmt) + ".jpg");
		CreateDirectory(filename.PathPart());
		log.printf("%s[%u]: Saving detection image in '%s'\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s").str(), index, filename.str());
		img->image.SaveJPEG(filename, tags);

		// mark as saved
		img->saved = true;
	}
}
