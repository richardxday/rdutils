
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <algorithm>

#include <rdlib/Recurse.h>
#include <rdlib/jpeginfo.h>

#include "ImageDiffer.h"

uint_t ImageDiffer::settingschangecount = 0;
uint_t ImageDiffer::differsrunning = 0;

ImageDiffer::ImageDiffer(uint_t _index) :
	AThread(),
	index(_index),
	settingschange(settingschangecount),
	verbose(0),
	imagenumber(0),
	savedimagenumber(0),
	seqno(0),
	lastdetectionlogged(false)
{
	imglist.SetDestructor(&__DeleteImage);

	GetStat("fastavg", fastavg);
	GetStat("fastsd", fastsd);
	GetStat("slowavg", slowavg);
	GetStat("slowsd", slowsd);

	Configure();

	Log(0, "New differ");

	differsrunning++;
}

ImageDiffer::~ImageDiffer()
{
	Log(0, "Shutting down");
	remove(tempfile);

	Stop();
}

void ImageDiffer::Log(uint_t level, const char *fmt, ...)
{
	if ((verbose >= level) || (verbose2 > level)) {
		va_list ap;
		va_start(ap, fmt);
		Log(level, fmt, ap);
		va_end(ap);
	}
}

void ImageDiffer::Log(uint_t level, const char *fmt, va_list ap)
{
	static AThreadLockObject loglock;
	ADateTime dt;
	AString   str;
	AString   filename = logpath.CatPath(AString("imagediff-%;.txt").Arg(dt.DateFormat("%Y-%M-%D").str()));
	AStdFile  fp;

	str.printf("%s[%u]: ", dt.DateFormat("%Y-%M-%D %h:%m:%s").str(), index);
	str.vprintf(fmt, ap);

	if (verbose >= level) {
		AThreadLock lock(loglock);
		CreateDirectory(filename.PathPart());
		if (fp.open(filename, "a")) {
			fp.printf("%s\n", str.str());
			fp.close();
		}
	}

	if (verbose2 > level) {
		printf("%s\n", str.str());
	}
}

ImageDiffer::ProtectedSettings& ImageDiffer::GetSettings()
{
	static ProtectedSettings _settings("imagediff", true, ~0);
	return _settings;
}

bool ImageDiffer::SettingExists(const AString& name) const
{
	ProtectedSettings& _settings = GetSettings();
	AThreadLock		   lock(_settings);
	ASettingsHandler&  settings = _settings.GetSettings();
	AString			   suffix   = AString(":%").Arg(index);
	return (settings.Exists(name + suffix) || settings.Exists(name));
}

