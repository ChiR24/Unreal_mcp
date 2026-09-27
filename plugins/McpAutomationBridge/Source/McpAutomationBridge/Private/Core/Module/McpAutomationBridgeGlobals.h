// Shared globals for McpAutomationBridge plugin
#pragma once

// Workaround for UE 5.0 engine headers using __has_feature (Clang-specific) with MSVC
// ConcurrentLinearAllocator.h and other engine headers use this macro
#if defined(_MSC_VER) && !defined(__has_feature)
#define __has_feature(x) 0
#endif

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"

extern FString GCurrentSequencePath;

