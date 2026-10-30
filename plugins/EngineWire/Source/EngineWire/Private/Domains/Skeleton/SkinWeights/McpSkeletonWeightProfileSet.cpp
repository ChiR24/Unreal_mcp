// set_vertex_weights: weights of a skin weight profile, written into the mesh's source data from UE 5.4. The build makes
// the LOD model's profiles from the source data (it resets the LOD model's own first), so a profile written into the
// LOD model alone, as this was, was wiped by the rebuild that followed while the reply said the weights were set.
#include "Domains/Skeleton/McpSkeletonActions.h"
#include "Domains/Skeleton/Assets/McpSkeletonAssetLoading.h"
#include "Domains/Skeleton/Assets/McpSkeletonPayload.h"
#include "Domains/Skeleton/SkinWeights/McpSkeletonSkinWeightSource.h"

#include "Engine/SkeletalMesh.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#if __has_include("Animation/SkinWeightProfile.h")
#include "Animation/SkinWeightProfile.h"
#endif

namespace McpSkeletonHandlers
{
namespace
{
struct FProfileVertex
{
    int32 Vertex = 0;
    TArray<McpSkinSource::FInfluence> Influences; // by bone name, for the source data
    TArray<TPair<int32, float>> Raw;              // by bone index, for the LOD model before UE 5.4
};

// weights: every entry a vertex of the LOD with influences on the mesh's own bones, weights 0 to 1 and not all 0.
// Nothing changes when one is wrong (bad entries used to be skipped and any bone index written).
bool ParseProfileWeights(const TSharedPtr<FJsonObject>& Payload, const USkeletalMesh* Mesh, int32 NumVertices, TArray<FProfileVertex>& Out, FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Payload->TryGetArrayField(TEXT("weights"), Entries) || !Entries || Entries->Num() == 0)
    {
        OutError = TEXT("weights is required: [{vertexIndex, influences: [{boneIndex, weight}]}].");
        return false;
    }
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    for (int32 Index = 0; Index < Entries->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Influences = nullptr;
        double Vertex = -1.0;
        if (!(*Entries)[Index]->TryGetObject(Entry) || !(*Entry)->TryGetNumberField(TEXT("vertexIndex"), Vertex) ||
            !(*Entry)->TryGetArrayField(TEXT("influences"), Influences) || Influences->Num() == 0)
        {
            OutError = FString::Printf(TEXT("weights[%d] needs vertexIndex and influences [{boneIndex, weight}]. Nothing was changed."), Index);
            return false;
        }
        if (Vertex < 0.0 || Vertex >= NumVertices)
        {
            OutError = FString::Printf(TEXT("weights[%d].vertexIndex %d is not a vertex of the LOD, which has %d (0 to %d). Nothing was changed."),
                Index, static_cast<int32>(Vertex), NumVertices, NumVertices - 1);
            return false;
        }
        FProfileVertex& Weights = Out.AddDefaulted_GetRef();
        Weights.Vertex = static_cast<int32>(Vertex);
        double Total = 0.0;
        for (const TSharedPtr<FJsonValue>& Value : *Influences)
        {
            const TSharedPtr<FJsonObject>* Influence = nullptr;
            double Bone = -1.0;
            double Weight = -1.0;
            if (!Value->TryGetObject(Influence) || !(*Influence)->TryGetNumberField(TEXT("boneIndex"), Bone) ||
                !(*Influence)->TryGetNumberField(TEXT("weight"), Weight) || Bone < 0.0 || Bone >= Ref.GetRawBoneNum() || Weight < 0.0 || Weight > 1.0)
            {
                OutError = FString::Printf(TEXT("weights[%d] has an influence that is not {boneIndex: 0 to %d (the mesh's bones), weight: 0 to 1}. Nothing was changed."),
                    Index, Ref.GetRawBoneNum() - 1);
                return false;
            }
            Weights.Influences.Add({Ref.GetBoneName(static_cast<int32>(Bone)), static_cast<float>(Weight)});
            Weights.Raw.Add({static_cast<int32>(Bone), static_cast<float>(Weight)});
            Total += Weight;
        }
        if (Total <= 0.0)
        {
            OutError = FString::Printf(TEXT("weights[%d]: every weight is 0, which leaves the vertex on no bone. Nothing was changed."), Index);
            return false;
        }
    }
    return true;
}

#if ENGINE_MINOR_VERSION >= 4
// Every source vertex at a given render vertex's place takes its weights; every other vertex keeps the profile's (a new
// profile starts from the default weights). The source data's build normalizes them.
bool WriteProfile(USkeletalMesh* Mesh, int32 LOD, FName Profile, const TArray<FProfileVertex>& Weights, bool bAllowDrop,
                  const TSharedPtr<FJsonObject>& Result, FString& OutCode, FString& OutError)
{
    McpSkinSource::FSourceMesh Source;
    if (!McpSkinSource::Read(Mesh, LOD, true, Source, OutCode, OutError, Profile))
    {
        return false;
    }
    TArray<FSoftSkinVertex> Vertices;
    Mesh->GetImportedModel()->LODModels[LOD].GetVertices(Vertices);
    const float Diagonal = McpSkinSource::BoundsDiagonal(Source.Positions);
    const McpSkinSource::FNearest Grid(Source.Positions, Diagonal * 0.01f);
    const float Tolerance = FMath::Max(Diagonal * 1e-5f, 1e-4f);
    TSet<int32> Set;
    TArray<int32> Matches;
    for (const FProfileVertex& Vertex : Weights)
    {
        Matches.Reset();
        Grid.FindAll(Vertices[Vertex.Vertex].Position, Tolerance, Matches);
        if (Matches.Num() == 0)
        {
            OutCode = TEXT("INVALID_ARGUMENT");
            OutError = FString::Printf(TEXT("Vertex %d of LOD %d has no source vertex at its place: the mesh's render data no longer matches its source data (reimport it). Nothing was changed."), Vertex.Vertex, LOD);
            return false;
        }
        for (const int32 Match : Matches)
        {
            Source.Weights[Match] = Vertex.Influences;
            Set.Add(Match);
        }
    }
    TArray<TSharedPtr<FJsonValue>> Dropped;
    if (!McpSkinSource::CheckRenderOnlyMorphs(Mesh, Source, bAllowDrop, Dropped, OutError))
    {
        OutCode = TEXT("RENDER_ONLY_MORPHS");
        return false;
    }
    // The build makes only the profiles the mesh lists.
    if (!Mesh->GetSkinWeightProfiles().ContainsByPredicate([Profile](const FSkinWeightProfileInfo& Info) { return Info.Name == Profile; }))
    {
        FSkinWeightProfileInfo Info;
        Info.Name = Profile;
        Mesh->AddSkinWeightProfile(Info);
    }
    if (!McpSkinSource::Write(Mesh, LOD, Source, true, {}, OutError, Profile))
    {
        OutCode = TEXT("WRITE_FAILED");
        return false;
    }
    Result->SetNumberField(TEXT("sourceVerticesSet"), Set.Num());
    Result->SetArrayField(TEXT("morphTargetsDropped"), Dropped);
    return true;
}
#else
// Before UE 5.4 the profile goes into the LOD model, as it always did; the read-back after the build says whether it
// lasted.
bool WriteProfile(USkeletalMesh* Mesh, int32 LOD, FName Profile, const TArray<FProfileVertex>& Weights, bool,
                  const TSharedPtr<FJsonObject>&, FString&, FString&)
{
    if (!Mesh->GetSkinWeightProfiles().ContainsByPredicate([Profile](const FSkinWeightProfileInfo& Info) { return Info.Name == Profile; }))
    {
        FSkinWeightProfileInfo Info;
        Info.Name = Profile;
        Mesh->AddSkinWeightProfile(Info);
    }
    FSkeletalMeshLODModel& Model = Mesh->GetImportedModel()->LODModels[LOD];
    FImportedSkinWeightProfileData& Data = Model.SkinWeightProfiles.FindOrAdd(Profile);
    Data.SkinWeights.SetNum(Model.NumVertices);
    for (const FProfileVertex& Vertex : Weights)
    {
        FRawSkinWeight& Skin = Data.SkinWeights[Vertex.Vertex];
        FMemory::Memzero(&Skin, sizeof(FRawSkinWeight));
        for (int32 Index = 0; Index < Vertex.Raw.Num() && Index < MAX_TOTAL_INFLUENCES; ++Index)
        {
            Skin.InfluenceBones[Index] = static_cast<FBoneIndexType>(Vertex.Raw[Index].Key);
            Skin.InfluenceWeights[Index] = static_cast<uint16>(FMath::Clamp(Vertex.Raw[Index].Value, 0.0f, 1.0f) * 65535.0f);
        }
    }
    Mesh->Build();
    return true;
}
#endif
} // namespace

