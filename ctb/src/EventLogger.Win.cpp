#include "EventLogger.Win.h"

void EventLogger::log(const OveruseEvent &event) {
	char line[256];
	int len = snprintf(line, sizeof(line), "{\"pid\":%lu,\"name\":\"%s\",\"type\":%s,\"value\":%.2f}\n", event.pid, wstringToString(event.name).c_str(), event.type.c_str(), event.value);
	if (buffer.size() + len >= 8192) flush();
	buffer.insert(buffer.end(), line, line + len);
}

void EventLogger::flush() {
	if (!buffer.empty()) {
		DWORD written;
		WriteFile(hFile, buffer.data(), (DWORD)buffer.size(), &written, nullptr);
		buffer.clear();
	}
}
