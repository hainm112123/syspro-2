#ifdef _WIN32
#include "EventLogger.Win.h"
#else
#include "EventLogger.Linux.h"
#endif

#include "ConfigModifier.h"
#include <iostream>

int main() {
	/*EventLogger logger("tmp.txt");
	std::string pname = "brave.exe";
	OveruseEvent event1 = { 1, pname, "cpu", 30, "2025-11-06 13:24:18" };
	OveruseEvent event2 = { 1, pname, "ram", 1000, "2025-11-06 13:24:18" };
	logger.log(event1);
	logger.log(event2);
	logger.flush();*/
	ConfigModifier cnfMdf;
	if (cnfMdf.start()) {
		std::cout << "Success\n";
	}
	else {
		std::cout << "Failed\n";
	}

	return 0;
}
