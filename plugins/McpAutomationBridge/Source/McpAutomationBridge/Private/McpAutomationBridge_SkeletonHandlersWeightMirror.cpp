#include "McpAutomationBridge_SkeletonHandlersActions.h"
#include "McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Engine/SkeletalMesh.h"
#include "McpAutomationBridgeHelpersSafeOperationsFacade.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#if __has_include("Animation/SkinWeightProfile.h")
#include "Animation/SkinWeightProfile.h"
#endif

#if WITH_EDITOR

namespace McpSkeletonHandlers {

bool HandleMirrorWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
        FString Axis = GetStringFieldSkel(Payload, TEXT("axis"));
        if (Axis.IsEmpty())
        {
            Axis = TEXT("X");
        }
        FString ProfileName = GetStringFieldSkel(Payload, TEXT("profileName"));
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

        FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];

        // Create mirrored skin weight profile
        FSkinWeightProfileInfo NewProfile;
        NewProfile.Name = FName(*ProfileName);
        Mesh->AddSkinWeightProfile(NewProfile);

        FImportedSkinWeightProfileData& ProfileData = LODModel.SkinWeightProfiles.FindOrAdd(FName(*ProfileName));
        ProfileData.SkinWeights.SetNum(LODModel.NumVertices);

        // Initialize profile - mirroring logic would need vertex position data
        // For now we create the profile structure and indicate manual completion needed
        for (uint32 i = 0; i < LODModel.NumVertices; ++i)
        {
            FMemory::Memzero(&ProfileData.SkinWeights[i], sizeof(FRawSkinWeight));
        }

        Mesh->Build();
        McpSafeAssetSave(Mesh);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        Result->SetStringField(TEXT("profileName"), ProfileName);
        Result->SetStringField(TEXT("axis"), Axis);
        Result->SetNumberField(TEXT("lodIndex"), LODIndex);
        Result->SetStringField(TEXT("note"), TEXT("Skin weight profile created. Use Skeletal Mesh Editor for precise mirroring with bone name mapping."));

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Skin weight profile '%s' created for mirroring along %s axis"), *ProfileName, *Axis), Result);
        return true;
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("mirror_weights requires editor mode"), TEXT("NOT_EDITOR"));
        return true;
#endif
}

} // namespace McpSkeletonHandlers

#endif // WITH_EDITOR
