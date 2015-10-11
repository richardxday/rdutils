
#include <errno.h>

#include <rdlib/StdMemFile.h>

#include "CameraStream.h"

CameraStream::CameraStream(ASocketServer *_server) : AHTTPRequest(_server),
													 stage(Stage_Idle),
													 imagehandler(NULL),
													 deleteimagehandler(false)
{
}

CameraStream::~CameraStream()
{
	stage = Stage_Done;
	Close();

	if (imagehandler && deleteimagehandler) delete imagehandler;
}

bool CameraStream::OpenHost(const AString& _host)
{
	camerahost = _host;
	stage      = Stage_CameraControl;
	
	return Open();
}

bool CameraStream::Open()
{
	AString _url;
	bool    success = false;
	
	switch (stage) {
		case Stage_CameraControl:
			_url.printf("%s/camera_control.cgi?loginuse=%s&loginpas=%s&param=0&value=0", camerahost.str(), username.str(), password.str());
			break;
			
		case Stage_Streaming:
			_url.printf("%s/videostream.cgi?user=%s&pwd=%s", camerahost.str(), username.str(), password.str());
			break;

		default:
			break;
	}

	if (_url.Valid()) {
		_url = _url.SearchAndReplace("{username}", username).SearchAndReplace("{password}", password);

		sstage  = StreamStage_Headers;
		success = AHTTPRequest::OpenURL(_url);

		if (success) SetReceiveBufferSize(32768);
	}

	return success;
}

void CameraStream::Cleanup()
{
	AHTTPRequest::Cleanup();

	content.resize(0);
	contentlength = 0;
	
	if ((++stage) < Stage_Done) Open();
}

void CameraStream::ProcessData()
{
	if (stage == Stage_Streaming) {
		debug("Data received %u bytes, content %u bytes\n", (uint_t)pos, (uint_t)content.size());
		
		while (pos) {
			if (sstage == StreamStage_ReadingData) {
				uint_t oldlen = content.size();
				uint_t newlen = MIN(oldlen + pos, contentlength);
				uint_t nbytes = newlen - oldlen;
				
				content.resize(newlen);
				memcpy(&content[oldlen], &data[0], nbytes);
				RemoveBytes(nbytes);

				if (content.size() >= contentlength) {
					ProcessContent();

					content.resize(0);
					contentlength = 0;
					sstage = StreamStage_LookingForBoundary;
				}
			}
			else {
				const uint_t maxlen = 200;
				AString header;

				if (GetHeader(header, maxlen)) {
					//debug("Received '%s'\n", header.str());
					
					ProcessHeader(header);
				}
				else if (pos >= maxlen) {
					debug("Received garbage, dumping...\n");

					RemoveBytes(maxlen);
				}
				else break;
			}
		}
	}
}

void CameraStream::ProcessHeader(const AString& header)
{
	switch (sstage) {
		case StreamStage_Headers:
			if (header.PosNoCase("Content-Type:") == 0) {
				AString lines = header.Mid(header.Pos(":") + 2);
				uint_t i, n = lines.CountLines(";");

				for (i = 0; i < n; i++) {
					AString line = lines.Line(i, ";").Words(0);
					if (line.PosNoCase("boundary=") == 0) {
						boundary = "--" + line.Mid(9);
						//debug("Boundary specified as '%s'\n", boundary.str());
						debug("Found boundary specification\n");
					}
				}
			}
			else if (header.Empty()) sstage++;
			break;

		case StreamStage_LookingForBoundary:
			if (header == boundary) {
				contentdt.TimeStamp();
				sstage++;
			}
			break;


		case StreamStage_LookingForContent:
			if (header.PosNoCase("Content-Length:") == 0) {
				contentlength = (uint_t)header.Mid(15);
				//debug("Data is %u bytes long\n", contentlength);
			}
			else if (header.Empty()) sstage++;
			break;
	}
}

void CameraStream::ProcessContent()
{
	AStdMemFile fp;

	//debug("Got %u bytes of content\n", (uint_t)content.size());
	if (fp.open("w")) {
		AImage image;

		fp.writebytes(&content[0], content.size());
		fp.rewind();

		if (image.LoadJPEG(fp)) {
			ProcessImage(image);
		}
		else debug("Failed to convert data to JPEG\n");
	}
}

void CameraStream::ProcessImage(const AImage& image)
{
	//debug("Got image %d x %d\n", image.GetRect().w, image.GetRect().h);
	if (imagehandler) imagehandler->ProcessImage(contentdt, image);
}
