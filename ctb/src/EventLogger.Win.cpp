#include "EventLogger.Win.h"

void EventLogger::log(const OveruseEvent &event) {
	char line[256];
	int len = snprintf(line, sizeof(line), "{\"pid\":%d,\"name\":\"%s\",\"type\":%s,\"value\":%.2f,\"time\":%s}\n", 
											event.pid, event.name, event.type, event.value, event.time);
	if (buffer.size() + len >= BUF_SIZE) flush();
	buffer.insert(buffer.end(), line, line + len);
}

void EventLogger::flush() {
	if (!buffer.empty()) {
		DWORD written;
		WriteFile(hFile, buffer.data(), (DWORD)buffer.size(), &written, nullptr);
		buffer.clear();
	}
}

void EventLogger::getEvents() {
	DWORD result = WaitForSingleObject(hEvent, 2000);
	while (1) {
		LONG head = ring->head;
		LONG tail = ring->tail;
		if (tail == head) break;
		auto event = ring->buffer[head];
		log(event);
		InterlockedExchange(&ring->head, (head + 1) & (RING_SIZE - 1));
	}
}

void EventLogger::run() {
	HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedRing), FILE_MAP_NAME.c_str());
	if (!hMap) {
		printf("Create file mapping failed\n");
		return;
	}
	ring = (SharedRing*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!ring) {
		printf("Map view of file failed\n");
		CloseHandle(hMap);
		return;
	}
	hEvent = CreateEvent(nullptr, FALSE, FALSE, WIN_EVENT_NAME.c_str());
	if (!hEvent) {
		printf("Create event failed\n");
		UnmapViewOfFile(ring);
		CloseHandle(hMap);
		return;
	}

	printf("Start waiting for events...\n");
	printf("Press q to quit...\n");
	while (true) {
		if (_kbhit()) {
			char c = _getch();
			if (c == 'q') break;
		}
		getEvents();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	flush();

	CloseHandle(hEvent);
	UnmapViewOfFile(ring);
	CloseHandle(hMap);
}
