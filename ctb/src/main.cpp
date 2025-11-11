#ifdef _WIN32
#include "EventLogger.Win.h"
#else
#include "EventLogger.Linux.h"
#endif

#include "ConfigModifier.h"
#include <iostream>

int main() {
	ConfigModifier cnfMdf;
	if (cnfMdf.start()) {
		std::cout << "Success\n";
	}
	else {
		std::cout << "Failed\n";
	}
	EventLogger logger("tmp.txt");
	logger.run();
	

	return 0;
}
