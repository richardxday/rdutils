
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <algorithm>

#include <rdlib/Recurse.h>
#include <rdlib/jpeginfo.h>

#include "ImageDiffer.h"

uint_t	ImageDiffer::settingschangecount = 0;

ImageDiffer::ImageDiffer(uint_t _index) :
	AThread(),
	index(_index),
	settingschange(settingschangecount),
	verbose(0)
{
	imglist.SetDestructor(&__DeleteImage);
	
	diffavg = GetStat("avg");
	diffsd  = GetStat("sd");

	Configure();

	Log("New differ");
}

ImageDiffer::~ImageDiffer()
{
	Log("Shutting down");
	remove(tempfile);

	Stop();
}

void ImageDiffer::Log(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	Log(index, fmt, ap);
	va_end(ap);
}

void ImageDiffer::Log(uint_t index, const char *fmt, va_list ap)
{
	static AThreadLockObject loglock;
	ADateTime dt;
	AString   str;
	AString   filename = logpath.CatPath(AString("imagediff-%;.txt").Arg(dt.DateFormat("%Y-%M-%D").str()));
	AStdFile  fp;
	
	str.printf("%s[%u]: ", dt.DateFormat("%Y-%M-%D %h:%m:%s").str(), index);
	str.vprintf(fmt, ap);

	AThreadLock lock(loglock);
	CreateDirectory(filename.PathPart());
	if (fp.open(filename, "a")) {
		fp.printf("%s\n", str.str());
		fp.close();
	}
}

ImageDiffer::ProtectedSettings& ImageDiffer::GetSettings()
{
	static ProtectedSettings _settings("imagediff", true, ~0);
	return _settings;
}

AString ImageDiffer::GetSetting(const AString& name, const AString& defval)
{
	ProtectedSettings& _settings = GetSettings();
	AThreadLock		   lock(_settings);
	ASettingsHandler&  settings = _settings.GetSettings();
	AString			   suffix   = AString(":%").Arg(index);
	
	return settings.Get(name + suffix, settings.Get(name, defval));
}

void ImageDiffer::CheckSettingsUpdate()
{
	if (index == 1) {
		ProtectedSettings& _settings = GetSettings();
		AThreadLock		   lock(_settings);
		ASettingsHandler&  settings = _settings.GetSettings();
		if (settings.HasFileChanged()) {
			settings.Read();
			settingschangecount++;
		}
	}
}

AString ImageDiffer::GetGlobalSetting(const AString& name, const AString& defval)
{
	ProtectedSettings& _settings = GetSettings();
	AThreadLock		   lock(_settings);
	ASettingsHandler&  settings = _settings.GetSettings();

	return settings.Get(name, defval);
}

ImageDiffer::ProtectedSettings& ImageDiffer::GetStats()
{
	static ProtectedSettings _stats("imagediff-stats", true, 5000);
	_stats.GetSettings().CheckWrite();
	return _stats;
}

double ImageDiffer::GetStat(const AString& name)
{
	ProtectedSettings& _stats = GetStats();
	AThreadLock		   lock(_stats);
	ASettingsHandler&  stats  = _stats.GetSettings();
	AString			   suffix = AString(":%").Arg(index);

	return (double)stats.Get(name + suffix);
}

void ImageDiffer::SetStat(const AString& name, double val)
{
	ProtectedSettings& _stats = GetStats();
	AThreadLock		   lock(_stats);
	ASettingsHandler&  stats  = _stats.GetSettings();
	AString			   suffix = AString(":%").Arg(index);
	
	stats.Set(name + suffix, AString("%0.16e").Arg(val));
}

