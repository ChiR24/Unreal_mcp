// Helper utilities for EngineWireSubsystem
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
#include "Core/Module/EngineWireGlobals.h"
#include "EngineWireSubsystem.h"

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

#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "Foundation/BridgeHelpers/Security/McpCommandValidate.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetCreation.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetResolve.h"
#include "Safety/McpSafeOperations.h"
#include "Foundation/BridgeHelpers/Properties/McpComponentLookup.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "Foundation/BridgeHelpers/Responses/McpOutputCapture.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Properties/McpPropApply.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Properties/McpNestedPropPath.h"
#include "Foundation/BridgeHelpers/Blueprints/McpScsLookup.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"
#include "Foundation/BridgeHelpers/Actors/McpActorSpawnHelpers.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetDirs.h"
