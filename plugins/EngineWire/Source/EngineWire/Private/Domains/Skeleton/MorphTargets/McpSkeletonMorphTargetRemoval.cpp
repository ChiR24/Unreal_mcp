#include "Domains/Skeleton/Assets/McpSkeletonAssetLoading.h"
#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"
#include "Domains/Skeleton/SkinWeights/McpSkeletonSkinWeightSource.h"

#include "Animation/MorphTarget.h"
#include "AssetCompilingManager.h"
#include "Engine/SkeletalMesh.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#if MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#else
#include "Rendering/SkeletalMeshLODImporterData.h"
#endif

using namespace McpSkeletonHandlers;

// delete_morph_target. A morph lives in the mesh's source data (every imported one, and every one create, set_deltas and
// import write) and the render morph is built from it: UnregisterMorphTarget alone dropped the render morph and its own
// rebuild made it again, so the reply said deleted while the mesh kept it. The morph leaves the source data of every LOD
// that holds it and the mesh is rebuilt; one only the render data has goes with that rebuild.
namespace
{
void RemoveSourceMorph(USkeletalMesh* Mesh, FName Name)
{
    for (int32 LOD = 0; LOD < Mesh->GetLODNum(); ++LOD)
    {
#if MCP_SKIN_SOURCE_IS_MESH_DESCRIPTION
        FMeshDescription* Description = Mesh->HasMeshDescription(LOD) ? Mesh->GetMeshDescription(LOD) : nullptr;
        if (Description && FSkeletalMeshAttributes(*Description).UnregisterMorphTargetAttribute(Name))
        {
            Mesh->CommitMeshDescription(LOD);
        }
#else
        if (!Mesh->IsLODImportedDataBuildAvailable(LOD))
        {
            continue;
        }
        FSkeletalMeshImportData Data;
        Mesh->LoadLODImportedData(LOD, Data);
        const int32 Slot = Data.MorphTargetNames.IndexOfByKey(Name.ToString());
        if (Slot == INDEX_NONE)
        {
            continue;
        }
        Data.MorphTargetNames.RemoveAt(Slot);
        if (Data.MorphTargets.IsValidIndex(Slot)) Data.MorphTargets.RemoveAt(Slot);
        if (Data.MorphTargetModifiedPoints.IsValidIndex(Slot)) Data.MorphTargetModifiedPoints.RemoveAt(Slot);
        Mesh->SaveLODImportedData(LOD, Data);
#endif
    }
}
}

bool UEngineWireSubsystem::HandleDeleteMorphTarget(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    const FString MorphTargetName = GetJsonStringField(Payload, TEXT("morphTargetName"));
    if (SkeletalMeshPath.IsEmpty() || MorphTargetName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("skeletalMeshPath and morphTargetName are required"), TEXT("MISSING_PARAMETER"));
        return true;
    }

    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        SendAutomationError(RequestingSocket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }
    const FName Name(*MorphTargetName);
    UMorphTarget* TargetToRemove = Mesh->FindMorphTarget(Name);
    if (!TargetToRemove)
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Morph target '%s' not found"), *MorphTargetName), TEXT("MORPH_NOT_FOUND"));
        return true;
    }

    // The rebuild drops every morph only the render data has: the one asked for goes either way, any other is refused
    // as the writers refuse it, unless dropRenderOnlyMorphs.
    McpSkinSource::FSourceMesh Source;
    FString Code;
    const bool bHasSource = McpSkinSource::Read(Mesh, 0, true, Source, Code, Error);
    if (!bHasSource && Code != TEXT("SOURCE_DATA_MISSING"))
    {
        SendAutomationError(RequestingSocket, RequestId, Error, Code);
        return true;
    }
    TArray<TSharedPtr<FJsonValue>> Dropped;
    if (bHasSource)
    {
        Source.Morphs.FindOrAdd(Name);
        if (!McpSkinSource::CheckRenderOnlyMorphs(Mesh, Source, GetJsonBoolField(Payload, TEXT("dropRenderOnlyMorphs"), false), Dropped, Error))
        {
            SendAutomationError(RequestingSocket, RequestId, Error, TEXT("RENDER_ONLY_MORPHS"));
            return true;
        }
    }

    Mesh->Modify();
    if (bHasSource)
    {
        // A null-property PostEditChange rebuilds the render data from the source data; the build may run async.
        RemoveSourceMorph(Mesh, Name);
        Mesh->PostEditChange();
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    else
    {
        // A mesh without source data keeps its morphs in the render data alone.
        Mesh->UnregisterMorphTarget(TargetToRemove);
    }
    Mesh->MarkPackageDirty();
    if (Mesh->FindMorphTarget(Name))
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(
            TEXT("%s still has morph target '%s' after its rebuild."), *Mesh->GetName(), *MorphTargetName), TEXT("MORPH_NOT_DELETED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    Result->SetNumberField(TEXT("remainingMorphTargets"), Mesh->GetMorphTargets().Num());
    if (Dropped.Num() > 0)
    {
        Result->SetArrayField(TEXT("morphTargetsDropped"), Dropped);
    }
    SaveAndReport(Mesh, Payload, Result);
    SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Morph target '%s' deleted"), *MorphTargetName), Result);
    return true;
}