void ImageDiffer::Configure()
{
	AString indexstr = AString("%").Arg(index);
	logpath      = GetSetting("loglocation", "/var/log/imagediff");
	name         = GetSetting("name");
	delay        = (uint_t)(1000.0 * (double)GetSetting("delay", "1"));
	wgetargs  	 = GetSetting("wgetargs");
	cameraurl    = GetSetting("cameraurl");
	videosrc     = GetSetting("videosrc");
	streamerargs = GetSetting("streamerargs", "-s 640x480");
	capturecmd   = GetSetting("capturecmd").DeEscapify();
	tempfile  	 = (GetSetting("tempfile", AString("/home/%/temp-{index}.jpeg").Arg(getenv("LOGNAME"))).
					SearchAndReplace("{index}", indexstr));
	imagedir  	 = (GetSetting("imagedir", "/media/cctv").
					SearchAndReplace("{name}",      name.Valid() ? name : "{index}").
					SearchAndReplace("{index}",     indexstr));
	imagefmt  	 = (GetSetting("filename", "%Y-%M-%D-{name}/%h/Image-%Y-%M-%D-%h-%m-%s-%S").
					SearchAndReplace("{name}",      name.Valid() ? name : "{index}").
					SearchAndReplace("{index}",     indexstr));
	detlogfmt 	 = (GetSetting("detlogfilename", "detection.dat").
					SearchAndReplace("{imagepath}", imagefmt.PathPart()).
					SearchAndReplace("{imagename}", imagefmt.FilePart()).
					SearchAndReplace("{name}",  	name.Valid() ? name : "{index}").
					SearchAndReplace("{index}", 	indexstr));
	detimgdir 	 = (GetSetting("detimagedir").
					SearchAndReplace("{imagedir}",  imagedir).
					SearchAndReplace("{name}",  	name.Valid() ? name : "{index}").
					SearchAndReplace("{index}",     indexstr));
	detimgfmt 	 = (GetSetting("detfilename", "{imagepath}/detection/{imagename}").
					SearchAndReplace("{imagepath}", imagefmt.PathPart()).
					SearchAndReplace("{imagename}", imagefmt.FilePart()).
					SearchAndReplace("{name}",  	name.Valid() ? name : "{index}").
					SearchAndReplace("{index}", 	indexstr));
	detcmd    	 = GetSetting("detcommand").SearchAndReplace("{index}", indexstr);
	detstartcmd  = GetSetting("detstartcommand").SearchAndReplace("{index}", indexstr);
	detendcmd    = GetSetting("detendcommand").SearchAndReplace("{index}", indexstr);
	nodetcmd  	 = GetSetting("nodetcommand").SearchAndReplace("{index}", indexstr);
	coeff  	  	 = (double)GetSetting("coeff", 	   	  "1.0e-3");
	avgfactor 	 = (double)GetSetting("avgfactor", 	  "1.0");
	sdfactor  	 = (double)GetSetting("sdfactor",  	  "2.0");
	redscale  	 = (double)GetSetting("rscale",    	  "1.0");
	grnscale  	 = (double)GetSetting("gscale",    	  "1.0");
	bluscale  	 = (double)GetSetting("bscale",    	  "1.0");
	threshold 	 = (double)GetSetting("threshold", 	  "3000.0");
	logthreshold = (double)GetSetting("logthreshold", "{threshold}").SearchAndReplace("{threshold}", GetSetting("threshold", "3000.0"));
	verbose   	 = (uint_t)GetSetting("verbose",      "0");

	Log("Destination '%s'", imagedir.CatPath(imagefmt).str());
	if (detimgdir.Valid()) Log("Detection files destination '%s'", detimgdir.CatPath(detimgfmt).str());
	if (detlogfmt.Valid()) Log("Detection log '%s' with threshold %0.1lf", imagedir.CatPath(detlogfmt).str(), logthreshold);
	
	cmd.Delete();
	if (cameraurl.Valid()) {
		cmd.printf("wget %s \"%s\" -O %s 2>/dev/null", wgetargs.str(), cameraurl.str(), tempfile.str());
	}
	else if (videosrc.Valid()) {
		cmd.printf("streamer -c /dev/%s %s -o %s 2>/dev/null", videosrc.str(), streamerargs.str(), tempfile.str());
	}
	else if (capturecmd.Valid()) {
		cmd.printf("%s 2>/dev/null", capturecmd.SearchAndReplace("{file}", tempfile).str());
	}
	else Log("No valid capture command!");

	Log("Capture command '%s'", cmd.str());
	
	detcount = 0;

	{
		AString filename;
	
		maskimage.Delete();
		filename = GetSetting("maskimage");
		if (filename.Valid()) {
			if (maskimage.Load(filename)) {
				Log("Loaded mask image '%s'", filename.str());   
			}
			else {
				Log("Failed to load mask image '%s'", filename.str());   
			}
		}
	
		gainimage.Delete();
		filename = GetSetting("gainimage");
		if (filename.Valid()) {
			if (gainimage.Load(filename)) {
				Log("Loaded gain image '%s'", filename.str());   
			}
			else {
				Log("Failed to load gain image '%s'", filename.str());   
			}
		}
		gaindata.resize(0);
	}
	
	predetectionimages  = (uint_t)GetSetting("predetectionimages",  "2");
	postdetectionimages = (uint_t)GetSetting("postdetectionimages", "2");
	forcesavecount      = 0;
	
	matmul = 1.f;
	matwid = mathgt = 0;
	
	AString _matrix = GetSetting("matrix");
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

#if 1
		debug("Matrix is %u x %u:\n", matwid, mathgt);
		for (row = 0; row < nrows; row++) {
			for (col = 0; col < ncols; col++) debug("%8.3f", matrix[col + row * ncols]);
			debug("\n");
		}
		debug("Multiplier %0.6f\n", matmul);
#endif
	}
	else matrix.resize(0);
}

