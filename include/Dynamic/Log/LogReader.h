#pragma once

#include "Dynamic/Log/LogRecord.h"
#include "Util/Optional.h"

#include <fstream>
#include <vector>

namespace dynamic
{

class EagerLogReader
{
public:
	EagerLogReader() = delete;

	static std::vector<LogRecord> readLogFromFile(const char* fileName);
};

class LazyLogReader
{
private:
	std::ifstream ifs;
public:
	LazyLogReader(const char* fileName);

	util::Optional<LogRecord> readLogRecord();
};

}