bool HandleSetVertexWeightsAction(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SkeletalMeshPath = GetJsonStringField(Payload, TEXT("skeletalMeshPath"));
    FString ProfileName = GetJsonStringField(Payload, TEXT("profileName"));
    if (ProfileName.IsEmpty())
    {
        ProfileName = TEXT("CustomWeights");
    }
    if (ProfileName.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
    {
        // The engine's name for the mesh's own weights, which no alternate profile may take.
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("'Default' names the mesh's own weights, not a skin weight profile; pick another profileName (copy_weights, mirror_weights and prune_weights edit the mesh's own weights)."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FString Error;
    USkeletalMesh* Mesh = SkeletalMeshPath.IsEmpty() ? nullptr : LoadSkeletalMeshFromPathSkel(SkeletalMeshPath, Error);
    if (!Mesh)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, SkeletalMeshPath.IsEmpty() ? FString(TEXT("skeletalMeshPath is required")) : Error,
            SkeletalMeshPath.IsEmpty() ? TEXT("MISSING_PARAM") : TEXT("MESH_NOT_FOUND"));
        return true;
    }
    const int32 LOD = GetJsonIntField(Payload, TEXT("lodIndex"), 0);
    FSkeletalMeshModel* Imported = Mesh->GetImportedModel();
    if (!Imported || !Imported->LODModels.IsValidIndex(LOD))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("lodIndex %d does not exist (the mesh has %d LODs)."),
            LOD, Imported ? Imported->LODModels.Num() : 0), TEXT("INVALID_LOD"));
        return true;
    }
    TArray<FProfileVertex> Weights;
    if (!ParseProfileWeights(Payload, Mesh, static_cast<int32>(Imported->LODModels[LOD].NumVertices), Weights, Error))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FName Profile(*ProfileName);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString Code;
    if (!WriteProfile(Mesh, LOD, Profile, Weights, GetJsonBoolField(Payload, TEXT("dropRenderOnlyMorphs"), false), Result, Code, Error))
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, Error, Code);
        return true;
    }
    // The build makes the LOD model's profiles; one it did not make means the weights did not last.
    const FImportedSkinWeightProfileData* Built = Imported->LODModels[LOD].SkinWeightProfiles.Find(Profile);
    if (!Built || Built->SkinWeights.Num() == 0)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, FString::Printf(
            TEXT("The rebuild of %s made no skin weight profile '%s' from the weights written; nothing lasted."), *Mesh->GetName(), *ProfileName), TEXT("PROFILE_NOT_BUILT"));
        return true;
    }
    Result->SetStringField(TEXT("skeletalMeshPath"), SkeletalMeshPath);
    Result->SetStringField(TEXT("profileName"), ProfileName);
    Result->SetNumberField(TEXT("verticesModified"), Weights.Num());
    Result->SetNumberField(TEXT("lodIndex"), LOD);
    SaveAndReport(Mesh, Payload, Result);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Set weights for %d vertices in profile '%s'"), Weights.Num(), *ProfileName), Result);
    return true;
}
} // namespace McpSkeletonHandlers
