#include "Domains/Skeleton/Assets/McpSkeletonAssetLoading.h"
#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"
#include "Domains/Skeleton/MorphTargets/McpSkeletonMorphTargetDeltas.h"
#include "Domains/Skeleton/SkinWeights/McpSkeletonSkinWeightSource.h"

#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Safety/McpSafeOperations.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "UObject/Package.h"

using namespace McpSkeletonHandlers;

// create_morph_target and set_morph_target_deltas write the morph into the mesh's source data, so the rebuild that
// follows makes it and every later rebuild keeps it. A morph registered from render data alone is dropped by the next
// rebuild from the source data, and from UE 5.4 RegisterMorphTarget rebuilds at once: the reply said created while
// the mesh kept no morph. Only a mesh without source data still takes a render-data morph.
namespace
{
// Writes Deltas (render vertices of LOD) as morph Name into Source and rebuilds the mesh. Every source vertex at a
// render vertex's place moves with it: a seam splits one source vertex into several render vertices. A refusal
// changes nothing.
bool WriteSourceMorph(USkeletalMesh* Mesh, int32 LOD, FName Name, const TArray<FMorphTargetDelta>& Deltas,
                      McpSkinSource::FSourceMesh& Source, bool bAllowDrop, FString& OutCode, FString& OutError)
{
    TArray<FSoftSkinVertex> Vertices;
    const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
    if (Model && Model->LODModels.IsValidIndex(LOD))
    {
        Model->LODModels[LOD].GetVertices(Vertices);
    }
    const float Diagonal = McpSkinSource::BoundsDiagonal(Source.Positions);
    const McpSkinSource::FNearest Grid(Source.Positions, Diagonal * 0.01f);
    const float Tolerance = FMath::Max(Diagonal * 1e-5f, 1e-4f);
    TArray<FVector3f> Morph;
    Morph.Init(FVector3f::ZeroVector, Source.Positions.Num());
    TArray<int32> Matches;
    for (const FMorphTargetDelta& Delta : Deltas)
    {
        const int32 Vertex = static_cast<int32>(Delta.SourceIdx);
        Matches.Reset();
        if (Vertices.IsValidIndex(Vertex))
        {
            Grid.FindAll(Vertices[Vertex].Position, Tolerance, Matches);
        }
        if (Matches.Num() == 0)
        {
            OutCode = TEXT("INVALID_ARGUMENT");
            OutError = Vertices.IsValidIndex(Vertex)
                ? FString::Printf(TEXT("Vertex %d of LOD %d has no source vertex at its place: the mesh's render data no longer matches its source data (reimport it). Nothing was changed."), Vertex, LOD)
                : FString::Printf(TEXT("vertexIndex %d is not a vertex of LOD %d, which has %d (0 to %d). Nothing was changed."), Vertex, LOD, Vertices.Num(), Vertices.Num() - 1);
            return false;
        }
        for (const int32 Match : Matches)
        {
            Morph[Match] = Delta.PositionDelta;
        }
    }
    if (!Morph.ContainsByPredicate([](const FVector3f& Delta) { return Delta.SizeSquared() > 1e-8f; }))
    {
        OutCode = TEXT("INVALID_MORPH_DATA");
        OutError = TEXT("Every positionDelta is zero: a morph target that moves no vertex is not built. Nothing was changed.");
        return false;
    }
    Source.Morphs.Add(Name, MoveTemp(Morph));
    TArray<TSharedPtr<FJsonValue>> Dropped;
    if (!McpSkinSource::CheckRenderOnlyMorphs(Mesh, Source, bAllowDrop, Dropped, OutError))
    {
        OutCode = TEXT("RENDER_ONLY_MORPHS");
        return false;
    }
    if (!McpSkinSource::Write(Mesh, LOD, Source, false, {Name}, OutError))
    {
        OutCode = TEXT("WRITE_FAILED");
        return false;
    }
    if (!Mesh->FindMorphTarget(Name))
    {
        OutCode = TEXT("MORPH_NOT_BUILT");
        OutError = FString::Printf(TEXT("The deltas of '%s' were written to %s's source data but its rebuild made no morph target from them; check the mesh's morph target import settings."),
            *Name.ToString(), *Mesh->GetName());
        return false;
    }
    return true;
}

// A mesh without source data: the morph is made from render data (a new one is checked before it is registered, so a
// refusal changes nothing), then looked up again, as a rebuild while registering drops it.
bool RegisterRenderMorph(USkeletalMesh* Mesh, int32 LOD, FName Name, const TArray<FMorphTargetDelta>& Deltas, FString& OutCode, FString& OutError)
{
    UMorphTarget* Morph = Mesh->FindMorphTarget(Name);
    const bool bCreated = Morph == nullptr;
    UMorphTarget* Checked = NewObject<UMorphTarget>(GetTransientPackage());
    Checked->BaseSkelMesh = Mesh;
    Checked->PopulateDeltas(Deltas, LOD, GetMorphTargetLodSections(Mesh, LOD), false, false);
    const bool bValid = Checked->HasValidData();
    Checked->MarkAsGarbage();
    if (!bValid)
    {
        OutCode = TEXT("INVALID_MORPH_DATA");
        OutError = FString::Printf(TEXT("Morph target '%s' has no valid data after populating %d deltas; check that the vertex indices exist in LOD %d. Nothing was changed."),
            *Name.ToString(), Deltas.Num(), LOD);
        return false;
    }
    Mesh->Modify();
    if (bCreated)
    {
        Morph = NewObject<UMorphTarget>(Mesh, Name);
        Morph->BaseSkelMesh = Mesh;
    }
    Morph->PopulateDeltas(Deltas, LOD, GetMorphTargetLodSections(Mesh, LOD), false, false);
    Mesh->RegisterMorphTarget(Morph);
    Mesh->MarkPackageDirty();
    if (!Mesh->FindMorphTarget(Name))
    {
        OutCode = TEXT("MORPH_NOT_BUILT");
        OutError = FString::Printf(TEXT("%s rebuilt as '%s' was registered and dropped it; the mesh has no source data to keep a morph target in."),
            *Mesh->GetName(), *Name.ToString());
        return false;
    }
    return true;
}

// Loads the mesh and the deltas, then writes the morph; answers the request itself when anything is refused.
bool ApplyMorph(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                USkeletalMesh* Mesh, FName Name, const TArray<FMorphTargetDelta>& Deltas, int32 LOD)
{
    FString Code;
    FString Error;
    McpSkinSource::FSourceMesh Source;
    bool bApplied = false;
    if (McpSkinSource::Read(Mesh, LOD, true, Source, Code, Error))
    {
        bApplied = WriteSourceMorph(Mesh, LOD, Name, Deltas, Source, GetJsonBoolField(Payload, TEXT("dropRenderOnlyMorphs"), false), Code, Error);
    }
    else if (Code == TEXT("SOURCE_DATA_MISSING"))
    {
        bApplied = RegisterRenderMorph(Mesh, LOD, Name, Deltas, Code, Error);
    }
    if (!bApplied)
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, Code);
    }
    return bApplied;
}

