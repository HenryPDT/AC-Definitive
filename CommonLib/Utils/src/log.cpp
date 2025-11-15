#include "log.h"
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdarg>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace
{
	std::ofstream log_file;
	std::recursive_mutex log_mutex;
	std::vector<Log::LogSink> sinks;

	std::filesystem::path get_log_path(HMODULE hModule)
	{
		char module_path[MAX_PATH];
		GetModuleFileNameA(hModule, module_path, sizeof(module_path));
		std::filesystem::path dll_path = module_path;
		return dll_path.parent_path() / "PluginLoader.log";
	}
}

void Log::Init(HMODULE hModule)
{
	std::lock_guard<std::recursive_mutex> lock(log_mutex);
	if (!log_file.is_open())
	{
		auto log_path = get_log_path(hModule);
		log_file.open(log_path, std::ios::out | std::ios::trunc);
		if (log_file.is_open())
		{
			Write("[INFO] Logger initialized. Log file: %s", log_path.string().c_str());
		}
	}
}

void Log::Shutdown()
{
	std::lock_guard<std::recursive_mutex> lock(log_mutex);
	if (log_file.is_open())
	{
		Write("[INFO] Logger shutting down.");
		log_file.close();
	}
}

void Log::Flush()
{
	std::lock_guard<std::recursive_mutex> lock(log_mutex);
	if (log_file.is_open())
	{
		log_file.flush();
	}
}

void Log::AddSink(LogSink sink)
{
	std::lock_guard<std::recursive_mutex> lock(log_mutex);
	sinks.push_back(sink);
}

void Log::RemoveSink(LogSink sink)
{
	std::lock_guard<std::recursive_mutex> lock(log_mutex);
	sinks.erase(std::remove(sinks.begin(), sinks.end(), sink), sinks.end());
}

void Log::Write(const char* fmt, ...)
{
	std::lock_guard<std::recursive_mutex> lock(log_mutex);
	
	char buffer[4096];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	std::tm tm_now;
	localtime_s(&tm_now, &time_t_now);

	std::stringstream ss;
	ss << "[" << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S") << "] ";
	ss << "[TID:0x" << std::hex << GetCurrentThreadId() << std::dec << "] ";
	ss << buffer;

	std::string final_log_line = ss.str();

	if (log_file.is_open())
	{
		log_file << final_log_line << std::endl;
		log_file.flush();
	}

	for (const auto& sink : sinks)
	{
		sink(final_log_line.c_str());
	}
}
