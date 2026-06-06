// Helper utilities for McpAutomationBridgeSubsystem
#pragma once

#include "AssetRegistry/AssetData.h"
#include "Containers/ScriptArray.h"
#include "Containers/StringConv.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformFileManager.h"
#include "Internationalization/Text.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeLock.h"
#include "UObject/TextProperty.h"
#include "UObject/UnrealType.h"
#include <type_traits>

#if PLATFORM_UNIX || PLATFORM_MAC
#include <errno.h>
#include <sys/stat.h>
#endif

#if defined(PLATFORM_HOLOLENS)
#define MCP_PLATFORM_HOLOLENS PLATFORM_HOLOLENS
#else
#define MCP_PLATFORM_HOLOLENS 0
#endif

#if PLATFORM_WINDOWS || MCP_PLATFORM_HOLOLENS
#include "Windows/WindowsHWrapper.h"
#endif

// Include centralized UE version compatibility macros.
#include "McpVersionCompatibility.h"

// Globals used by registry helpers and fast-mode simulations
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"  // GEditor for McpSafeLoadMap
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "RenderingThread.h"  // FlushRenderingCommands for safe level saves

#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/WorldSettings.h"
#include "TickTaskManagerInterface.h"
#include "HAL/PlatformProcess.h"
#endif

#include "McpAutomationBridgeHelpersProjectPaths.h"
#include "McpAutomationBridgeHelpersCommandValidation.h"
#include "McpAutomationBridgeHelpersAssetCreation.h"
#include "McpAutomationBridgeHelpersAssetResolution.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeHelpersComponentLookup.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersClassResolution.h"
#include "McpAutomationBridgeHelpersOutputCapture.h"
#include "McpAutomationBridgeHelpersPropertyExport.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersPropertyApply.h"
#include "McpAutomationBridgeHelpersJsonFields.h"
#include "McpAutomationBridgeHelpersNestedPropertyPath.h"
#include "McpAutomationBridgeHelpersScsLookup.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpAutomationBridgeHelpersBlueprintPaths.h"
#include "McpAutomationBridgeHelpersPropertyLookup.h"
#include "McpAutomationBridgeHelpersResponses.h"
#include "McpAutomationBridgeHelpersActorSpawn.h"
#include "McpAutomationBridgeHelpersResponseVerification.h"
#include "McpAutomationBridgeHelpersAssetDirectories.h"
