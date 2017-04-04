#ifndef COMMON_H
#define COMMON_H

// ============================================================================
// Some helper functions:
//    See: http://codereview.stackexchange.com/questions/419/converting-between-stdwstring-and-stdstring
// ============================================================================

namespace common {

	std::wstring s2ws(const std::string& s);
	std::string  ws2s(const std::wstring& s);
	std::string toStr(const char* str, ...);
	std::string GetTimeStr(const char* pattern);
	long long GetTimestamp();
	std::string GetTimestampStr();
	std::string GetCommonOutputPrefix();
	std::string GetCommonOutputDirectory();

	void Trigger(float startTimeAgo, float endTimeAgo, bool allowPendingSave = true);
	void SaveToDisk(bool save);

	typedef struct ImageBuffer {
		unsigned char* pBuffer;
		unsigned int nWidth;
		unsigned int nHeight;

		ImageBuffer() {
		}

		ImageBuffer(unsigned int w, unsigned int h) {
			nWidth = w;
			nHeight = h;
			pBuffer = (unsigned char*) malloc(nWidth * nHeight);
		}

		~ImageBuffer() {
			// Do we need ref counter logic?
			// delete[] pBuffer; 
		}

		std::string ToJPEG();
	};
}

#endif