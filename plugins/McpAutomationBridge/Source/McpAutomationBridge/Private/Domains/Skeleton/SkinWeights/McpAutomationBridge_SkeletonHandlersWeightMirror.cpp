#include "Domains/Skeleton/McpAutomationBridge_SkeletonHandlersActions.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Engine/SkeletalMesh.h"
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

bool HandleMirrorWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
        FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
        FString Axis = GetJsonStringField(Payload, TEXT("axis"));
        if (Axis.IsEmpty())
        {
            Axis = TEXT("X");
        }
        FString ProfileName = GetJsonStringField(Payload, TEXT("profileName"));
        if (ProfileName.IsEmpty())
        {
            ProfileName = TEXT("MirroredWeights");
        }

        if (SkeletalMeshPath.IsEmpty())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("skeletalMeshPath is required"), TEXT("MISSING_PARAM"));
            return true;
        }

        FString Error;
        USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
        if (!Mesh)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
            return true;
        }

#if WITH_EDITORONLY_DATA
        FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
        if (!ImportedModel || ImportedModel->LODModels.Num() == 0)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Mesh has no LOD models"), TEXT("NO_LOD_MODELS"));
            return true;
        }

        int32 LODIndex = 0;
        Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);

        if (!ImportedModel->LODModels.IsValidIndex(LODIndex))
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("LOD index %d out of range (max: %d)"), LODIndex, ImportedModel->LODModels.Num() - 1),
                TEXT("INVALID_LOD"));
            return true;
        }
        const FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];

        // Same defect as copy_weights: this registered the profile, filled it
        // with memzero'd FRawSkinWeight entries and SAVED the mesh, so the
        // asset gained a profile that collapses it onto the root bone -- and
        // the reply called it created. The mirroring itself was never written
        // (the original comment said it "would need vertex position data").
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("skeletalMeshPath"), Mesh->GetPathName());
        Result->SetStringField(TEXT("profileName"), ProfileName);
        Result->SetStringField(TEXT("axis"), Axis);
        Result->SetNumberField(TEXT("lodIndex"), LODIndex);
        Result->SetNumberField(TEXT("vertexCount"), LODModel.NumVertices);
        Result->SetBoolField(TEXT("profileCreated"), false);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("mirror_weights is not implemented: mirroring influences needs per-vertex positions and a left/right bone mapping this action does not build. Use the Skeletal Mesh Editor's mirroring tools instead. Nothing was written to the mesh."),
            Result, TEXT("NOT_SUPPORTED"));
        return true;
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("mirror_weights requires editor mode"), TEXT("NOT_EDITOR"));
        return true;
#endif
}

} // namespace McpSkeletonHandlers

#endif // WITH_EDITOR
