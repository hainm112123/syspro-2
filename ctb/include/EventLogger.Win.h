#pragma once
#ifndef EVENT_LOGGER_WIN_H
#define EVENT_LOGGER_WIN_H
#ifdef _WIN32

#define UNICODE
#define _UNICODE
#include "IEventLogger.h"
#include <windows.h>
#include "nlohmann/json.hpp"
#include <string>
#include <vector>
#include <conio.h>
#include <chrono>
#include <thread>
#include <wmistr.h>
#include <tchar.h>

using json = nlohmann::json;

class EventLogger : public IEventLogger {
private:
	const std::wstring FILE_MAP_NAME = _TEXT("SharedOveruseEventRing");
	const std::wstring WIN_EVENT_NAME = _TEXT("SharedRingEvent");
	HANDLE hFile;
	std::vector<char> buffer;
	SharedRing* ring = nullptr;
	HANDLE hEvent;
public:
	EventLogger(const std::string path) {
		hFile = CreateFileW(stringToWstring(path).c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		buffer.reserve(BUF_SIZE);
	}
	~EventLogger() {
		flush();
		if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
	}

	void log(const OveruseEvent& event);
	void flush();
	void getEvents();
	void run();
};

#endif
#endif
