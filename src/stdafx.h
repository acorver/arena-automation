#pragma once

#define WIN32_LEAN_AND_MEAN 
#include <boost/thread.hpp> 
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/vector_expression.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/atomic.hpp>
#include <boost/algorithm/string/replace.hpp>

//#include <boost/gil/gil_all.hpp>

#include <stdio.h>

#include <crow/crow.h>

#include <windows.h>
#include <algorithm>
#include <vector>
#include <queue>
#include <deque>
#include <map>
#include <fstream>
#include <climits>
#include <string>
#include <regex>
#include <chrono>
#include <stdarg.h>

#include "log.h"

#include <jpeg-compressor/jpge.h>

#include <serial/SerialClass.h>

#include <cortex/Cortex.h>

#include <photron/PDCLIB.h>

#include <opencv3/include/opencv2/opencv.hpp>
#include <opencv3/include/opencv2/core/core.hpp>
#include <opencv3/include/opencv2/imgproc/imgproc.hpp>
#include <opencv3/include/opencv2/core/operations.hpp>
#include <opencv3/include/opencv2/highgui/highgui.hpp>
#include <opencv3/include/opencv2/video/background_segm.hpp>
#include <opencv3/include/opencv2/core/ocl.hpp>

#include <videoInput/videoInput/videoInput.h>

#include <json/json.hpp>
using json = nlohmann::json;

#include <msgpack.hpp>

//#define ELPP_THREAD_SAFE
//#include <easyloggingpp/easylogging++.h>

#include <linalg/linalg.h>
#define PI 3.14159265358979323846

#include <cpr/cpr.h>

