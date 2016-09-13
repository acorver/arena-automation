#pragma once

#define WIN32_LEAN_AND_MEAN 
#include <boost/thread.hpp> 
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "lib/boost/numeric/ublas/vector.hpp"
#include "lib/boost/numeric/ublas/vector_expression.hpp"
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/atomic.hpp>

//#include <boost/gil/gil_all.hpp>

#include <stdio.h>

#include "lib/crow/crow.h"

#include <windows.h>
#include <algorithm>
#include <vector>
#include <queue>
#include <deque>
#include <map>
#include <fstream>
#include <climits>
#include <string>
#include <stdarg.h>

#include "lib/nanoflann/nanoflann.hpp"

#include "lib/tpl/tpl.h"

#include "lib/jpeg-compressor/jpge.h"

#include "log.h"

#include "lib/serial/SerialClass.h"

#include "lib/cortex/Cortex.h"

#include "lib/photron/PDCLIB.h"


#include "lib/opencv3/include/opencv2/opencv.hpp"
#include "lib/opencv3/include/opencv2/core/core.hpp"
#include "lib/opencv3/include/opencv2/imgproc/imgproc.hpp"
#include "lib/opencv3/include/opencv2/core/operations.hpp"
#include "lib/opencv3/include/opencv2/highgui/highgui.hpp"
#include "lib/opencv3/include/opencv2/video/background_segm.hpp"
#include "lib/opencv3/include/opencv2/core/ocl.hpp"

#include "lib/videoInput/videoInput/videoInput.h"

#include "lib/json/json.hpp"
using json = nlohmann::json;

#include <regex>

#include <msgpack.hpp>

/*
extern "C" {
	#include "lib/ffmpeg/libavcodec/avcodec.h"
	#include "lib/ffmpeg/libavformat/avformat.h"
	#include "lib/ffmpeg/libavutil/mathematics.h"
	#include "lib/ffmpeg/libavutil/imgutils.h"
	#include "lib/ffmpeg/libavutil/opt.h"
	#include "lib/ffmpeg/libavutil/samplefmt.h"
	#include "lib/ffmpeg/libswscale/swscale.h"
}
*/