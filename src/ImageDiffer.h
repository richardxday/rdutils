#ifndef __IMAGE_DIFFER__
#define __IMAGE_DIFFER__

#include <vector>

#include <rdlib/strsup.h>
#include <rdlib/DataList.h>
#include <rdlib/DateTime.h>
#include <rdlib/SettingsHandler.h>
#include <rdlib/BMPImage.h>

class ImageDiffer {
public:
	ImageDiffer(const ASettingsHandler& _settings, ASettingsHandler& _stats, AStdFile& _log, uint_t _index);
	~ImageDiffer();

	void Process(const ADateTime& dt, bool update);

	static void Delete(uptr_t item, void *context) {
		UNUSED(context);
		delete (ImageDiffer *)item;
	}

protected:
	typedef struct {
		AString	  filename;
		AImage	  image;
		AImage	  detimage;
		ARect	  rect;
		ADateTime dt;
		bool	  saved;
	} IMAGE;

	static void __DeleteImage(uptr_t item, void *context) {
		UNUSED(context);

		delete (IMAGE *)item;
	}

	IMAGE *CreateImage(AStdData& log, const char *filename, const IMAGE *img0);
	void SaveImage(IMAGE *img);

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
	AString					detcmd;
	AString					nodetcmd;
	AString					detstartcmd;
	AString					detendcmd;
	AImage					gainimage;
	std::vector<float>		gaindata;
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
	uint_t					predetectionimages;
	uint_t					postdetectionimages;
	uint_t					forcesavecount;
	uint_t					detcount;
	uint_t    			  	matwid, mathgt;
	uint_t				    subsample, sample;
	uint32_t				statswritetime;
	uint_t				    verbose;
};

#endif
