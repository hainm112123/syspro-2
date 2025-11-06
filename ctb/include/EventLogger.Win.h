#pragma once
#ifndef EVENT_LOGGER_WIN_H
#define EVENT_LOGGER_WIN_H

#include "IEventLogger.h"
#include "nlohmann/json.hpp"
#include <windows.h>
#include <string>
#include <vector>
#define UNICODE
#define _UNICODE

using json = nlohmann::json;

class EventLogger : public IEventLogger {
private:
	HANDLE hFile;
	std::vector<char> buffer;
public:
	EventLogger(const std::string path) {
		hFile = CreateFileW(stringToWstring(path).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
		buffer.reserve(8192);
	}
	~EventLogger() {
		flush();
		if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
	}

	void log(const OveruseEvent& event);
	void flush();
};

#endif