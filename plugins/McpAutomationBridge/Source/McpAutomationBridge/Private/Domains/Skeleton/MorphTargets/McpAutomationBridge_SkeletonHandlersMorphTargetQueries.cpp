#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/MorphTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Misc/Paths.h"

using namespace McpSkeletonHandlers;
bool UMcpAutomationBridgeSubsystem::HandleSetMorphTargetValue(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString MorphTargetName = GetJsonStringField(Payload, TEXT("morphTargetName"));
    double Value = GetJsonNumberField(Payload, TEXT("value"), 0.0);
    bool bAddMissing = GetJsonBoolField(Payload, TEXT("addMissing"), false);

    if (ActorName.IsEmpty() || MorphTargetName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("actorName and morphTargetName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    Value = FMath::Clamp(Value, 0.0, 1.0);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    AActor* FoundActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            FoundActor = *It;
            break;
        }
    }

    if (!FoundActor)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    USkeletalMeshComponent* SkelMeshComp = FoundActor->FindComponentByClass<USkeletalMeshComponent>();
    if (!SkelMeshComp)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Actor does not have a SkeletalMeshComponent"), TEXT("NO_SKEL_MESH_COMP"));
        return true;
    }

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    USkeletalMesh* SkelMesh = SkelMeshComp->GetSkeletalMeshAsset();
#else
    // UE 5.0: Use SkeletalMesh property directly
    USkeletalMesh* SkelMesh = SkelMeshComp->SkeletalMesh;
#endif
    if (SkelMesh)
    {
        const bool bHasMorphTarget = SkelMesh->FindMorphTarget(FName(*MorphTargetName)) != nullptr;

        if (!bHasMorphTarget && !bAddMissing)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Morph target '%s' not found on mesh"), *MorphTargetName),
                TEXT("MORPH_TARGET_NOT_FOUND"));
            return true;
        }
    }

    SkelMeshComp->SetMorphTarget(FName(*MorphTargetName), static_cast<float>(Value));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("value"), Value);

    TArray<TSharedPtr<FJsonValue>> ActiveMorphs;
    const TMap<FName, float>& MorphCurves = SkelMeshComp->GetMorphTargetCurves();
    for (const auto& Pair : MorphCurves)
    {
        if (Pair.Value > 0.0f)
        {
            TSharedPtr<FJsonObject> MorphObj = McpHandlerUtils::CreateResultObject();
            MorphObj->SetStringField(TEXT("name"), Pair.Key.ToString());
            MorphObj->SetNumberField(TEXT("weight"), Pair.Value);
            ActiveMorphs.Add(MakeShared<FJsonValueObject>(MorphObj));
        }
    }
    Result->SetArrayField(TEXT("activeMorphTargets"), ActiveMorphs);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Morph target '%s' set to %.3f"), *MorphTargetName, Value), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleListMorphTargets(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    if (SkeletalMeshPath.IsEmpty())
    {
        SkeletalMeshPath = GetJsonStringField(Payload, TEXT("meshPath"));
    }

    if (SkeletalMeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> MorphTargetArray;
    for (const UMorphTarget* MT : Mesh->GetMorphTargets())
    {
        if (MT)
        {
            TSharedPtr<FJsonObject> MTObj = McpHandlerUtils::CreateResultObject();
            MTObj->SetStringField(TEXT("name"), MT->GetName());
            MTObj->SetNumberField(TEXT("numDeltas"), MT->GetMorphLODModels().Num() > 0 ?
                MT->GetMorphLODModels()[0].Vertices.Num() : 0);
            MorphTargetArray.Add(MakeShared<FJsonValueObject>(MTObj));
        }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetArrayField(TEXT("morphTargets"), MorphTargetArray);
    Result->SetNumberField(TEXT("count"), MorphTargetArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Found %d morph targets"), MorphTargetArray.Num()), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleDeleteMorphTarget(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    FString MorphTargetName = GetJsonStringField(Payload, TEXT("morphTargetName"));

    if (SkeletalMeshPath.IsEmpty() || MorphTargetName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("skeletalMeshPath and morphTargetName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }

    UMorphTarget* TargetToRemove = Mesh->FindMorphTarget(FName(*MorphTargetName));
    if (!TargetToRemove)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Morph target '%s' not found"), *MorphTargetName), TEXT("MORPH_NOT_FOUND"));
        return true;
    }

    Mesh->Modify();
    Mesh->UnregisterMorphTarget(TargetToRemove);
    Mesh->MarkPackageDirty();
    McpSafeAssetSave(Mesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("remainingMorphTargets"), Mesh->GetMorphTargets().Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Morph target '%s' deleted"), *MorphTargetName), Result);
    return true;
}

