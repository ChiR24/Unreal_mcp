#pragma once

#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "ActorPartition/ActorPartitionSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FoliageType.h"
#include "FoliageTypeObject.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliageActor.h"
#include "ProceduralFoliageComponent.h"
#include "ProceduralFoliageSpawner.h"
#include "ProceduralFoliageVolume.h"
#include "WorldPartition/WorldPartition.h"

namespace McpFoliageHandlers {
AInstancedFoliageActor* GetOrCreateFoliageActorForWorldSafe(UWorld* World, bool bCreateIfNone);

// The foliage type at InOutPath or, for a StaticMesh path, the /Game/Foliage/Auto_<mesh> type made from it (made
// once, then reused). Replies ASSET_NOT_FOUND and returns null when neither loads; InOutPath becomes the type's path.
UFoliageType* ResolveFoliageTypeOrMesh(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                                       TSharedPtr<FMcpBridgeWebSocket> Socket, FString& InOutPath);

// The editor world's foliage actor, created when missing; replies and returns null without one.
AInstancedFoliageActor* RequireFoliageActor(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                                            TSharedPtr<FMcpBridgeWebSocket> Socket);

// Adds Instance of Type to IFA, registering the type on the actor first when it does not carry it yet.
inline void AddFoliageInstance(AInstancedFoliageActor* IFA, UFoliageType* Type, const FFoliageInstance& Instance)
{
    FFoliageInfo* Info = IFA->FindInfo(Type);
    if (!Info)
    {
        IFA->AddFoliageType(Type, &Info);
    }
    if (Info)
    {
        Info->AddInstance(Type, Instance, nullptr);
    }
}

// foliageTypePath (or foliageType), sanitized; a bare name lives in /Game/Foliage. OutPath is empty
// when neither is given. Replies SECURITY_VIOLATION and returns false for an unsafe path.
inline bool ReadFoliageTypePath(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                                TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload, FString& OutPath)
{
    const FString Requested = GetJsonStringField(Payload, TEXT("foliageTypePath"), GetJsonStringField(Payload, TEXT("foliageType")));
    OutPath.Reset();
    if (Requested.IsEmpty()) return true;
    const FString SafePath = SanitizeProjectRelativePath(Requested);
    if (SafePath.IsEmpty())
    {
        Bridge.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid or unsafe foliage type path: %s"), *Requested), TEXT("SECURITY_VIOLATION"));
        return false;
    }
    OutPath = FPaths::GetPath(SafePath).IsEmpty() ? FString::Printf(TEXT("/Game/Foliage/%s"), *SafePath) : SafePath;
    return true;
}
}
