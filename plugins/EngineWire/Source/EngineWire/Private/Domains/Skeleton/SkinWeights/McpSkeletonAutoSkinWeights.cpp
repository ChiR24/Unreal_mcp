#include "Domains/Skeleton/Assets/McpSkeletonAssetLoading.h"
#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"
#include "Domains/Skeleton/SkinWeights/McpSkeletonSkinWeightSource.h"

#include "Engine/SkeletalMesh.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
// Smooth binding comes from GeometryScripting; the calls below all exist from
// UE 5.5 on (the same gate skin_mesh_to_skeleton uses).
#if __has_include("GeometryScript/MeshAssetFunctions.h") && \
    __has_include("GeometryScript/MeshBoneWeightFunctions.h") && \
    ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#define MCP_AUTO_SKIN_HAS_GEOMETRY_SCRIPT 1
#include "Domains/Geometry/McpGeometryScriptGuard.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"
#include "UDynamicMesh.h"
#else
#define MCP_AUTO_SKIN_HAS_GEOMETRY_SCRIPT 0
#endif


namespace McpSkeletonHandlers {

bool HandleAutoSkinWeightsAction(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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
        if (!McpGeometryScriptReady())
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId, McpGeometryScriptMissing, McpGeometryScriptMissingCode);
            return true;
        }
        USkeleton* Skeleton = Mesh->GetSkeleton();
        if (!Skeleton)
        {
            Subsystem->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("%s has no skeleton to bind to"), *SkeletalMeshPath), TEXT("SKELETON_NOT_FOUND"));
            return true;
        }
        // The rebuild below drops a morph target only the render data has, as every source writer's does (they refuse
        // it the same way), and before UE 5.7 the round trip through the dynamic mesh carries no morph target at all
        // (FMeshDescriptionToDynamicMesh reads none on 5.5 and 5.6): refused before anything changes.
        McpSkinSource::FSourceMesh Source;
        FString SourceCode;
        if (McpSkinSource::Read(Mesh, 0, true, Source, SourceCode, Error))
        {
            TArray<TSharedPtr<FJsonValue>> Dropped;
            if (!McpSkinSource::CheckRenderOnlyMorphs(Mesh, Source, GetJsonBoolField(Payload, TEXT("dropRenderOnlyMorphs"), false), Dropped, Error))
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("RENDER_ONLY_MORPHS"));
                return true;
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 7
            TArray<FString> MorphNames;
            for (const TPair<FName, TArray<FVector3f>>& Morph : Source.Morphs)
            {
                MorphNames.Add(Morph.Key.ToString());
            }
            if (MorphNames.Num() > 0)
            {
                Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("On this engine version the "
                    "smooth binding rebuilds LOD 0 without its morph targets (%s), so nothing was changed."),
                    *FString::Join(MorphNames, TEXT(", "))), TEXT("MORPHS_WOULD_BE_LOST"));
                return true;
            }
#endif
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

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
        // Verification counts so the rebuild is checkable.
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

        SaveAndReport(Mesh, Payload, Result);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Skin weights recomputed by smooth binding on LOD 0"), Result);
        return true;
#endif
}

} // namespace McpSkeletonHandlers

