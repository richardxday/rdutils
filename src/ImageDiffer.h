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

	void Compare(const char *file1, const char *file2, const char *outfile);
	
	static AString GetGlobalSetting(const AString& name, const AString& defval = "");

	static void Delete(uptr_t item, void *context) {
		UNUSED(context);
		delete (ImageDiffer *)item;
	}

	static uint_t DiffersRunning() {return differsrunning;}
	
protected:
	typedef struct {
		AString	  filename;
		AImage	  image;
		AImage	  detimage;
		ARect	  rect;
		ADateTime dt;
		double    avg;
		double    sd;
		double	  diff;
		double	  level;
		double	  rawlevel;
		double    fastavg;
		double    fastsd;
		double	  slowavg;
		double	  slowsd;
		bool	  saved;
		bool	  logged;
	} IMAGE;

	static void __DeleteImage(uptr_t item, void *context) {
		UNUSED(context);

		delete (IMAGE *)item;
	}

	IMAGE *CreateImage(const char *filename, const IMAGE *img0 = NULL);
	void SaveImage(IMAGE *img);
	void LogDetection(IMAGE *img);
	
	void FindDifference(const IMAGE *img1, IMAGE *img2, std::vector<double>& difference);
	void CalcLevel(IMAGE *img2, double avg, double sd, std::vector<double>& difference);
	void CreateDetectionImage(const IMAGE *img1, IMAGE *img2, const std::vector<double>& difference);

	bool SettingExists(const AString& name);
	AString GetSetting(const AString& name, const AString& defval = "");
	void CheckSettingsUpdate();

	double  GetStat(const AString& name);
	void    SetStat(const AString& name, double val);
	
	void Configure();

	AString CreateWGetCommand(const AString& url);
	AString CreateCaptureCommand();
	void Process(const ADateTime& dt);

	virtual void *Run();

	void Log(uint_t level, const char *fmt, ...);
	void Log(uint_t level, const char *fmt, va_list ap);

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
	AString					name;
	AString   			  	wgetargs;
	AString   			  	cameraurl;
	AString   			  	videosrc;
	AString					streamerargs;
	AString					capturecmd;
	AString   			  	tempfile;
	AString					cmd;
	AString   			  	imagedir;
	AString   			  	imagefmt;
	AString   			  	detlogfmt;
	AString   			  	detimgdir;
	AString   			  	detimgfmt;
	AString					detcmd;
	AString					nodetcmd;
	AString					detstartcmd;
	AString					detendcmd;
	AImage					maskimage;
	AImage					gainimage;
	AList					sourceimagelist;
	bool				    readingfromimagelist;
	std::vector<double>		gaindata;
	double    			  	fastattcoeff;
	double    			  	fastdeccoeff;
	double    			  	slowattcoeff;
	double    			  	slowdeccoeff;
	double    			  	fastavg;
	double    			  	fastsd;
	double    			  	slowavg;
	double    			  	slowsd;
	double    			  	avgfactor;
	double    			  	sdfactor;
	double    			  	redscale;
	double    			  	grnscale;
	double    			  	bluscale;
	double					diffgain;
	double					diffthreshold;
	double    			  	threshold;
	double    			  	logthreshold;
	std::vector<double>		matrix;
	std::vector<double>		previouslevels;
	size_t					previouslevelindex;
	uint_t					predetectionimages;
	uint_t					postdetectionimages;
	uint_t					forcesavecount;
	uint_t					detcount;
	uint_t    			  	matwid, mathgt;
	uint_t					settingschange;
	uint_t				    verbose;
	uint_t					verbose2;
	uint_t					timeout;
	bool					logdetections;
	bool					lastdetectionlogged;
	
	static uint_t			settingschangecount;
	static uint_t			differsrunning;
};

#endif