ImageDiffer::IMAGE *ImageDiffer::CreateImage(const char *filename, const IMAGE *img0)
{
	IMAGE *img = NULL;

	if ((img = new IMAGE) != NULL) {
		JPEG_INFO info;
		AImage& image = img->image;

		if (ReadJPEGInfo(filename, info) && image.LoadJPEG(filename)) {
			// apply mask immediately
			image *= maskimage;
			
			img->filename = filename;
			img->rect     = image.GetRect();
			img->saved    = false;

			if (!img0) {
				Log("New set of images size %dx%d", img->rect.w, img->rect.h);
			}
			else if (img0 && (img0->rect != img->rect)) {
				Log("Images are different sizes (%dx%d -> %dx%d), deleting image list",
					img0->rect.w, img0->rect.h, img->rect.w, img->rect.h);

				imglist.DeleteList();
			}
		}
		else {
			Log("Failed to load image '%s'", filename);
			delete img;
			img = NULL;
		}
	}

	return img;
}

void ImageDiffer::FindDifference(const IMAGE *img1, IMAGE *img2, std::vector<float>& difference)
{
	const AImage::PIXEL *pix1 = img1->image.GetPixelData();
	const AImage::PIXEL *pix2 = img2->image.GetPixelData();
	const ARect& rect = img1->rect;
	const uint_t w = rect.w, h = rect.h, len = w * h;
	std::vector<float> data;
	float *ptr, *p;
	double avg[3];
	uint_t x, y;

	difference.resize(len);
	data.resize(len * 3);
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
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			float val = 0.f;

			// apply matrix to data
			if (matwid && mathgt) {
				for (my = 0; my < mathgt; my++) {
					if (((y + my) >= cy) && ((y + my) < (h + cy))) {
						for (mx = 0; mx < matwid; mx++) {
							if (((x + mx) >= cx) && ((x + mx) < (w + cx))) {
								val += matrix[mx + my * matwid] * ptr[(x + mx - cx) + (y + my - cy) * w];
							}
							//else debug("x: ((%u + %u) >= %u) && ((%u + %u) < (%u + %u)) failed\n", x, mx, cx, x, mx, w, cx);
						}
					}
					//else debug("y: ((%u + %u) >= %u) && ((%u + %u) < (%u + %u)) failed\n", y, my, cy, y, my, h, cy);
				}

				val *= matmul;
			}
			// or just use original if no matrix
			else val = ptr[x + y * w];

			// store result in difference array
			difference[x + y * w] = (float)val;

			// update average and SD
			avg2 += val;
			sd2  += val * val;
		}
	}

	// calculate final average and SD
	avg2 /= (double)len;
	sd2   = sqrt(sd2 / (double)len - avg2 * avg2);

	img2->avg = avg2;
	img2->sd  = sd2;
}

