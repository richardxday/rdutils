#ifndef __IMAGE_DIFFER__
#define __IMAGE_DIFFER__

#include <vector>

#include <rdlib/strsup.h>
#include <rdlib/DataList.h>
#include <rdlib/DateTime.h>
#include <rdlib/SettingsHandler.h>
#include <rdlib/BMPImage.h>
#include <rdlib/Thread.h>
#include <rdlib/ThreadLock.h>

class ImageDiffer : public AThread {
public:
	ImageDiffer(uint_t _index);
	~ImageDiffer();

	static AString GetGlobalSetting(const AString& name, const AString& defval = "");

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

	IMAGE *CreateImage(const char *filename, const IMAGE *img0);
	void SaveImage(IMAGE *img);

	AString GetSetting(const AString& name, const AString& defval = "");
	void CheckSettingsUpdate();

	double  GetStat(const AString& name);
	void    SetStat(const AString& name, double val);
	
	void Configure();

	void Process();

	virtual void *Run();

	void Log(const char *fmt, ...);
	void Log(uint_t index, const char *fmt, va_list ap);

	class ProtectedSettings {
	public:
		ProtectedSettings(const AString& name, bool inhomedir = true, uint32_t iwritedelay = ~0) : settings(name, inhomedir, iwritedelay) {}
		~ProtectedSettings() {}

		ASettingsHandler& GetSettings() {return settings;}

		operator AThreadLockObject& () {return tlock;}
		
	protected:
		AThreadLockObject tlock;
		ASettingsHandler  settings;
	};

	static ProtectedSettings& GetSettings();
	static ProtectedSettings& GetStats();

protected:
	uint_t					index;
	ADataList				imglist;
	AString					logpath;
	uint_t					delay;
	AString   			  	wgetargs;
	AString   			  	cameraurl;
	AString   			  	videosrc;
	AString					streamerargs;
	AString					capturecmd;
	AString   			  	tempfile;
	AString					cmd;
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
	uint_t					settingschange;
	uint_t				    verbose;
	
	static uint_t			settingschangecount;
};

#endif
