#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"
#include "Domains/Skeleton/MorphTargets/McpAutomationBridge_SkeletonHandlersMorphTargetDeltas.h"

#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleCreateMorphTarget(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    const FString MorphTargetName = GetJsonStringField(Payload, TEXT("morphTargetName"));

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

    if (Mesh->FindMorphTarget(FName(*MorphTargetName)))
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
        Result->SetBoolField(TEXT("alreadyExists"), true);
        Result->SetNumberField(TEXT("deltasApplied"), 0);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Morph target '%s' already exists"), *MorphTargetName), Result);
        return true;
    }

    // UE 5.7 requires valid delta data BEFORE RegisterMorphTarget(): it checks
    // HasValidData() and fires an ensure() for empty morphs. Parse first and
    // never create or register an empty target.
    TArray<FMorphTargetDelta> Deltas;
    FString DeltaError;
    FString DeltaErrorCode;
    if (!ParseMorphTargetDeltas(Payload, Deltas, DeltaError, DeltaErrorCode))
    {
        // A missing array keeps the historical EMPTY_MORPH_TARGET code.
        const FString Code = DeltaErrorCode == TEXT("MISSING_PARAM") ? FString(TEXT("EMPTY_MORPH_TARGET")) : DeltaErrorCode;
        SendAutomationError(RequestingSocket, RequestId, DeltaError, Code);
        return true;
    }

    const int32 LODIndex = GetIntFieldSkel(Payload, TEXT("lodIndex"), 0);
    UMorphTarget* NewMorphTarget = NewObject<UMorphTarget>(Mesh, FName(*MorphTargetName));
    if (!NewMorphTarget)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create morph target object"), TEXT("CREATION_FAILED"));
        return true;
    }

    // BaseSkelMesh is required for HasValidData(); PopulateDeltas needs the
    // LOD's sections to map vertices onto sections.
    NewMorphTarget->BaseSkelMesh = Mesh;
    NewMorphTarget->PopulateDeltas(Deltas, LODIndex, GetMorphTargetLodSections(Mesh, LODIndex), false, false);

    if (!NewMorphTarget->HasValidData())
    {
        NewMorphTarget->MarkAsGarbage();
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Morph target '%s' has no valid data after populating %d deltas; check that the vertex indices exist in LOD %d"),
                *MorphTargetName, Deltas.Num(), LODIndex),
            TEXT("INVALID_MORPH_DATA"));
        return true;
    }

    // Only register AFTER the morph target has valid data.
    Mesh->Modify();
    Mesh->RegisterMorphTarget(NewMorphTarget);
    Mesh->MarkPackageDirty();
    McpSafeAssetSave(Mesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("morphTargetCount"), Mesh->GetMorphTargets().Num());
    Result->SetNumberField(TEXT("deltaCount"), Deltas.Num());
    Result->SetNumberField(TEXT("deltasApplied"), Deltas.Num());
    Result->SetNumberField(TEXT("lodIndex"), LODIndex);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Morph target '%s' created with %d deltas"), *MorphTargetName, Deltas.Num()), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetMorphTargetDeltas(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    const FString MorphTargetName = GetJsonStringField(Payload, TEXT("morphTargetName"));

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

    TArray<FMorphTargetDelta> Deltas;
    FString DeltaError;
    FString DeltaErrorCode;
    if (!ParseMorphTargetDeltas(Payload, Deltas, DeltaError, DeltaErrorCode))
    {
        SendAutomationError(RequestingSocket, RequestId, DeltaError, DeltaErrorCode);
        return true;
    }

    const int32 LODIndex = GetIntFieldSkel(Payload, TEXT("lodIndex"), 0);
    UMorphTarget* MorphTarget = Mesh->FindMorphTarget(FName(*MorphTargetName));
    bool bCreatedMorphTarget = false;
    if (!MorphTarget)
    {
        MorphTarget = NewObject<UMorphTarget>(Mesh, FName(*MorphTargetName));
        if (!MorphTarget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create morph target object"), TEXT("CREATION_FAILED"));
            return true;
        }
        MorphTarget->BaseSkelMesh = Mesh;
        bCreatedMorphTarget = true;
    }

    // MorphLODModels is protected in UE 5.6+; PopulateDeltas is the supported
    // editor path and needs the LOD's sections to map vertices onto sections.
    Mesh->Modify();
    MorphTarget->PopulateDeltas(Deltas, LODIndex, GetMorphTargetLodSections(Mesh, LODIndex), false, false);

    // Never report success for a morph target the engine would ensure() on.
    if (!MorphTarget->HasValidData())
    {
        if (bCreatedMorphTarget)
        {
            MorphTarget->MarkAsGarbage();
        }
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Morph target '%s' has no valid data after populating %d deltas; check that the vertex indices exist in LOD %d"),
                *MorphTargetName, Deltas.Num(), LODIndex),
            TEXT("INVALID_MORPH_DATA"));
        return true;
    }

    if (bCreatedMorphTarget)
    {
        Mesh->RegisterMorphTarget(MorphTarget);
    }
    Mesh->MarkPackageDirty();
    McpSafeAssetSave(Mesh);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("deltaCount"), Deltas.Num());
    Result->SetNumberField(TEXT("deltasApplied"), Deltas.Num());
    Result->SetNumberField(TEXT("lodIndex"), LODIndex);
    Result->SetBoolField(TEXT("created"), bCreatedMorphTarget);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Set %d deltas on morph target '%s'"), Deltas.Num(), *MorphTargetName), Result);
    return true;
}

#endif // WITH_EDITOR
