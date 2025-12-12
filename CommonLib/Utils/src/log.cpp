#include "log.h"
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
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
		wchar_t module_path[MAX_PATH];
		GetModuleFileNameW(hModule, module_path, MAX_PATH);
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
	char buffer[4096];
	char time_buf[64];
	
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	// Format time
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	std::tm tm_now;
	localtime_s(&tm_now, &time_t_now);
	std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_now);

	// Combine into final string
	char final_log_line[4200];
	snprintf(final_log_line, sizeof(final_log_line), "[%s] [TID:0x%X] %s", time_buf, GetCurrentThreadId(), buffer);

	std::vector<Log::LogSink> sinks_copy;

	{
		std::lock_guard<std::recursive_mutex> lock(log_mutex);
		if (log_file.is_open())
		{
			log_file << final_log_line << "\n"; // \n is faster than endl (no forced flush)
			log_file.flush(); // keep file output in sync with console and avoid buffered loss
		}
		// Copy sinks to avoid calling them while holding the lock (prevents deadlocks)
		sinks_copy = sinks;
	}

	// Dispatch to sinks
	for (const auto& sink : sinks_copy)
	{
		sink(final_log_line);
	}
}
