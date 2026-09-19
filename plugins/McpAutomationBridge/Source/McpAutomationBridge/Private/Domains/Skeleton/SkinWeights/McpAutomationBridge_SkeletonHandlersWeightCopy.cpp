#include "Domains/Skeleton/McpAutomationBridge_SkeletonHandlersActions.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersProjectPaths.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#if __has_include("Animation/SkinWeightProfile.h")
#include "Animation/SkinWeightProfile.h"
#endif

#if WITH_EDITOR

namespace McpSkeletonHandlers {

bool HandleCopyWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
        FString SourceMeshPath = GetJsonStringField(Payload, TEXT("sourceMeshPath"));
        FString TargetMeshPath = GetJsonStringField(Payload, TEXT("targetMeshPath"));
        FString ProfileName = GetJsonStringField(Payload, TEXT("profileName"));
        if (ProfileName.IsEmpty())
        {
            ProfileName = TEXT("CopiedWeights");
        }
        int32 LODIndex = 0;
        Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);

        if (SourceMeshPath.IsEmpty() || TargetMeshPath.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("sourceMeshPath and targetMeshPath are required"), TEXT("MISSING_PARAM"));
            return true;
        }

        // CRITICAL: Validate any extra path parameters for security and existence
        // This prevents false negatives where unused parameters contain invalid paths
        FString ExtraSkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
        if (!ExtraSkeletalMeshPath.IsEmpty())
        {
            FString SanitizedExtraPath = SanitizeProjectRelativePath(ExtraSkeletalMeshPath);
            if (SanitizedExtraPath.IsEmpty())
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("Invalid skeletalMeshPath parameter '%s': contains traversal sequences or invalid characters"), *ExtraSkeletalMeshPath),
                    TEXT("INVALID_PATH"));
                return true;
            }
            // Also verify the asset exists - this prevents false negatives when test provides invalid path
            UObject* ExtraMeshAsset = StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *ExtraSkeletalMeshPath);
            if (!ExtraMeshAsset)
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId,
                    FString::Printf(TEXT("skeletalMeshPath parameter '%s' does not exist"), *ExtraSkeletalMeshPath),
                    TEXT("MESH_NOT_FOUND"));
                return true;
            }
        }

        FString Error;
        USkeletalMesh* SourceMesh = LoadSkeletalMeshFromPathSkel(SourceMeshPath, Error);
        if (!SourceMesh)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Source mesh not found: %s"), *Error), TEXT("SOURCE_NOT_FOUND"));
            return true;
        }

        USkeletalMesh* TargetMesh = LoadSkeletalMeshFromPathSkel(TargetMeshPath, Error);
        if (!TargetMesh)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Target mesh not found: %s"), *Error), TEXT("TARGET_NOT_FOUND"));
            return true;
        }

#if WITH_EDITORONLY_DATA
        FSkeletalMeshModel* SourceModel = SourceMesh->GetImportedModel();
        FSkeletalMeshModel* TargetModel = TargetMesh->GetImportedModel();

        if (!SourceModel || !TargetModel ||
            LODIndex >= SourceModel->LODModels.Num() ||
            LODIndex >= TargetModel->LODModels.Num())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid LOD models"), TEXT("INVALID_LOD"));
            return true;
        }

        const FSkeletalMeshLODModel& SourceLOD = SourceModel->LODModels[LODIndex];
        const FSkeletalMeshLODModel& TargetLOD = TargetModel->LODModels[LODIndex];

        // This used to register the profile and fill it with memzero'd
        // FRawSkinWeight entries -- every influence bone 0, every weight 0 --
        // then Build() and SAVE it, while reporting the profile as "created".
        // Selecting that profile collapses the mesh onto the root bone, so the
        // call left the asset worse than it found it. The transfer itself was
        // never implemented (the original comment said as much). Refuse, and
        // write nothing.
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("sourceMeshPath"), SourceMesh->GetPathName());
        Result->SetStringField(TEXT("targetMeshPath"), TargetMesh->GetPathName());
        Result->SetStringField(TEXT("profileName"), ProfileName);
        Result->SetNumberField(TEXT("lodIndex"), LODIndex);
        Result->SetNumberField(TEXT("sourceVertexCount"), SourceLOD.NumVertices);
        Result->SetNumberField(TEXT("targetVertexCount"), TargetLOD.NumVertices);
        Result->SetBoolField(TEXT("profileCreated"), false);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("copy_weights is not implemented: transferring influences between meshes needs a per-vertex mapping this action does not build. Use the Skeletal Mesh Editor's Skin Weight Profile import (FSkinWeightProfileHelpers::ImportSkinWeightProfile) instead. Nothing was written to the target mesh."),
            Result, TEXT("NOT_SUPPORTED"));
        return true;
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("copy_weights requires editor mode"), TEXT("NOT_EDITOR"));
        return true;
#endif
}

} // namespace McpSkeletonHandlers

#endif // WITH_EDITOR
