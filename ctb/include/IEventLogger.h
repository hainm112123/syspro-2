#pragma once
#ifndef IEVENT_LOGGER_H
#define IEVENT_LOGGER_H

#include "utils.h"

class IEventLogger {
public:
	const int BUF_SIZE = 8192;
	IEventLogger() {}
	~IEventLogger() {}
	virtual void log(const OveruseEvent& event) = 0;
	virtual void flush() = 0;
	virtual void run() = 0;
};

#endif 
