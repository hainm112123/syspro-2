#ifdef _WIN32
#include "EventLogger.Win.h"
#else
#include "EventLogger.Win.h"
#endif

int main() {
	EventLogger logger("tmp.txt");
	std::string pname = "brave.exe";
	OveruseEvent event1 = { (DWORD)1, stringToWstring(pname), "cpu", 30 };
	OveruseEvent event2 = { (DWORD)1, stringToWstring(pname), "ram", 1000 };
	logger.log(event1);
	logger.log(event2);
	logger.flush();

	return 0;
}