AString ImageDiffer::GetSetting(const AString& name, const AString& defval) const
{
	ProtectedSettings& _settings = GetSettings();
	AThreadLock		   lock(_settings);
	ASettingsHandler&  settings = _settings.GetSettings();
	AString			   suffix   = AString(":%").Arg(index);
	AString			   str      = settings.Get(name + suffix, settings.Get(name, defval));
	int p = 0, p1, p2;

	while (((p1 = str.Pos("{", p)) >= 0) && ((p2 = str.Pos("}", p1 + 1)) >= 0)) {
		AString str1 = str.Left(p1);
		AString str2 = str.Mid(p1 + 1, p2 - p1 - 1);
		AString str3 = str.Mid(p2 + 1);

		if (SettingExists(str2)) {
			str  = str1 + GetSetting(str2);
			p    = str.len();
			str += str3;
		}
		else p = p2 + 1;
	}

	return str;
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

AString ImageDiffer::GetStat(const AString& name)
{
	ProtectedSettings& _stats = GetStats();
	AThreadLock		   lock(_stats);
	ASettingsHandler&  stats  = _stats.GetSettings();
	AString			   suffix = AString(":%").Arg(index);

	return stats.Get(name + suffix);
}

void ImageDiffer::SetStat(const AString& name, double val)
{
	ProtectedSettings& _stats = GetStats();
	AThreadLock		   lock(_stats);
	ASettingsHandler&  stats  = _stats.GetSettings();
	AString			   suffix = AString(":%").Arg(index);

	stats.Set(name + suffix, AString("%0.16e").Arg(val));
}

void ImageDiffer::SetStat(const AString& name, uint_t val)
{
	ProtectedSettings& _stats = GetStats();
	AThreadLock		   lock(_stats);
	ASettingsHandler&  stats  = _stats.GetSettings();
	AString			   suffix = AString(":%").Arg(index);

	stats.Set(name + suffix, AString("%").Arg(val));
}

AString ImageDiffer::CreateWGetCommand(const AString& url)
{
	AString cmd;

	cmd.printf("timeout %u wget %s \"%s\" 2>/dev/null", timeout, wgetargs.str(), url.str());

	return cmd;
}

AString ImageDiffer::CreateCaptureCommand()
{
	AString cmd;

	if (cameraurl.Valid()) {
		cmd.printf("%s -O %s", CreateWGetCommand(cameraurl).str(), tempfile.str());
	}
	else if (videosrc.Valid()) {
		cmd.printf("streamer -c /dev/%s %s 2>/dev/null -o %s", videosrc.str(), streamerargs.str(), tempfile.str());
	}
	else if (capturecmd.Valid()) {
		cmd.printf("%s 2>/dev/null", capturecmd.SearchAndReplace("{file}", tempfile).str());
	}
	else Log(0, "No valid capture command!");

	return cmd;
}

AString ImageDiffer::FindFile(const AString& filename) const
{
	if (AStdFile::exists(filename)) return filename;
	
	AString dirs = GetSetting("filedirs", "");
	if (dirs.Valid()) dirs += ";";
	dirs += GetSettings().GetSettings().GetFilename().PathPart();

	uint_t i, n = dirs.CountLines(";");
	for (i = 0; i < n; i++) {
		AString dir       = dirs.Line(i, ";");
		AString filename2 = dir.CatPath(filename);

		//debug("Looking for '%s' in '%s'\n", filename.str(), dir.str());
		if (dir.Valid() && AStdFile::exists(filename2)) {
			//debug("Found '%s' in '%s' as '%s'\n", filename.str(), dir.str(), filename2.str());
			return filename2;	
		}
	}

	return "";
}

void ImageDiffer::Configure()
{
	AString indexstr = AString("%").Arg(index);
	verbose   	  = (uint_t)GetSetting("verbose",      "0");
	verbose2  	  = (uint_t)GetSetting("verbose2",     "0");
	timeout       = (uint_t)GetSetting("timeout",	  "20");

	logpath       = GetSetting("loglocation", "/var/log/imagediff");
	name          = GetSetting("name");
	delay         = (uint_t)(1000.0 * (double)GetSetting("delay", "1"));
	wgetargs  	  = GetSetting("wgetargs");
	cameraurl     = GetSetting("cameraurl");
	videosrc      = GetSetting("videosrc");
	streamerargs  = GetSetting("streamerargs", "-s 640x480");
	capturecmd    = GetSetting("capturecmd").DeEscapify();
	tempfile  	  = (GetSetting("tempfile", AString("/home/%/temp-{index}.jpeg").Arg(getenv("LOGNAME"))).
					 SearchAndReplace("{index}", indexstr));
	imagedir  	  = (GetSetting("imagedir", "/media/cctv").
					 SearchAndReplace("{name}",      name.Valid() ? name : "{index}").
					 SearchAndReplace("{index}",     indexstr));
	imagefmt  	  = (GetSetting("filename", "%Y-%M-%D-{name}/%h/Image-%Y-%M-%D-%h-%m-%s-%S-seq{seq}").
					 SearchAndReplace("{name}",      name.Valid() ? name : "{index}").
					 SearchAndReplace("{index}",     indexstr));
	detlogfmt 	  = (GetSetting("detlogfilename", "detection.dat").
					 SearchAndReplace("{imagepath}", imagefmt.PathPart()).
					 SearchAndReplace("{imagename}", imagefmt.FilePart()).
					 SearchAndReplace("{name}",  	 name.Valid() ? name : "{index}").
					 SearchAndReplace("{index}", 	 indexstr));
	detimgdir 	  = (GetSetting("detimagedir").
					 SearchAndReplace("{imagedir}",  imagedir).
					 SearchAndReplace("{name}",  	 name.Valid() ? name : "{index}").
					 SearchAndReplace("{index}",     indexstr));
	detimgfmt 	  = (GetSetting("detfilename", "{imagepath}/detection/{imagename}").
					 SearchAndReplace("{imagepath}", imagefmt.PathPart()).
					 SearchAndReplace("{imagename}", imagefmt.FilePart()).
					 SearchAndReplace("{name}",  	 name.Valid() ? name : "{index}").
					 SearchAndReplace("{index}", 	 indexstr));
	detcmd    	  = GetSetting("detcommand").SearchAndReplace("{index}", indexstr);
	detstartcmd   = GetSetting("detstartcommand").SearchAndReplace("{index}", indexstr);
	detendcmd     = GetSetting("detendcommand").SearchAndReplace("{index}", indexstr);
	nodetcmd  	  = GetSetting("nodetcommand").SearchAndReplace("{index}", indexstr);
	logdetections = ((uint_t)GetSetting("logdetections", "0") != 0);
	
	sourceimagelist.DeleteAll();
	AString imgdir = GetSetting("imagesourcedir");
	if (imgdir.Valid()) {
		extern AQuitHandler quithandler;

		Log(0, "Finding files in '%s'...", imgdir.str());
		CollectFiles(imgdir, "*.jpg", RECURSE_ALL_SUBDIRS, sourceimagelist, FILE_FLAG_IS_DIR, 0, &quithandler);
		Log(0, "Found %u files in '%s'", sourceimagelist.Count(), imgdir.str());
	}
	readingfromimagelist = (sourceimagelist.Count() > 0);

	fastattcoeff  = (double)GetSetting("fastattcoeff", 	  "1.0e-1");
	fastdeccoeff  = (double)GetSetting("fastdeccoeff", 	  "1.5e-1");
	slowattcoeff  = (double)GetSetting("slowattcoeff", 	  "1.0e-2");
	slowdeccoeff  = (double)GetSetting("slowdeccoeff", 	  "1.5e-2");
	avgfactor 	  = (double)GetSetting("avgfactor", 	  "1.0");
	sdfactor  	  = (double)GetSetting("sdfactor",  	  "2.0");
	redscale  	  = (double)GetSetting("rscale",    	  "1.0");
	grnscale  	  = (double)GetSetting("gscale",    	  "1.0");
	bluscale  	  = (double)GetSetting("bscale",    	  "1.0");
	diffgain      = (double)GetSetting("diffmul",		  "1.0") / (double)GetSetting("diffdiv", "1.0");
	diffthreshold = (double)GetSetting("diffthreshold",   ".25");
	threshold 	  = (double)GetSetting("threshold", 	  "3000.0");
	logthreshold  = (double)GetSetting("logthreshold", "{threshold}").SearchAndReplace("{threshold}", GetSetting("threshold", "3000.0"));

	GetStat("seqno", seqno);
	
	Log(0, "Destination '%s'", imagedir.CatPath(imagefmt).str());
	if (detimgdir.Valid()) Log(0, "Detection files destination '%s'", detimgdir.CatPath(detimgfmt).str());
	if (detlogfmt.Valid()) Log(0, "Detection log '%s' with threshold %0.1lf", imagedir.CatPath(detlogfmt).str(), logthreshold);

	cmd = CreateCaptureCommand();

	Log(0, "Capture command '%s'", cmd.str());

	detcount = 0;

	{
		AString filename;

		maskimage.Delete();
		filename = GetSetting("maskimage");
		if (filename.Valid()) {
			AString filename2;
			if ((filename2 = FindFile(filename)).Valid()) {
				if (maskimage.Load(filename2)) {
					Log(0, "Loaded mask image '%s'", filename2.str());
				}
				else {
					Log(0, "Failed to load mask image '%s'", filename2.str());
				}
			}
			else {
				Log(0, "Failed to find mask image '%s'", filename.str());
			}
		}

		gainimage.Delete();
		filename = GetSetting("gainimage");
		if (filename.Valid()) {
			AString filename2;
			
			if ((filename2 = FindFile(filename)).Valid()) {
				if (gainimage.Load(filename2)) {
					Log(0, "Loaded gain image '%s'", filename2.str());
				}
				else {
					Log(0, "Failed to load gain image '%s'", filename2.str());
				}
			}
			else {
				Log(0, "Failed to find gain image '%s'", filename.str());
			}
		}
		gaindata.resize(0);
	}

	predetectionimages  = (uint_t)GetSetting("predetectionimages",  "2");
	postdetectionimages = (uint_t)GetSetting("postdetectionimages", "2");
	forcesavecount      = 0;

	matwid = mathgt = 0;

	AString _matrix = GetSetting("matrix");
	if (_matrix.Valid()) {
		uint_t row, nrows = _matrix.CountLines(";");
		uint_t col, ncols = 1;

		for (row = 0; row < nrows; row++) {
			uint_t n = _matrix.Line(row, ";").CountLines(",");
			ncols = std::max(ncols, n);
		}

		nrows |= 1;
		ncols |= 1;

		matrix.resize(nrows * ncols);
		for (row = 0; row < nrows; row++) {
			AString line = _matrix.Line(row,";");

			for (col = 0; col < ncols; col++) matrix[col + row * ncols] = (double)line.Line(col, ",");
		}

		matwid = ncols;
		mathgt = nrows;

#if 0
		debug("Matrix is %u x %u:\n", matwid, mathgt);
		for (row = 0; row < nrows; row++) {
			for (col = 0; col < ncols; col++) debug("%8.3lf", matrix[col + row * ncols]);
			debug("\n");
		}
#endif
	}
	else matrix.resize(0);

	previouslevels.resize(10);
	previouslevelindex = 0;

	if (!readingfromimagelist && cameraurl.Valid()) {
		AString str;
		uint_t i;

		for (i = 1; (str = AString("camerapreurl[%]").Arg(i)).Valid() && SettingExists(str); i++) {
			AString cmd;

			cmd.printf("%s -O /dev/null", CreateWGetCommand(GetSetting(str)).str());

			if (system(cmd) == 0) {
				Log(0, "Ran '%s' successfully", cmd.str());
			}
		}
	}
}

ImageDiffer::IMAGE *ImageDiffer::CreateImage(const char *filename, const IMAGE *img0)
{
	IMAGE *img = NULL;

	if ((img = new IMAGE) != NULL) {
		JPEG_INFO info;
		AImage& image = img->image;

		if (ReadJPEGInfo(filename, info) && image.LoadJPEG(filename)) {
			// apply mask immediately
			image            *= maskimage;

			img->filename     = filename;
			img->rect         = image.GetRect();
			img->saved        = false;
			img->logged       = false;
			img->imagenumber  = imagenumber++;
			
			if (!img0) {
				Log(0, "New set of images size %dx%d", img->rect.w, img->rect.h);
			}
			else if (img0 && (img0->rect != img->rect)) {
				Log(0, "Images are different sizes (%dx%d -> %dx%d), deleting image list",
					img0->rect.w, img0->rect.h, img->rect.w, img->rect.h);

				imglist.DeleteList();
			}
		}
		else {
			Log(0, "Failed to load image '%s'", filename);
			delete img;
			img = NULL;
		}
	}

	return img;
}

void ImageDiffer::FindDifference(const IMAGE *img1, IMAGE *img2, std::vector<double>& difference)
{
	const AImage::PIXEL *pix1 = img1->image.GetPixelData();
	const AImage::PIXEL *pix2 = img2->image.GetPixelData();
	const ARect& rect = img1->rect;
	const uint_t w = rect.w, h = rect.h, len = w * h;
	std::vector<double> data;
	double  *p;
	double avg[3];
	uint_t x, y;

	difference.resize(len);
	data.resize(len * 3);

	memset(avg, 0, sizeof(avg));

	// find difference between the two images and
	// normalize against average on each line per component
	for (y = 0, p = &data[0]; y < h; y++) {
		double yavg[3];

		// reset average
		memset(yavg, 0, sizeof(yavg));

		// subtract pixels and update per-component average
		for (x = 0; x < w; x++, p += 3, pix1++, pix2++) {
			p[0]     = ((double)pix1->r - (double)pix2->r) * redscale;
			p[1]     = ((double)pix1->g - (double)pix2->g) * grnscale;
			p[2]     = ((double)pix1->b - (double)pix2->b) * bluscale;

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

			gaindata.resize(n);

			for (y = 0; y < h; y++) {
				// convert detection y into gainimage y
				const uint_t y2 = (y * gainhgt + h / 2) / h;

				for (x = 0; x < w; x++) {
					// convert detection x into gainimage x
					const uint_t x2 = (x * gainwid + w / 2) / w;
					// get ptr to pixel data
					const AImage::PIXEL *p = gainptr + x2 + y2 * gainwid;

					// convert pixel into RGB doubleing point values 0-1
					gaindata[i++] = (double)p->r / 255.f;
					gaindata[i++] = (double)p->g / 255.f;
					gaindata[i++] = (double)p->b / 255.f;
				}
			}
		}
	}

	// subtract overall average from pixel data, scale by gain image and
	// calculate modulus
	double *p2, *p3;
	for (y = 0, p = &data[0], p2 = &data[0], p3 = &gaindata[0]; y < h; y++) {
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
	double maxdifference = 0.0;
	uint_t mx, my, cx = (matwid - 1) >> 1, cy = (mathgt - 1) >> 1;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			double val = 0.0;

			// apply matrix to data
			if (matwid && mathgt) {
				for (my = 0; my < mathgt; my++) {
					if (((y + my) >= cy) && ((y + my) < (h + cy))) {
						for (mx = 0; mx < matwid; mx++) {
							if (((x + mx) >= cx) && ((x + mx) < (w + cx))) {
								val += matrix[mx + my * matwid] * data[(x + mx - cx) + (y + my - cy) * w];
							}
							//else debug("x: ((%u + %u) >= %u) && ((%u + %u) < (%u + %u)) failed\n", x, mx, cx, x, mx, w, cx);
						}
					}
					//else debug("y: ((%u + %u) >= %u) && ((%u + %u) < (%u + %u)) failed\n", y, my, cy, y, my, h, cy);
				}
			}
			// or just use original if no matrix
			else val = data[x + y * w];

			val *= diffgain;

			// store result in difference array
			difference[x + y * w] = val;

			// find maximum difference
			maxdifference = std::max(maxdifference, val);
		}
	}

	double rawlevel = 0.0;
	double thres    = diffthreshold * maxdifference;
	uint_t n = 0;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			double val = difference[x + y * w];

			rawlevel += val;
			if (val >= thres) {
				// update average and SD
				avg2 += val;
				sd2  += val * val;
				n++;
			}
		}
	}

	// calculate final average and SD
	avg2 /= (double)n;
	sd2   = sqrt(sd2 / (double)n - avg2 * avg2);

	img2->avg 	   = avg2;
	img2->sd  	   = sd2;
	img2->rawlevel = rawlevel / (double)len;
	img2->diff     = 0.0;
}

