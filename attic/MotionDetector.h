#ifndef __MOTION_DETECTOR__
#define __MOTION_DETECTOR__

#include <rdlib/SettingsHandler.h>

#include "CameraStream.h"

class MotionDetector : public CameraStream::ImageHandler {
public:
	MotionDetector(ASocketServer& _server, const ASettingsHandler& _settings, ASettingsHandler& _stats, AStdFile& _log, uint_t _index);
	virtual ~MotionDetector();

	void Configure();

	virtual void ProcessImage(const ADateTime& dt, const AImage& image);

	const CameraStream& GetStream() const {return stream;}
	
protected:
	AString GetSetting(const AString& name, const AString& def = "") const;
					   
protected:
	CameraStream		    stream;
	const ASettingsHandler& settings;
	ASettingsHandler&		stats;
	AStdFile&				log;
	uint_t					index;
	std::vector<AImage>		images;
	uint_t					imgindex;
	AString   			  	imagedir;
	AString   			  	imagefmt;
	AString   			  	detimgdir;
	AString   			  	detimgfmt;
	AString					detcmd;
	AString					nodetcmd;
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
	uint32_t				statswritetime;
	uint_t				    verbose;
};

#endif

