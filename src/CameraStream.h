#ifndef __CAMERA_STREAM__
#define __CAMERA_STREAM__

#include <rdlib/HTTPRequest.h>
#include <rdlib/BMPImage.h>
#include <rdlib/DateTime.h>

class CameraStream : public AHTTPRequest {
public:
	CameraStream(ASocketServer *_server = NULL);
	virtual ~CameraStream();

	void SetUsernameAndPassword(const AString& _username, const AString& _password) {username = _username; password = _password;}
	
	virtual bool Open(const AString& _host);

	bool StreamComplete() const {return (stage >= Stage_Done);}

	class ImageHandler {
	public:
		ImageHandler() {}
		virtual ~ImageHandler() {}
		
		virtual void ProcessImage(const ADateTime& dt, const AImage& image) = 0;
	};

	virtual void SetImageHandler(ImageHandler *handler, bool del = false) {imagehandler = handler; deleteimagehandler = del;}
	
protected:
	virtual bool Open();
	virtual void Cleanup();
	virtual void ProcessData();
	virtual void ProcessHeader(const AString& header);
	virtual void ProcessContent();
	virtual void ProcessImage(const AImage& image);
							  
	enum {
		Stage_Idle = 0,
		Stage_CameraControl,
		Stage_Streaming,

		Stage_Done,
	};

	enum {
		StreamStage_Headers = 0,
		StreamStage_LookingForBoundary,
		StreamStage_LookingForContent,
		StreamStage_ReadingData,
	};
	
protected:
	AString 			 camerahost;
	AString 			 username;
	AString 			 password;
	AString 			 boundary;
	uint_t  			 stage;
	uint_t  			 sstage;
	uint_t  			 contentlength;
	ADateTime			 contentdt;
	std::vector<uint8_t> content;
	ImageHandler 		 *imagehandler;
	bool			     deleteimagehandler;
};

#endif
