#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleCreateMorphTarget(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString MorphTargetName = GetStringFieldSkel(Payload, TEXT("morphTargetName"));

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

    // Check if morph target already exists
    UMorphTarget* ExistingMorph = Mesh->FindMorphTarget(FName(*MorphTargetName));
    if (ExistingMorph)
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
        Result->SetBoolField(TEXT("alreadyExists"), true);

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Morph target '%s' already exists"), *MorphTargetName), Result);
        return true;
    }

    // CRITICAL FIX: UE 5.7 requires morph targets to have valid delta data BEFORE registration.
    // RegisterMorphTarget() internally checks HasValidData() and fires an ensure() for empty morphs.
    // We must either:
    // 1. Provide deltas and populate them BEFORE registering, OR
    // 2. Return EMPTY_MORPH_TARGET error immediately without creating the morph target

    // Check if deltas parameter is provided
    const TArray<TSharedPtr<FJsonValue>>* DeltasArray = nullptr;
    bool bHasDeltas = Payload->TryGetArrayField(TEXT("deltas"), DeltasArray) && DeltasArray && DeltasArray->Num() > 0;

    if (!bHasDeltas)
    {
        // No deltas provided - cannot create a valid morph target in UE 5.7+
        // Return error WITHOUT creating/registering to avoid engine ensure failure
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Morph target '%s' requires vertex deltas. Provide 'deltas' array with vertex indices and position offsets. Example: {\"deltas\": [{\"vertexIndex\": 0, \"positionDelta\": {\"x\": 1, \"y\": 0, \"z\": 0}}]}"), *MorphTargetName),
            TEXT("EMPTY_MORPH_TARGET"));
        return true;
    }

    // Parse deltas array
    TArray<FMorphTargetDelta> Deltas;
    for (const TSharedPtr<FJsonValue>& DeltaValue : *DeltasArray)
    {
        const TSharedPtr<FJsonObject>* DeltaObj = nullptr;
        if (DeltaValue->TryGetObject(DeltaObj) && DeltaObj && DeltaObj->IsValid())
        {
            FMorphTargetDelta Delta;

            double VertexIndex = 0;
            (*DeltaObj)->TryGetNumberField(TEXT("vertexIndex"), VertexIndex);
            Delta.SourceIdx = static_cast<uint32>(VertexIndex);

            const TSharedPtr<FJsonObject>* PositionDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("positionDelta"), PositionDelta) && PositionDelta && PositionDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*PositionDelta)->TryGetNumberField(TEXT("x"), X);
                (*PositionDelta)->TryGetNumberField(TEXT("y"), Y);
                (*PositionDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.PositionDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            const TSharedPtr<FJsonObject>* TangentDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("tangentDelta"), TangentDelta) && TangentDelta && TangentDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*TangentDelta)->TryGetNumberField(TEXT("x"), X);
                (*TangentDelta)->TryGetNumberField(TEXT("y"), Y);
                (*TangentDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.TangentZDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            Deltas.Add(Delta);
        }
    }

    if (Deltas.Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Deltas array was provided but contained no valid delta entries. Each delta must have vertexIndex and positionDelta."),
            TEXT("INVALID_MORPH_DATA"));
        return true;
    }

    // Create new morph target
    UMorphTarget* NewMorphTarget = NewObject<UMorphTarget>(Mesh, FName(*MorphTargetName));
    if (!NewMorphTarget)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create morph target object"), TEXT("CREATION_FAILED"));
        return true;
    }

    // Set BaseSkelMesh - required for HasValidData() to work properly
    NewMorphTarget->BaseSkelMesh = Mesh;

    // Get LOD index (default to 0)
    int32 LODIndex = 0;
    Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);

    // Populate deltas BEFORE registering - this is critical for UE 5.7+
    // PopulateDeltas requires the sections array from the skeletal mesh LOD model
#if WITH_EDITOR
    const FSkeletalMeshModel* SkelMeshModel = Mesh->GetImportedModel();
    TArray<FSkelMeshSection> Sections;
    if (SkelMeshModel && SkelMeshModel->LODModels.IsValidIndex(LODIndex))
    {
        const FSkeletalMeshLODModel& LODModel = SkelMeshModel->LODModels[LODIndex];
        Sections = LODModel.Sections;
    }

    NewMorphTarget->PopulateDeltas(Deltas, LODIndex, Sections, false, false);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Morph target creation with deltas requires editor"), TEXT("NOT_SUPPORTED"));
    return true;