void ImageDiffer::CalcLevel(IMAGE *img2, double avg, double sd, std::vector<double>& difference)
{
	// calculate minimum level based on average and SD values, individual levels must exceed this
	const uint_t len = img2->rect.w * img2->rect.h;
	double diff = avgfactor * avg + sdfactor * sd, level = 0.0, rawlevel = 0.0;
	uint_t i;

	// find level = sum of levels above minimum level
	for (i = 0; i < len; i++) {
		rawlevel += difference[i];
		difference[i] = std::max(difference[i] - diff, 0.0);
		level += difference[i];
	}

	// divide by area of image and multiply up to make values arbitarily scaled
	rawlevel /= (double)len;
	level     = level * 1000.0 / (double)len;

	img2->diff     = diff;
	img2->level    = level;
	img2->rawlevel = rawlevel;
}

void ImageDiffer::CreateDetectionImage(const IMAGE *img1, IMAGE *img2, const std::vector<double>& difference)
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
	AListNode *node;
	AString imgfile;

	if (readingfromimagelist && ((node = sourceimagelist.Pop()) != NULL)) {
		AString *str = AString::Cast(node);

		if (str) {
			imgfile = *str;
			Log(2, "Using image '%s'", imgfile.str());
		}

		delete node;
	}
	else if (cmd.Valid() && (system(cmd) == 0)) {
		imgfile = tempfile;
	}

	if (imgfile.Valid()) {
		IMAGE *img;

		if ((img = CreateImage(imgfile, (const IMAGE *)imglist[imglist.Count() - 1])) != NULL) {
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
				std::vector<double> difference;

				// find difference between images
				FindDifference(img1, img2, difference);

				// filter values
				if (img2->avg >= fastavg) fastavg += (img2->avg - fastavg) * fastattcoeff;
				else					  fastavg += (img2->avg - fastavg) * fastdeccoeff;
				if (img2->sd  >= fastsd)  fastsd  += (img2->sd 	- fastsd) * fastattcoeff;
				else					  fastsd  += (img2->sd 	- fastsd) * fastdeccoeff;
				if (img2->avg >= slowavg) slowavg += (img2->avg - slowavg) * slowattcoeff;
				else					  slowavg += (img2->avg - slowavg) * slowdeccoeff;
				if (img2->sd  >= slowsd)  slowsd  += (img2->sd 	- slowsd) * slowattcoeff;
				else					  slowsd  += (img2->sd 	- slowsd) * slowdeccoeff;
				slowavg = std::min(slowavg, fastavg);
				slowsd  = std::min(slowsd,  fastsd);
				
				img2->fastavg = fastavg;
				img2->fastsd  = fastsd;
				img2->slowavg = slowavg;
				img2->slowsd  = slowsd;
				
				// store values in settings handler
				SetStat("fastavg", fastavg);
				SetStat("fastsd",  fastsd);
				SetStat("slowavg", slowavg);
				SetStat("slowsd",  slowsd);

				//CalcLevel(img2, std::max(fastavg - slowavg, 0.0), slowsd, difference);

				img2->level = fastavg - avgfactor * slowavg - sdfactor * slowsd;

				const double& level = img2->level;
				Log(1, "Level = %0.1lf, (rawlevel = %0.1lf, this frame = %0.3lf/%0.3lf, fast = %0.3lf/%0.3lf, slow = %0.3lf/%0.3lf, diff = %0.3lf)",
					level,
					img2->rawlevel,
					img2->avg,
					img2->sd,
					fastavg,
					fastsd,
					slowavg,
					slowsd,
					img2->diff);

				SetStat("level", level);

				previouslevels[previouslevelindex] = level;
				if ((++previouslevelindex) == previouslevels.size()) previouslevelindex = 0;

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
						if (logdetections || (logthreshold <= threshold)) {
							LogDetection((IMAGE *)imglist[i]);
						}
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
							Log(0, "Detection start command '%s' failed", cmd.str());
						}
					}

					// increment detection count
					detcount++;

					// run detection command
					if (detcmd.Valid()) {
						AString cmd = detcmd.SearchAndReplace("{level}", AString("%0.4").Arg(level)).SearchAndReplace("{detcount}", AString("%").Arg(detcount));
						if (system(cmd) != 0) {
							Log(0, "Detection command '%s' failed", cmd.str());
						}
					}
				}
				else {
					// if there's been some detections, run detection end command
					if (detcount && detendcmd.Valid()) {
						AString cmd = detendcmd.SearchAndReplace("{level}", AString("%0.4").Arg(level)).SearchAndReplace("{detcount}", AString("%").Arg(detcount));
						if (system(cmd) != 0) {
							Log(0, "Detection end command '%s' failed", cmd.str());
						}
					}

					// reset detection count
					detcount = 0;

					// if not a detection, run non-detection command
					if (nodetcmd.Valid()) {
						AString cmd = nodetcmd.SearchAndReplace("{level}", AString("%0.4").Arg(level));
						if (system(cmd) != 0) {
							Log(0, "No-detection command '%s' failed", cmd.str());
						}
					}
				}

				// save detection data
				if (level >= logthreshold) {
					LogDetection(img2);
				}

				lastdetectionlogged = img2->logged;
			}
		}
	}
	else Log(0, "Failed to fetch image using '%s'", cmd.str());
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
		FILE_INFO info;

		// detect if any images *haven't* been saved
		if ((img->imagenumber - savedimagenumber) > 1) {
			// images not saved -> increment sequence number
			seqno = (seqno + 1) % 1000000000;
			SetStat("seqno", seqno);
			Log(0, "New sequence %09u", seqno);
		}

		// generate sequence number string for filenames
		AString seqstr = AString("%09").Arg(seqno);
		
		// save detection image, if possible
		if (detimgdir.Valid() && detimgfmt.Valid() && img->detimage.Valid()) {
			AString& filename = img->savedetfilename;
			
			filename = detimgdir.CatPath(dt.DateFormat(detimgfmt).SearchAndReplace("{seq}", seqstr) + ".jpg");

			AString dir = filename.PathPart();
			if (!GetFileInfo(dir, &info) && !CreateDirectory(dir)) {
				Log(0, "Failed to create directory '%s'", dir.str());
			}

			img->detimage.SaveJPEG(filename, tags);
		}

		// save main image
		AString& filename = img->savefilename;
		
		filename = imagedir.CatPath(dt.DateFormat(imagefmt).SearchAndReplace("{seq}", seqstr) + ".jpg");
		
		AString dir = filename.PathPart();
		if (!GetFileInfo(dir, &info) && !CreateDirectory(dir)) {
			Log(0, "Failed to create directory '%s'", dir.str());
		}

		Log(1, "Saving detection image in '%s'", filename.str());
		if (!img->image.SaveJPEG(filename, tags)) {
			Log(0, "Failed to save detection image in '%s'", filename.str());
		}

		// mark as saved
		img->saved = true;

		// update last saved image number
		savedimagenumber = img->imagenumber;
	}
}

