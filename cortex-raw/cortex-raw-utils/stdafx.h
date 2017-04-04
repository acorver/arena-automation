// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>

#define GLOBAL
#define LOCAL
#define EXPORTED
#define ERRFLAG 1
#define OK 0
#define g_nMaxCameras 128
#define AMNM 1024
#define MAX_VBOARDS 1024 // WHAT DOES THIS MEAN??

#include <windows.h>  // For CopyFile

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tchar.h>
#include <fstream>
#include <iostream>

#include <cstring>
using namespace std;

#include "json.hpp"
using json = nlohmann::json;

