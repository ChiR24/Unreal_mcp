// Helper utilities for McpAutomationBridgeSubsystem
#pragma once

#include "AssetRegistry/AssetData.h"
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

// Include centralized UE version compatibility macros.
#include "Core/Compatibility/McpVersionCompatibility.h"

// Globals used by registry helpers and fast-mode simulations
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"

#include "Editor.h"  // GEditor for McpSafeLoadMap
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"
#include "RenderingThread.h"  // FlushRenderingCommands for safe level saves

#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/World.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/WorldSettings.h"
#include "TickTaskManagerInterface.h"
#include "HAL/PlatformProcess.h"

#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersCommandValidation.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetCreation.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetResolution.h"
#include "Safety/McpSafeOperations.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersComponentLookup.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersOutputCapture.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApply.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersNestedPropertyPath.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersScsLookup.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersResponses.h"
#include "Foundation/BridgeHelpers/Actors/McpAutomationBridgeHelpersActorSpawn.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersResponseVerification.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetDirectories.h"