void ImageDiffer::CalcLevel(IMAGE *img2, double avg, double sd, std::vector<float>& difference)
{
	// calculate minimum level based on average and SD values, individual levels must exceed this
	const uint_t len = img2->rect.w * img2->rect.h;
	double diff = avgfactor * avg + sdfactor * sd, level = 0.0;	
	uint_t i;
	
	// find level = sum of levels above minimum level
	for (i = 0; i < len; i++) {
		difference[i] = std::max(difference[i] - diff, 0.0);
		level += difference[i];
	}

	// divide by area of image and multiply up to make values arbitarily scaled
	level = level * 1000.0 / (double)len;

	img2->diff  = diff;
	img2->level = level;
}

void ImageDiffer::CreateDetectionImage(const IMAGE *img1, IMAGE *img2, const std::vector<float>& difference)
{
	const ARect& rect = img2->rect;
	
	// create detection image, if detection image directory valid
	if (img2->detimage.Create(rect.w, rect.h)) {
		const AImage::PIXEL *pixel1 = img1->image.GetPixelData();
		const AImage::PIXEL *pixel2 = img2->image.GetPixelData();
		AImage::PIXEL *pixel = img2->detimage.GetPixelData();
		const uint_t  len    = rect.w * rect.h;
		uint_t i;
		
		// use individual level from above and scale and max RGB values from original images
		for (i = 0; i < len; i++, pixel++, pixel1++, pixel2++) {
			pixel->r = (uint8_t)limit((double)std::max(pixel1->r, pixel2->r) * difference[i] / 255.0, 0.0, 255.0);
			pixel->g = (uint8_t)limit((double)std::max(pixel1->g, pixel2->g) * difference[i] / 255.0, 0.0, 255.0);
			pixel->b = (uint8_t)limit((double)std::max(pixel1->b, pixel2->b) * difference[i] / 255.0, 0.0, 255.0);
		}
	}
}

void ImageDiffer::Process(const ADateTime& dt)
{
	if (cmd.Valid() && (system(cmd) == 0)) {
		IMAGE *img;

		if ((img = CreateImage(tempfile, (const IMAGE *)imglist[imglist.Count() - 1])) != NULL) {
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
				std::vector<float> difference;

				// find difference between images
				FindDifference(img1, img2, difference);

				// filter values
				Interpolate(diffavg, img2->avg, coeff);
				Interpolate(diffsd,  img2->sd,  coeff);

				// store values in settings handler
				SetStat("avg", diffavg);
				SetStat("sd",  diffsd);

				CalcLevel(img2, diffavg, diffsd, difference);

				if (verbose) Log("Level = %0.1lf, (this frame = %0.3lf/%0.3lf, filtered = %0.3lf/%0.3lf, diff = %0.3lf)", img2->level, img2->avg, img2->sd, diffavg, diffsd, img2->diff);

				const double& level = img2->level;
				SetStat("level", level);

				if (detimgdir.Valid()) CreateDetectionImage(img1, img2, difference);

				// should image(s) be saved?
				if ((level >= threshold) || forcesavecount) {
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
					if		(level >= threshold) forcesavecount = postdetectionimages;
					// else decrement forcesavecount
					else if (forcesavecount)	 forcesavecount--;
				}

				if (level >= threshold) {
					// start if detection?
					if (!detcount && detstartcmd.Valid()) {
						AString cmd = detstartcmd.SearchAndReplace("{level}", AString("%0.4").Arg(level));
						if (system(cmd) != 0) {
							Log("Detection start command '%s' failed", cmd.str());
						}
					}

					// increment detection count
					detcount++;

					// run detection command
					if (detcmd.Valid()) {
						AString cmd = detcmd.SearchAndReplace("{level}", AString("%0.4").Arg(level)).SearchAndReplace("{detcount}", AString("%").Arg(detcount));
						if (system(cmd) != 0) {
							Log("Detection command '%s' failed", cmd.str());
						}
					}
				}
				else {
					// if there's been some detections, run detection end command
					if (detcount && detendcmd.Valid()) {
						AString cmd = detendcmd.SearchAndReplace("{level}", AString("%0.4").Arg(level)).SearchAndReplace("{detcount}", AString("%").Arg(detcount));
						if (system(cmd) != 0) {
							Log("Detection end command '%s' failed", cmd.str());
						}
					}

					// reset detection count
					detcount = 0;
						
					// if not a detection, run non-detection command
					if (nodetcmd.Valid()) {
						AString cmd = nodetcmd.SearchAndReplace("{level}", AString("%0.4").Arg(level));
						if (system(cmd) != 0) {
							Log("No-detection command '%s' failed", cmd.str());
						}
					}
				}
				
				// save detection data
				if ((level >= logthreshold) && detlogfmt.Valid()) {
					static AThreadLockObject tlock;
					AString  	filename = imagedir.CatPath(dt.DateFormat(detlogfmt));
					AStdFile 	fp;
					AThreadLock lock(tlock);
			
					CreateDirectory(filename.PathPart());
					if (fp.open(filename, "a")) {
						fp.printf("%s %u %0.16le %0.16le %0.16le %0.16le %0.16le %0.16le\n",
								  dt.DateFormat("%Y-%M-%D %h:%m:%s.%S").str(),
								  index,
								  img->avg,
								  img->sd,
								  img->diff,
								  img->level,
								  threshold,
								  logthreshold);
						fp.close();
					}
				}
			}
		}
	}
	else Log("Failed to fetch image using '%s'", cmd.str());
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
		if (detimgdir.Valid() && detimgfmt.Valid() && img->detimage.Valid()) {
			AString filename = detimgdir.CatPath(dt.DateFormat(detimgfmt) + ".jpg");
			CreateDirectory(filename.PathPart());
			img->detimage.SaveJPEG(filename, tags);
		}
		
		// save main image
		AString filename = imagedir.CatPath(dt.DateFormat(imagefmt) + ".jpg");
		CreateDirectory(filename.PathPart());
		Log("Saving detection image in '%s'", filename.str());
		img->image.SaveJPEG(filename, tags);

		// mark as saved
		img->saved = true;
	}
}

