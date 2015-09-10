
#include <rdlib/Recurse.h>

#include "MotionDetector.h"

MotionDetector::MotionDetector(ASocketServer& _server,
							   const ASettingsHandler& _settings,
							   ASettingsHandler& _stats,
							   AStdFile& _log,
							   uint_t _index) : ImageHandler(),
												stream(&_server),
												settings(_settings),
												stats(_stats),
												log(_log),
												index(_index),
												images(4),
												imgindex(0),
												verbose(0)
{
	Configure();

	stream.SetImageHandler(this);
	
	diffavg = (double)stats.Get(AString("avg%u").Arg(index), "0.0");
	diffsd  = (double)stats.Get(AString("sd%u").Arg(index),  "0.0");

	log.printf("%s[%u]: New detector\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s.%S").str(), index);
}

MotionDetector::~MotionDetector()
{
	log.printf("%s[%u]: Shutting down\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s.%S").str(), index);
}

AString MotionDetector::GetSetting(const AString& name, const AString& def) const
{
	AString nstr = AString("%u").Arg(index);	
	return settings.Get(nstr + ":" + name, settings.Get(name, def)).SearchAndReplace("{camera}", nstr);
}

void MotionDetector::Configure()
{
	AString nstr = AString("%u").Arg(index);
	
	log.printf("%s[%u]: Reading new settings...\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s.%S").str(), index);

	stream.Close();
	stream.SetUsernameAndPassword(GetSetting("username", "admin"),
								  GetSetting("password", "arsebark"));

	AString camera = GetSetting("camera", "");
	log.printf("%s[%u]: Connecting to %s...\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s.%S").str(), index, camera.str());
	if (!stream.Open(camera)) {
		log.printf("%s[%u]: Failed to connect to %s...\n", ADateTime().DateFormat("%Y-%M-%D %h:%m:%s.%S").str(), index, camera.str());
	}
	
	imagedir  = GetSetting("imagedir", "/media/cctv");
	imagefmt  = GetSetting("filename", "%Y-%M-%D/%h/Image-{camera}-%Y-%M-%D-%h-%m-%s-%S");
	detimgdir = GetSetting("detimagedir");
	detimgfmt = GetSetting("detfilename", "%Y-%M-%D/%h/detection/Image-{camera}-%Y-%M-%D-%h-%m-%s-%S");
	detcmd    = GetSetting("detcommand", "");
	nodetcmd  = GetSetting("nodetcommand", "");
	coeff  	  = (double)GetSetting("coeff", "1.0e-3");
	avgfactor = (double)GetSetting("avgfactor", "1.0");
	sdfactor  = (double)GetSetting("sdfactor", "2.0");
	redscale  = (double)GetSetting("rscale", "1.0");
	grnscale  = (double)GetSetting("gscale", "1.0");
	bluscale  = (double)GetSetting("bscale", "1.0");
	threshold = (double)GetSetting("threshold", "3000.0");
	verbose   = (uint_t)GetSetting("verbose", "0");

	matmul 	  = 1.f;
	matwid 	  = mathgt = 0;

	AString _matrix = GetSetting("matrix", "");
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

void MotionDetector::ProcessImage(const ADateTime& dt, const AImage& image)
{
	AString datestr = dt.DateFormat("%Y-%M-%D %h:%m:%s.%S");
	
	images[imgindex] = image;

	AImage& image1 = images[(imgindex + images.size() - 1) % images.size()];
	AImage& image2 = images[imgindex];
	imgindex = (imgindex + 1) % images.size();

	if ((image1.GetRect() == image2.GetRect()) && (image2 != image1)) {
		const AImage::PIXEL *pix1 = image1.GetPixelData();
		const AImage::PIXEL *pix2 = image2.GetPixelData();
		const ARect& rect = image1.GetRect();
		std::vector<float> data;
		float *ptr, *p;
		double avg[3];
		uint_t x, y, w = rect.w, h = rect.h, w3 = w * 3, len = w * h, len3 = w3 * h;
				
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

		if (verbose) {
			log.printf("%s[%u]: Level = %0.1lf (diff = %0.6lf)\n",
					   datestr.str(), index, total, diff);
		}
		
		stats.Set(AString("level%u").Arg(index), AString("%0.4lf").Arg(total));

		if (total >= threshold) {
			static const TAG tags[] = {
				{AImage::TAG_JPEG_QUALITY, 95},
				{TAG_DONE, 0},
			};

			if (detimgdir.Valid()) {
				AImage img;
				if (img.Create(rect.w, rect.h)) {
					const AImage::PIXEL *pixel1 = image1.GetPixelData();
					const AImage::PIXEL *pixel2 = image2.GetPixelData();
					AImage::PIXEL       *pixel  = img.GetPixelData();

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
			log.printf("%s[%u]: Saving detection image in '%s'\n", datestr.str(), index, filename.str());
			image2.SaveJPEG(filename, tags);

			if (detcmd.Valid()) {
				AString cmd = detcmd.SearchAndReplace("{level}", AString("%0.4lf").Arg(total));
				if (system(cmd) != 0) {
					log.printf("%s[%u]: Detection command '%s' failed\n", datestr.str(), index, cmd.str());
				}
			}
		}
		else if (nodetcmd.Valid()) {
			AString cmd = nodetcmd.SearchAndReplace("{level}", AString("%0.4lf").Arg(total));
			if (system(cmd) != 0) {
				log.printf("%s[%u]: No-detection command '%s' failed\n", datestr.str(), index, cmd.str());
			}
		}
	}
	//else debug("Images are different sizes or identical\n");
}
