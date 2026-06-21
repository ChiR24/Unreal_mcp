#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/Landscape/McpLandscapeMetadataTags.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "Animation/SkeletalMeshActor.h"
#include "Components/ActorComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Exporters/Exporter.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "Materials/MaterialInterface.h"

#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif

bool HandleControlActorCallFunction(const FString &RequestId,
                                    const TSharedPtr<FJsonObject> &Payload,
                                    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

// Selection & Grouping (Phase 34)
bool HandleControlActorSelect(const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleControlActorSelectByClass(const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleControlActorSelectByTag(const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleControlActorSelectInVolume(const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleControlActorDeselectAll(const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleControlActorGetSelected(const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleControlActorGroup(const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleControlActorUngroup(const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleControlActorRunUtility(const FString &RequestId, const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
AActor *FindActorByNameInWorldForMcp(UWorld *World, const FString &Target,
                                     bool bExactMatchOnly);
#endif
