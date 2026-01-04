#pragma once
#ifndef LOG_H
#define LOG_H

#include <Windows.h>

namespace Log
{
	using LogSink = void(*)(const char*);

	void Init(HMODULE hModule);
	void SetFlags(bool time, bool tid);
	// Helper to initialize logging with a remote sink (like the Plugin Loader)
	// Disables local timestamps/TIDs to avoid duplication and adds the sink.
	void InitSink(LogSink sink);
	void Shutdown();
	void Flush();
	void Write(const char* fmt, ...);
	void AddSink(LogSink sink);
	void RemoveSink(LogSink sink);
}

#define LOG_INFO(fmt, ...)  Log::Write("[INFO] [%s] " fmt, __func__, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Log::Write("[WARN] [%s] " fmt, __func__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) Log::Write("[ERROR] [%s] " fmt, __func__, ##__VA_ARGS__)

#endif // LOG_H