void ImageDiffer::LogDetection(IMAGE *img)
{
	if (detlogfmt.Valid() && !img->logged) {
		static AThreadLockObject tlock;
		AString  	filename = imagedir.CatPath(img->dt.DateFormat(detlogfmt));
		AStdFile 	fp;
		AThreadLock lock(tlock);

		CreateDirectory(filename.PathPart());
		if (fp.open(filename, "a")) {
			if (!lastdetectionlogged) fp.printf("\n");
						
			fp.printf("%s %u %0.16le %0.16le %0.16le %0.16le %0.16le %0.16le %0.16le %0.16le %0.16le %0.16le %0.16le '%s' '%s'\n",
					  /*  1,2 */ img->dt.DateFormat("%Y-%M-%D %h:%m:%s.%S").str(),
					  /*  3 */ index,
					  /*  4 */ img->avg,
					  /*  5 */ img->sd,
					  /*  6 */ img->fastavg,
					  /*  7 */ img->fastsd,
					  /*  8 */ img->slowavg,
					  /*  9 */ img->slowsd,
					  /* 10 */ img->diff,
					  /* 11 */ img->level,
					  /* 12 */ img->rawlevel,
					  /* 13 */ threshold,
					  /* 14 */ logthreshold,
					  /* 15 */ img->savefilename.str(),
					  /* 16 */ img->savedetfilename.str());
			fp.close();

			lastdetectionlogged = true;
			img->logged = true;
		}
	}
}