#endif

    // NOW validate that we have valid data
    if (!NewMorphTarget->HasValidData())
    {
        // This shouldn't happen if deltas were valid, but check anyway
        NewMorphTarget->MarkAsGarbage();

        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Morph target '%s' has no valid data after populating deltas. Check vertex indices are valid."), *MorphTargetName),
            TEXT("INVALID_MORPH_DATA"));
        return true;
    }

    // Only register AFTER the morph target has valid data
    Mesh->RegisterMorphTarget(NewMorphTarget);

    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("morphTargetCount"), Mesh->GetMorphTargets().Num());
    Result->SetNumberField(TEXT("deltaCount"), Deltas.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Morph target '%s' created with %d deltas"), *MorphTargetName, Deltas.Num()), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetMorphTargetDeltas(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString MorphTargetName = GetStringFieldSkel(Payload, TEXT("morphTargetName"));

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

    UMorphTarget* MorphTarget = Mesh->FindMorphTarget(FName(*MorphTargetName));
    bool bCreatedMorphTarget = false;

    // Parse deltas array
    const TArray<TSharedPtr<FJsonValue>>* DeltasArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("deltas"), DeltasArray) || !DeltasArray)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("deltas array is required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Build delta vertices
    TArray<FMorphTargetDelta> Deltas;
    for (const TSharedPtr<FJsonValue>& DeltaValue : *DeltasArray)
    {
        const TSharedPtr<FJsonObject>* DeltaObj = nullptr;
        if (DeltaValue->TryGetObject(DeltaObj) && DeltaObj && DeltaObj->IsValid())
        {
            FMorphTargetDelta Delta;

            double VertexIndex = 0;
            (*DeltaObj)->TryGetNumberField(TEXT("vertexIndex"), VertexIndex);
            Delta.SourceIdx = static_cast<uint32>(VertexIndex);

            const TSharedPtr<FJsonObject>* PositionDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("positionDelta"), PositionDelta) && PositionDelta && PositionDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*PositionDelta)->TryGetNumberField(TEXT("x"), X);
                (*PositionDelta)->TryGetNumberField(TEXT("y"), Y);
                (*PositionDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.PositionDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            const TSharedPtr<FJsonObject>* TangentDelta = nullptr;
            if ((*DeltaObj)->TryGetObjectField(TEXT("tangentDelta"), TangentDelta) && TangentDelta && TangentDelta->IsValid())
            {
                double X = 0, Y = 0, Z = 0;
                (*TangentDelta)->TryGetNumberField(TEXT("x"), X);
                (*TangentDelta)->TryGetNumberField(TEXT("y"), Y);
                (*TangentDelta)->TryGetNumberField(TEXT("z"), Z);
                Delta.TangentZDelta = FVector3f(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z));
            }

            Deltas.Add(Delta);
        }
    }

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

    // Apply deltas to morph target
    // MorphLODModels is protected in UE 5.6+, use PopulateDeltas() for proper editor workflow
#if WITH_EDITOR
    // Use PopulateDeltas - the proper API for morph target manipulation
    // This handles all internal data structures correctly
    TArray<FSkelMeshSection> EmptySections; // PopulateDeltas can work with empty sections array
    MorphTarget->PopulateDeltas(Deltas, 0, EmptySections, false, false);
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Morph target manipulation requires editor"), TEXT("NOT_SUPPORTED"));
    return true;
#endif


    // Validate morph target has valid data after setting deltas
    // This prevents returning success for morph targets that trigger Engine Ensures
    if (!MorphTarget->HasValidData())
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Morph target '%s' has no valid data - deltas may be empty or invalid"), *MorphTargetName),
            TEXT("INVALID_MORPH_DATA"));
        return true;
    }

    if (bCreatedMorphTarget)
    {
        Mesh->RegisterMorphTarget(MorphTarget);
    }

    McpSafeAssetSave(Mesh);

    // Save if requested
    bool bSave = false;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave)
    {
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("deltaCount"), Deltas.Num());
    Result->SetBoolField(TEXT("created"), bCreatedMorphTarget);

    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Set %d deltas on morph target '%s'"), Deltas.Num(), *MorphTargetName), Result);
    return true;
}

#endif // WITH_EDITOR
