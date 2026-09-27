#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"

#include "Engine/SkeletalMesh.h"
#include "Safety/McpSafeOperations.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#if __has_include("Animation/SkinWeightProfile.h")
#include "Animation/SkinWeightProfile.h"
#endif
// Smooth binding comes from GeometryScripting; the calls below all exist from
// UE 5.5 on (the same gate skin_mesh_to_skeleton uses).
#if __has_include("GeometryScript/MeshAssetFunctions.h") && \
    __has_include("GeometryScript/MeshBoneWeightFunctions.h") && \
    ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#define MCP_AUTO_SKIN_HAS_GEOMETRY_SCRIPT 1
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"
#include "UDynamicMesh.h"
#else
#define MCP_AUTO_SKIN_HAS_GEOMETRY_SCRIPT 0
#endif


namespace McpSkeletonHandlers {

bool HandleSetVertexWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
        FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
        FString ProfileName = GetJsonStringField(Payload, TEXT("profileName"));
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

        const TArray<TSharedPtr<FJsonValue>>* WeightsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("weights"), WeightsArray) || !WeightsArray)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("weights array is required"), TEXT("MISSING_PARAM"));
            return true;
        }

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
            FSkinWeightProfileInfo NewProfile;
            NewProfile.Name = FName(*ProfileName);
            Mesh->AddSkinWeightProfile(NewProfile);
        }

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

        Mesh->Build();
        SaveIfRequested(Mesh, Payload);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        Result->SetStringField(TEXT("profileName"), ProfileName);
        Result->SetNumberField(TEXT("verticesModified"), WeightsSet);
        Result->SetNumberField(TEXT("lodIndex"), LODIndex);

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Set weights for %d vertices in profile '%s'"), WeightsSet, *ProfileName), Result);
        return true;
}

bool HandleAutoSkinWeightsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
        FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));

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

        // Mesh->Build() only rebuilds render data from the weights the mesh
        // already has, yet this used to answer "recalculated skin weights".
        // Recompute LOD 0 by smooth binding against the mesh's own skeleton.
#if !MCP_AUTO_SKIN_HAS_GEOMETRY_SCRIPT
        Subsystem->SendAutomationError(RequestingSocket, RequestId,
            TEXT("auto_skin_weights needs the GeometryScripting plugin on UE 5.5 or later; set weights explicitly with set_vertex_weights instead"),
            TEXT("NOT_SUPPORTED"));
        return true;
#else
        USkeleton* Skeleton = Mesh->GetSkeleton();
        if (!Skeleton)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("%s has no skeleton to bind to"), *SkeletalMeshPath), TEXT("SKELETON_NOT_FOUND"));
            return true;
        }
        UDynamicMesh* Working = NewObject<UDynamicMesh>();
        EGeometryScriptOutcomePins ReadOutcome = EGeometryScriptOutcomePins::Failure;
        UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(
            Mesh, Working, FGeometryScriptCopyMeshFromAssetOptions(), FGeometryScriptMeshReadLOD(), ReadOutcome);
        if (ReadOutcome != EGeometryScriptOutcomePins::Success)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Could not read LOD 0 geometry from %s"), *SkeletalMeshPath), TEXT("MESH_READ_FAILED"));
            return true;
        }
        UGeometryScriptLibrary_MeshBoneWeightFunctions::ComputeSmoothBoneWeights(
            Working, Skeleton, FGeometryScriptSmoothBoneWeightsOptions());
        // Keep the source normals, tangents and vertex order: recomputing them
        // or reordering vertices breaks the UV correspondence.
        FGeometryScriptCopyMeshToAssetOptions WriteOptions;
        WriteOptions.bEnableRecomputeNormals = false;
        WriteOptions.bEnableRecomputeTangents = false;
        WriteOptions.bUseOriginalVertexOrder = true;
        EGeometryScriptOutcomePins WriteOutcome = EGeometryScriptOutcomePins::Failure;
        Mesh->Modify();
        UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(
            Working, Mesh, WriteOptions, FGeometryScriptMeshWriteLOD(), WriteOutcome);
        if (WriteOutcome != EGeometryScriptOutcomePins::Success)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Could not write the recomputed weights back to %s"), *SkeletalMeshPath), TEXT("MESH_WRITE_FAILED"));
            return true;
        }
        Mesh->PostEditChange();
        Mesh->MarkPackageDirty();
        SaveIfRequested(Mesh, Payload);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        // Verification counts so the rebuild is checkable (dogfood #99).
        if (FSkeletalMeshModel* Model = Mesh->GetImportedModel())
        {
            if (Model->LODModels.Num() > 0)
            {
                Result->SetNumberField(TEXT("vertexCount"), Model->LODModels[0].NumVertices);
                Result->SetNumberField(TEXT("sectionCount"), Model->LODModels[0].Sections.Num());
                Result->SetNumberField(TEXT("maxBoneInfluences"), Model->LODModels[0].GetMaxBoneInfluences());
            }
            Result->SetNumberField(TEXT("lodCount"), Model->LODModels.Num());
        }
        Result->SetNumberField(TEXT("boneCount"), Mesh->GetRefSkeleton().GetNum());
        Result->SetStringField(TEXT("weights"), TEXT("smooth"));
        Result->SetNumberField(TEXT("lodIndex"), 0);

        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Skin weights recomputed by smooth binding on LOD 0"), Result);
        return true;
#endif
}

} // namespace McpSkeletonHandlers

