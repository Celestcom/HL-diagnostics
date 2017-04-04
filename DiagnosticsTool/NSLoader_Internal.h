#pragma once
#include <stdint.h>

#include "NSLoader_fwds.h"

#define INTERNAL_TOOL

#ifdef INTERNAL_TOOL
	#ifdef NSLOADER_EXPORTS
	#define NSLOADER_INTERNAL_API __declspec(dllexport) 
	#else
	#define NSLOADER_INTERNAL_API __declspec(dllimport) 
	#endif
#else 
	#define NSLOADER_INTERNAL_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct NSVR_LogEntry_ {
		char Message[512];
		uint32_t Length; 
	} NSVR_LogEntry;



	NSLOADER_INTERNAL_API NSVR_Result __stdcall NSVR_System_PollLogs(NSVR_System* system, NSVR_LogEntry* entry);

	typedef struct NSVR_SystemStats_ {
		unsigned int NumLiveEffects;
		unsigned int NumOrphanedEffects;
	} NSVR_SystemStats;
	

	NSLOADER_INTERNAL_API NSVR_Result __stdcall NSVR_System_GetStats(NSVR_System* system, NSVR_SystemStats* ptr);
#ifdef __cplusplus
}
#endif

