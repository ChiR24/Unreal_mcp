#pragma once

#include "McpAutomationBridge_GASAvailability.h"
#include "McpAutomationBridgeSubsystem.h"

#include "CoreMinimal.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX || PLATFORM_MAC
#include <errno.h>
#include <sys/stat.h>
#endif

#ifndef MCP_PLATFORM_HOLOLENS
#if defined(PLATFORM_HOLOLENS)
#define MCP_PLATFORM_HOLOLENS PLATFORM_HOLOLENS
#else
#define MCP_PLATFORM_HOLOLENS 0
#endif
#endif

#if PLATFORM_WINDOWS || MCP_PLATFORM_HOLOLENS
#include "Windows/WindowsHWrapper.h"
#endif

#include "McpAutomationBridgeHelpersProjectPaths.h"
#include "McpAutomationBridgeHelpersAssetCreation.h"
