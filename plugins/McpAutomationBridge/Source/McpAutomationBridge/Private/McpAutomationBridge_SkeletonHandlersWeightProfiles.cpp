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

bool HandleSetVertexWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));
        FString ProfileName = GetStringFieldSkel(Payload, TEXT("profileName"));
        if (ProfileName.IsEmpty())
        {
            ProfileName = TEXT("CustomWeights");
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

        // Parse weights array
        const TArray<TSharedPtr<FJsonValue>>* WeightsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("weights"), WeightsArray) || !WeightsArray)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("weights array is required"), TEXT("MISSING_PARAM"));
            return true;
        }

#if WITH_EDITORONLY_DATA
        // Access the LOD model for editing
        FSkeletalMeshModel* ImportedModel = Mesh->GetImportedModel();
        if (!ImportedModel || ImportedModel->LODModels.Num() == 0)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Mesh has no LOD models"), TEXT("NO_LOD_MODELS"));
            return true;
        }

        int32 LODIndex = 0;
        Payload->TryGetNumberField(TEXT("lodIndex"), LODIndex);

        if (LODIndex >= ImportedModel->LODModels.Num())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("LOD index %d out of range (max: %d)"), LODIndex, ImportedModel->LODModels.Num() - 1),
                TEXT("INVALID_LOD"));
            return true;
        }

        FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[LODIndex];

        // Create or update skin weight profile
        FSkinWeightProfileInfo* ProfileInfo = nullptr;
        for (FSkinWeightProfileInfo& Info : Mesh->GetSkinWeightProfiles())
        {
            if (Info.Name == FName(*ProfileName))
            {
                ProfileInfo = &Info;
                break;
            }
        }

        if (!ProfileInfo)
        {
            // Add new profile
            FSkinWeightProfileInfo NewProfile;
            NewProfile.Name = FName(*ProfileName);
            Mesh->AddSkinWeightProfile(NewProfile);
        }

        // Build FImportedSkinWeightProfileData from weights array
        FImportedSkinWeightProfileData& ProfileData = LODModel.SkinWeightProfiles.FindOrAdd(FName(*ProfileName));
        ProfileData.SkinWeights.SetNum(LODModel.NumVertices);

        int32 WeightsSet = 0;
        for (const TSharedPtr<FJsonValue>& WeightValue : *WeightsArray)
        {
            const TSharedPtr<FJsonObject>* WeightObj = nullptr;
            if (!WeightValue->TryGetObject(WeightObj) || !WeightObj || !WeightObj->IsValid())
            {
                continue;
            }

            int32 VertexIndex = 0;
            (*WeightObj)->TryGetNumberField(TEXT("vertexIndex"), VertexIndex);

            if (VertexIndex < 0 || VertexIndex >= static_cast<int32>(LODModel.NumVertices))
            {
                continue;
            }

            FRawSkinWeight& SkinWeight = ProfileData.SkinWeights[VertexIndex];
            FMemory::Memzero(&SkinWeight, sizeof(FRawSkinWeight));

            // Parse bone influences
            const TArray<TSharedPtr<FJsonValue>>* InfluencesArray = nullptr;
            if ((*WeightObj)->TryGetArrayField(TEXT("influences"), InfluencesArray) && InfluencesArray)
            {
                int32 InfluenceIndex = 0;
                for (const TSharedPtr<FJsonValue>& InfluenceValue : *InfluencesArray)
                {
                    if (InfluenceIndex >= MAX_TOTAL_INFLUENCES) break;

                    const TSharedPtr<FJsonObject>* InfluenceObj = nullptr;
                    if (InfluenceValue->TryGetObject(InfluenceObj) && InfluenceObj && InfluenceObj->IsValid())
                    {
                        int32 BoneIndex = 0;
                        double Weight = 0.0;
                        (*InfluenceObj)->TryGetNumberField(TEXT("boneIndex"), BoneIndex);
                        (*InfluenceObj)->TryGetNumberField(TEXT("weight"), Weight);

                        SkinWeight.InfluenceBones[InfluenceIndex] = static_cast<FBoneIndexType>(BoneIndex);
                        SkinWeight.InfluenceWeights[InfluenceIndex] = static_cast<uint16>(FMath::Clamp(Weight, 0.0, 1.0) * 65535.0);
                        InfluenceIndex++;
                    }
                }
            }

            WeightsSet++;
        }

        // Rebuild the mesh with the new skin weight profile
        Mesh->Build();
        McpSafeAssetSave(Mesh);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        Result->SetStringField(TEXT("profileName"), ProfileName);
        Result->SetNumberField(TEXT("verticesModified"), WeightsSet);
        Result->SetNumberField(TEXT("lodIndex"), LODIndex);

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Set weights for %d vertices in profile '%s'"), WeightsSet, *ProfileName), Result);
        return true;
#else
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("set_vertex_weights requires editor mode"), TEXT("NOT_EDITOR"));
        return true;
#endif
}

bool HandleAutoSkinWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
// Auto skin weights computation - typically done during import
        // We trigger a mesh rebuild which recalculates default weights
        FString SkeletalMeshPath = GetStringFieldSkel(Payload, TEXT("skeletalMeshPath"));

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

        // Rebuild the mesh - this recalculates skin weights based on bone positions
        Mesh->Build();
        McpSafeAssetSave(Mesh);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        Result->SetBoolField(TEXT("rebuilt"), true);

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Mesh rebuilt with recalculated skin weights"), Result);
        return true;
}

} // namespace McpSkeletonHandlers

#endif // WITH_EDITOR