// create_morph_target (bCreate) answers a name the mesh already has without changing it; set_morph_target_deltas
// replaces that morph's deltas, or makes it.
void EditMorph(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bCreate)
{
    const FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    const FString MorphTargetName = GetJsonStringField(Payload, TEXT("morphTargetName"));
    if (SkeletalMeshPath.IsEmpty() || MorphTargetName.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("skeletalMeshPath and morphTargetName are required"), TEXT("MISSING_PARAM"));
        return;
    }
    FString Error;
    USkeletalMesh* Mesh = LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return;
    }
    const FName Name(*MorphTargetName);
    const bool bExisted = Mesh->FindMorphTarget(Name) != nullptr;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("morphTargetName"), MorphTargetName);
    if (bCreate && bExisted)
    {
        Result->SetBoolField(TEXT("alreadyExists"), true);
        Result->SetNumberField(TEXT("deltasApplied"), 0);
        Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Morph target '%s' already exists"), *MorphTargetName), Result);
        return;
    }
    TArray<FMorphTargetDelta> Deltas;
    FString DeltaCode;
    if (!ParseMorphTargetDeltas(Payload, Deltas, Error, DeltaCode))
    {
        // A missing array keeps the historical EMPTY_MORPH_TARGET code for a create.
        Bridge->SendAutomationError(Socket, RequestId, Error, bCreate && DeltaCode == TEXT("MISSING_PARAM") ? FString(TEXT("EMPTY_MORPH_TARGET")) : DeltaCode);
        return;
    }
    const int32 LODIndex = GetJsonIntField(Payload, TEXT("lodIndex"), 0);
    if (!ApplyMorph(Bridge, RequestId, Payload, Socket, Mesh, Name, Deltas, LODIndex))
    {
        return;
    }
    Result->SetNumberField(TEXT("morphTargetCount"), Mesh->GetMorphTargets().Num());
    Result->SetNumberField(TEXT("deltaCount"), Deltas.Num());
    Result->SetNumberField(TEXT("deltasApplied"), Deltas.Num());
    Result->SetNumberField(TEXT("lodIndex"), LODIndex);
    Result->SetBoolField(TEXT("created"), !bExisted);
    SaveAndReport(Mesh, Payload, Result);
    Bridge->SendAutomationResponse(Socket, RequestId, true, bCreate
        ? FString::Printf(TEXT("Morph target '%s' created with %d deltas"), *MorphTargetName, Deltas.Num())
        : FString::Printf(TEXT("Set %d deltas on morph target '%s'"), Deltas.Num(), *MorphTargetName), Result);
}
} // namespace

bool UEngineWireSubsystem::HandleCreateMorphTarget(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    EditMorph(this, RequestId, Payload, RequestingSocket, true);
    return true;
}

bool UEngineWireSubsystem::HandleSetMorphTargetDeltas(
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    EditMorph(this, RequestId, Payload, RequestingSocket, false);
    return true;
}
