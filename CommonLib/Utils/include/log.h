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

// Throttled logging - only logs once per interval_ms. Useful for frequently-called functions.
#define LOG_THROTTLED(interval_ms, fmt, ...) do { \
    static ULONGLONG s_lastLogTime = 0; \
    ULONGLONG now = GetTickCount64(); \
    if (now - s_lastLogTime > (interval_ms)) { \
        s_lastLogTime = now; \
        LOG_INFO(fmt, ##__VA_ARGS__); \
    } \
} while(0)

#endif // LOG_H
