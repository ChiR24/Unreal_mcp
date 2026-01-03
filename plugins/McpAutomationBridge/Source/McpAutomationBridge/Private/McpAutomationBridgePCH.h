// McpAutomationBridgePCH.h
// Shared precompiled header for all McpAutomationBridge compilation units.
// This header provides common includes to avoid missing symbol errors
// when building without engine PCH.

#pragma once

// ============================================================================
// CORE UNREAL INCLUDES
// ============================================================================

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/Platform.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"

// Containers
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/StringConv.h"
#include "Containers/Ticker.h"

// Misc
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeExit.h"
#include "Misc/OutputDevice.h"
#include "Misc/PackageName.h"
#include "Misc/FeedbackContext.h"

// Templates
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"
#include "Templates/SubclassOf.h"

// ============================================================================
// JSON SUPPORT (must come early for FJsonObject)
// ============================================================================

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"

// ============================================================================
// ASYNC & THREADING
// ============================================================================

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"

// ============================================================================
// UOBJECT SYSTEM
// ============================================================================

#include "UObject/Object.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "UObject/SavePackage.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/PropertyPortFlags.h"

// ============================================================================
// ENGINE VERSION
// ============================================================================

#include "Runtime/Launch/Resources/Version.h"

// ============================================================================
// ENGINE TYPES
// ============================================================================

#include "Engine/EngineTypes.h"

// ============================================================================
// MATERIALS (for MD_Surface enum)
// ============================================================================

#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"

// ============================================================================
// ASSET REGISTRY
// ============================================================================

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"

// ============================================================================
// ANIMATION (for USkeleton forward declaration support)
// ============================================================================

// Forward declarations that may be needed
class USkeleton;
class UAnimSequence;
class UAnimBlueprint;

// ============================================================================
// EDITOR-ONLY INCLUDES
// ============================================================================

#if WITH_EDITOR

// Editor subsystem
#include "EditorSubsystem.h"
#include "Subsystems/EditorSubsystem.h"

// Scoped Transaction (for undo/redo support)
#if __has_include("ScopedTransaction.h")
#include "ScopedTransaction.h"
#define MCP_PCH_HAS_SCOPED_TRANSACTION 1
#elif __has_include("Editor/ScopedTransaction.h")
#include "Editor/ScopedTransaction.h"
#define MCP_PCH_HAS_SCOPED_TRANSACTION 1
#elif __has_include("Misc/ScopedTransaction.h")
#include "Misc/ScopedTransaction.h"
#define MCP_PCH_HAS_SCOPED_TRANSACTION 1
#else
#define MCP_PCH_HAS_SCOPED_TRANSACTION 0
#endif

// Editor asset library
#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#elif __has_include("Editor/EditorAssetLibrary.h")
#include "Editor/EditorAssetLibrary.h"
#endif

// Asset tools
#include "AssetToolsModule.h"

// Blueprints
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

// SCS (Simple Construction Script)
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

// Engine utilities
#include "EngineUtils.h"

// Modules
#include "Modules/ModuleManager.h"

#endif // WITH_EDITOR