void *ImageDiffer::Run()
{
	uint64_t dt = (uint64_t)ADateTime();

	if (delay) {
		dt += delay - 1;
		dt -= dt % delay;
	}
	
	while (!quitthread) {
		uint64_t newdt = (uint64_t)ADateTime();
		uint64_t diff  = SUBZ(dt, newdt);

		if (diff) Sleep((uint32_t)diff);
		
		if (cmd.Valid()) Process(dt);

		dt += delay;
		
		CheckSettingsUpdate();

		uint_t newsettingscount = settingschangecount;
		if (newsettingscount != settingschange) {
			settingschange = newsettingscount;
			Log("Re-configuring");
			Configure();

			dt = (uint64_t)ADateTime();
			if (delay) {
				dt += delay - 1;
				dt -= dt % delay;
			}
		}
	}

	return NULL;
}

void ImageDiffer::Compare(const char *file1, const char *file2, const char *outfile)
{
	IMAGE *img1, *img2;

	if ((img1 = CreateImage(file1)) != NULL) {
		if ((img2 = CreateImage(file2)) != NULL) {
			std::vector<float> difference;

			// find difference between images
			FindDifference(img1, img2, difference);
			
			CalcLevel(img2, img2->avg, img2->sd, difference);
 
			printf("Level = %0.1lf, (this frame = %0.3lf/%0.3lf, diff = %0.3lf)\n", img2->level, img2->avg, img2->sd, img2->diff);

			if (outfile) {
				const TAG tags[] = {
					{AImage::TAG_JPEG_QUALITY, 95},
					{TAG_DONE, 0},
				};

				CreateDetectionImage(img1, img2, difference);

				if (img2->detimage.SaveJPEG(outfile, tags)) {
					debug("Saved detection image to '%s'\n", outfile);
				}
				else debug("Failed to save detection image to '%s'\n", outfile);
			}
			
			delete img2;
		}
		else debug("Failed to load image '%s'\n", file2);
		
		delete img1;
	}
	else debug("Failed to load image '%s'\n", file1);
}