void *ImageDiffer::Run()
{
	uint64_t dt = (uint64_t)ADateTime();

	while (!quitthread &&
		   (!readingfromimagelist || (sourceimagelist.Count() > 0))) {
		uint64_t newdt = (uint64_t)ADateTime();
		uint32_t lag   = (uint32_t)SUBZ(newdt, dt), diff;

		if (lag >= (2 * delay)) {
			Log(0, "Lag:%ums", lag);
		}

		SetStat("lag", lag);

		newdt = (uint64_t)ADateTime();
		diff  = (uint32_t)SUBZ(dt, newdt);
		if (diff) Sleep(diff);

		if (cmd.Valid()) Process(dt);

		dt += delay;

		CheckSettingsUpdate();

		uint_t newsettingscount = settingschangecount;
		if (newsettingscount != settingschange) {
			settingschange = newsettingscount;
			Log(0, "Re-configuring");
			Configure();

			dt = (uint64_t)ADateTime();
		}
	}

	differsrunning--;

	return NULL;
}

void ImageDiffer::Compare(const char *file1, const char *file2, const char *outfile)
{
	IMAGE *img1, *img2;

	if ((img1 = CreateImage(file1)) != NULL) {
		if ((img2 = CreateImage(file2)) != NULL) {
			std::vector<double> difference;

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
