
#include <windows.h>
#include <fstream>
#include <ostream>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <strsafe.h>
#include <future>

#include <boost/algorithm/string.hpp>

#include "main.h"
#include "d3d9.h"
#include "version.h"
#include "d3dutil.h"
#include "Settings.h"
#include "KeyActions.h"
#include "Detouring.h"
#include "Registry.h"
#include "blacklist.h"

FILE* g_oFile = NULL;
bool g_active = false;

LRESULT CALLBACK GeDoSaToHook(_In_ int nCode, _In_ WPARAM wParam, _In_ LPARAM lParam) {
	SDLOG(16, "GeDoSaToHook called\n");
	return CallNextHookEx(NULL, nCode, wParam, lParam); 
}

const char* GeDoSaToVersion() {
	static string verString;
	if(verString.empty()) verString = format("%s (%s)", VER_STRING, MODE_STRING);
	return verString.c_str();
}

BOOL WINAPI DllMain(HMODULE hDll, DWORD dwReason, PVOID pvReserved) {	
	if(dwReason == DLL_PROCESS_ATTACH) {
		// don't attach to processes on the blacklist
		if(onBlacklist(getExeFileName())) {
			if(getExeFileName() == "GeDoSaToTool") return true;
			return false;
		}
		g_active = true;

		// read install location from registry
		getInstallDirectory();
		
		// initialize log	
		string logFn = format("logs\\%s_%s.log", getExeFileName().c_str(), getTimeString().c_str());
		boost::replace_all(logFn, ":", "-");
		boost::replace_all(logFn, " ", "_");
		logFn = getInstalledFileName(logFn);
		fopen_s(&g_oFile, logFn.c_str(), "w");
		if(!g_oFile) OutputDebugString(format("GeDoSaTo: Error opening log fn %s", logFn.c_str()).c_str());
		else OutputDebugString(format("GeDoSaTo: Opening log fn %s, handle: %p", logFn.c_str(), g_oFile).c_str());

		// startup
		sdlogtime(-1);
		SDLOG(-1, "===== start "INTERCEPTOR_NAME" %s = fn: %s\n", VERSION, getExeFileName().c_str());
		SDLOG(-1, "===== installation directory: %s\n", getInstallDirectory().c_str());

		// load settings
		Settings::get().load();
		Settings::get().report();
		 
		KeyActions::get().load();
		KeyActions::get().report();

		// detour
		startDetour();
		return true;
	}
	if(dwReason == DLL_PROCESS_DETACH && g_active) {
		Settings::get().shutdown();
		endDetour();
		SDLOG(-1, "===== end =\n");
		fclose(g_oFile);
	}
    return false;
}

const std::string& getInstallDirectory() {
	static string installDir;
	if(installDir.empty()) {
		installDir = getRegString(REG_KEY_PATH, "InstallPath");
		if(installDir.empty()) {
			messageErrorAndExit("Could not read the install location from the registry.\nMake sure to extract the downloaded files to a suitable location and run GeDoSaToTool.exe");
		}
	}
	return installDir;
}
const string& getExeFileName() {
	static string exeFn;
	if(exeFn.empty()) {
		char fileName[2048];
		GetModuleFileNameA(NULL, fileName, 2048);
		exeFn = string(fileName);
		size_t pos = exeFn.rfind("\\");
		if(pos != string::npos) {
			exeFn = exeFn.substr(pos+1);
			pos = exeFn.rfind(".exe");
			if(pos == string::npos) pos = exeFn.rfind(".EXE");
			if(pos != string::npos) {
				exeFn = exeFn.substr(0, pos);
			}
		}
	}
	return exeFn;
}
string getInstalledFileName(string filename) {
	return getInstallDirectory() + filename;
}
string getAssetFileName(string filename) {
	return getInstallDirectory() + "assets\\" + filename;
}

string getTimeString() {
	char timebuf[26];
    time_t ltime;
    struct tm gmt;
	time(&ltime);
    _localtime64_s(&gmt, &ltime);
    asctime_s(timebuf, 26, &gmt);
	timebuf[24] = '\0'; // remove newline
	return string(timebuf);
}

void __cdecl sdlogtime(int level) {
	SDLOG(level, "===== %s =====\n", getTimeString().c_str());
}

void __cdecl sdlog(const char *fmt, ...) {
	if(!fmt) { return; }

	va_list va_alist;

	va_start (va_alist, fmt);
	vfprintf(g_oFile, fmt, va_alist);
	va_end (va_alist);

	fflush(g_oFile);
}

void errorExit(LPTSTR lpszFunction) { 
    // Retrieve the system error message for the last-error code
    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL );

    // Display the error message and exit the process
    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(TCHAR), TEXT("%s failed with error %d: %s"), lpszFunction, dw, lpMsgBuf); 
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
    ExitProcess(dw); 
}

bool fileExists(const char *filename) {
  std::ifstream ifile(filename);
  return NULL != ifile;
}

void messageErrorAndExit(string error) {
	std::ofstream ofile("ERROR.txt", std::ios::out);
	ofile << error;
	ofile.close();
	exit(-1);
}

#include <stdarg.h>

string format(const char* formatString, ...) {
	va_list arglist;
	va_start(arglist, formatString);
	const unsigned BUFFER_SIZE = 2048*8;
	char buffer[BUFFER_SIZE];
	vsnprintf_s(buffer, BUFFER_SIZE, formatString, arglist);
	va_end(arglist);
	return string(buffer);
}