#include "crash_handler.h"
#include "log.h"
#include <Windows.h>
#include <DbgHelp.h>
#include <stdio.h>

#pragma comment(lib, "dbghelp.lib")

namespace
{
	const char* GetExceptionDescription(DWORD code)
	{
		switch (code)
		{
		case EXCEPTION_ACCESS_VIOLATION:      return "EXCEPTION_ACCESS_VIOLATION";
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
		case EXCEPTION_BREAKPOINT:            return "EXCEPTION_BREAKPOINT";
		case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
		case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "EXCEPTION_INT_DIVIDE_BY_ZERO";
		case EXCEPTION_STACK_OVERFLOW:        return "EXCEPTION_STACK_OVERFLOW";
		default:                              return "UNKNOWN_EXCEPTION";
		}
	}

	// Vectored exception handler: logs first/second-chance exceptions but lets them continue.
	LONG WINAPI VectoredExceptionHandler(EXCEPTION_POINTERS* pExceptionPointers)
	{
		DWORD code = pExceptionPointers->ExceptionRecord->ExceptionCode;
		void* addr = pExceptionPointers->ExceptionRecord->ExceptionAddress;
		DWORD threadId = GetCurrentThreadId();

		Log::Write("[VEH] Exception 0x%X (%s) at address 0x%p on thread %u",
			code, GetExceptionDescription(code), addr, threadId);

		// Continue search so normal SEH / unhandled filter still run.
		return EXCEPTION_CONTINUE_SEARCH;
	}

	LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* pExceptionPointers)
	{
		DWORD threadId = GetCurrentThreadId();
		DWORD processId = GetCurrentProcessId();

		Log::Write("================================================================");
		Log::Write("                 UNHANDLED EXCEPTION DETECTED                   ");
		Log::Write("================================================================");
		Log::Write("Process ID: %u, Thread ID: %u", processId, threadId);

		// Exception details
		DWORD code = pExceptionPointers->ExceptionRecord->ExceptionCode;
		Log::Write("Exception Code: 0x%X (%s)", code, GetExceptionDescription(code));
		Log::Write("Exception Flags: 0x%X", pExceptionPointers->ExceptionRecord->ExceptionFlags);
		Log::Write("Exception Address: 0x%p", pExceptionPointers->ExceptionRecord->ExceptionAddress);

		// Faulting module
		HMODULE hModule = NULL;
		GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)pExceptionPointers->ExceptionRecord->ExceptionAddress, &hModule);

		char module_name[MAX_PATH] = "Unknown";
		if (hModule)
		{
			GetModuleFileNameA(hModule, module_name, sizeof(module_name));
			Log::Write("Faulting module: %s", module_name);
			Log::Write("Offset: 0x%llX", (uintptr_t)pExceptionPointers->ExceptionRecord->ExceptionAddress - (uintptr_t)hModule);
		}
		else
		{
			Log::Write("Faulting module: (unknown)");
		}

		Log::Write("----------------------------------------------------------------");
		Log::Write("                          CALL STACK                            ");
		Log::Write("----------------------------------------------------------------");

		CONTEXT* context = pExceptionPointers->ContextRecord;
		STACKFRAME64 stackFrame = {};
#ifdef _WIN64
		stackFrame.AddrPC.Offset = context->Rip;
		stackFrame.AddrPC.Mode = AddrModeFlat;
		stackFrame.AddrStack.Offset = context->Rsp;
		stackFrame.AddrStack.Mode = AddrModeFlat;
		stackFrame.AddrFrame.Offset = context->Rbp;
		stackFrame.AddrFrame.Mode = AddrModeFlat;
		DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
#else
		stackFrame.AddrPC.Offset = context->Eip;
		stackFrame.AddrPC.Mode = AddrModeFlat;
		stackFrame.AddrStack.Offset = context->Esp;
		stackFrame.AddrStack.Mode = AddrModeFlat;
		stackFrame.AddrFrame.Offset = context->Ebp;
		stackFrame.AddrFrame.Mode = AddrModeFlat;
		DWORD machineType = IMAGE_FILE_MACHINE_I386;
#endif
		HANDLE hThread = GetCurrentThread();
		HANDLE hProcess = GetCurrentProcess();

		char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
		PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuffer;
		pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSymbol->MaxNameLen = MAX_SYM_NAME;

		for (int i = 0; i < 25; ++i)
		{
			if (!StackWalk64(machineType, hProcess, hThread, &stackFrame, context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
				break;
			if (stackFrame.AddrPC.Offset == 0)
				break;

			char line[1024];
			DWORD64 displacement = 0;
			if (SymFromAddr(hProcess, stackFrame.AddrPC.Offset, &displacement, pSymbol))
			{
				sprintf_s(line, sizeof(line), "0x%llX %s + 0x%llX", stackFrame.AddrPC.Offset, pSymbol->Name, displacement);
			}
			else
			{
				sprintf_s(line, sizeof(line), "0x%llX (No symbols found)", stackFrame.AddrPC.Offset);
			}
			Log::Write("%s", line);
		}

		Log::Write("================================================================");
		Log::Flush(); // Ensure log is written

		return EXCEPTION_EXECUTE_HANDLER;
	}
}

void CrashHandler::Init()
{
	// Initialize symbols upfront to avoid memory allocation during a crash
	SymInitialize(GetCurrentProcess(), NULL, TRUE);

	// Vectored handler for first/second-chance exceptions.
	AddVectoredExceptionHandler(1, VectoredExceptionHandler);

	// Unhandled exception filter as last-resort handler.
	SetUnhandledExceptionFilter(UnhandledExceptionHandler);
}

void CrashHandler::Shutdown()
{
	SymCleanup(GetCurrentProcess());
}
