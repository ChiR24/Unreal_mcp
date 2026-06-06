#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Animation/MorphTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
using namespace McpSkeletonHandlers;

bool UMcpAutomationBridgeSubsystem::HandleImportMorphTargets(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    FString SourceFilePath = GetStringFieldSkel(Payload, TEXT("morphTargetPath"));
    if (SourceFilePath.IsEmpty())
    {
        SourceFilePath = GetStringFieldSkel(Payload, TEXT("sourcePath"));
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

    // If source file provided, import from it
    if (!SourceFilePath.IsEmpty() && FPaths::FileExists(SourceFilePath))
    {
        // Note: Full FBX import for morph targets requires FbxImporter
        // This is a simplified response indicating the operation is queued
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("FBX morph target import requires using the asset import pipeline. Use manage_asset import action with the FBX file."),
            TEXT("USE_ASSET_IMPORT"));
        return true;
    }

    // Return current morph targets as info
    TArray<TSharedPtr<FJsonValue>> MorphTargetArray;
    for (UMorphTarget* MT : Mesh->GetMorphTargets())
    {
        if (!MT) continue;

        TSharedPtr<FJsonObject> MTObj = McpHandlerUtils::CreateResultObject();
        MTObj->SetStringField(TEXT("name"), MT->GetName());
        MorphTargetArray.Add(MakeShared<FJsonValueObject>(MTObj));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("morphTargets"), MorphTargetArray);
    Result->SetNumberField(TEXT("count"), MorphTargetArray.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Use manage_asset import to import morph targets from FBX"), Result);
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetMorphTargetValue(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString ActorName = GetStringFieldSkel(Payload, TEXT("actorName"));
    FString MorphTargetName = GetStringFieldSkel(Payload, TEXT("morphTargetName"));
    double Value = GetNumberFieldSkel(Payload, TEXT("value"), 0.0);
    bool bAddMissing = GetBoolFieldSkel(Payload, TEXT("addMissing"), false);

    if (ActorName.IsEmpty() || MorphTargetName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("actorName and morphTargetName are required"), TEXT("MISSING_PARAM"));
        return true;
    }

    // Clamp value to valid range
    Value = FMath::Clamp(Value, 0.0, 1.0);

    // Find the actor
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

    // Find skeletal mesh component
    USkeletalMeshComponent* SkelMeshComp = FoundActor->FindComponentByClass<USkeletalMeshComponent>();
    if (!SkelMeshComp)
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Actor does not have a SkeletalMeshComponent"), TEXT("NO_SKEL_MESH_COMP"));
        return true;
    }

    // Check if morph target exists on the mesh
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    USkeletalMesh* SkelMesh = SkelMeshComp->GetSkeletalMeshAsset();
#else
    // UE 5.0: Use SkeletalMesh property directly
    USkeletalMesh* SkelMesh = SkelMeshComp->SkeletalMesh;
#endif
    if (SkelMesh)
    {
        bool bHasMorphTarget = false;
        for (const UMorphTarget* MT : SkelMesh->GetMorphTargets())
        {
            if (MT && MT->GetFName() == FName(*MorphTargetName))
            {
                bHasMorphTarget = true;
                break;
            }
        }

        if (!bHasMorphTarget && !bAddMissing)
        {
            SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Morph target '%s' not found on mesh"), *MorphTargetName),
                TEXT("MORPH_TARGET_NOT_FOUND"));
            return true;
        }
    }

    // Set the morph target value
    SkelMeshComp->SetMorphTarget(FName(*MorphTargetName), static_cast<float>(Value));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("actorName"), ActorName);
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("value"), Value);

    // Get current morph target weights for reporting
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
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("set_morph_target_value requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleListMorphTargets(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
    if (SkeletalMeshPath.IsEmpty())
    {
        SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("meshPath"));
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
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("list_morph_targets requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleDeleteMorphTarget(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
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

    // Find the morph target
    UMorphTarget* TargetToRemove = nullptr;
    int32 Index = INDEX_NONE;
    for (int32 i = 0; i < Mesh->GetMorphTargets().Num(); ++i)
    {
        if (Mesh->GetMorphTargets()[i] && Mesh->GetMorphTargets()[i]->GetFName() == FName(*MorphTargetName))
        {
            TargetToRemove = Mesh->GetMorphTargets()[i];
            Index = i;
            break;
        }
    }

    if (!TargetToRemove || Index == INDEX_NONE)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Morph target '%s' not found"), *MorphTargetName), TEXT("MORPH_NOT_FOUND"));
        return true;
    }

    // Remove the morph target
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
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("delete_morph_target requires editor mode"), TEXT("NOT_EDITOR"));
    return true;
#endif
}

#endif // WITH_EDITOR
