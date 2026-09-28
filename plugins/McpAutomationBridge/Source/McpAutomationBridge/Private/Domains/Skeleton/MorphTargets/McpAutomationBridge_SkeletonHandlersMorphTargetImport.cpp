#include "Domains/Skeleton/McpAutomationBridge_SkeletonHandlersActions.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersAssetLoading.h"
#include "Domains/Skeleton/Assets/McpAutomationBridge_SkeletonHandlersPayload.h"
#include "Domains/Skeleton/SkinWeights/McpAutomationBridge_SkeletonHandlersSkinWeightSource.h"

#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"

// import_morph_targets: move blend shapes from one mesh's source data into another's, each target vertex
// taking the delta of its nearest source vertex, so the morphs survive every later build.
namespace McpSkeletonHandlers
{
bool HandleImportMorphTargetsAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const int32 LOD = GetJsonIntField(Payload, TEXT("lodIndex"), 0);
    FString Error;
    FString Code;
    USkeletalMesh* TargetMesh = LoadSkeletalMeshFromPathSkel(GetJsonStringField(Payload, TEXT("skeletalMeshPath")), Error);
    USkeletalMesh* SourceMesh = TargetMesh ? LoadSkeletalMeshFromPathSkel(GetJsonStringField(Payload, TEXT("sourceMeshPath")), Error) : nullptr;
    McpSkinSource::FSourceMesh Source;
    McpSkinSource::FSourceMesh Target;
    if (!TargetMesh || !SourceMesh)
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, TEXT("MESH_NOT_FOUND"));
        return true;
    }
    if (!McpSkinSource::Read(SourceMesh, 0, true, Source, Code, Error) || !McpSkinSource::Read(TargetMesh, LOD, true, Target, Code, Error))
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, Code);
        return true;
    }
    TArray<FName> Wanted;
    const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
    if (Payload->TryGetArrayField(TEXT("morphTargets"), Names) && Names->Num() > 0)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Names)
        {
            Wanted.AddUnique(FName(*Value->AsString()));
        }
    }
    else
    {
        Source.Morphs.GetKeys(Wanted);
    }
    TArray<FString> Missing;
    for (const FName& Name : Wanted)
    {
        if (!Source.Morphs.Contains(Name)) Missing.Add(Name.ToString());
    }
    if (Wanted.Num() == 0 || Missing.Num() > 0)
    {
        TArray<FString> RenderOnly;
        for (const UMorphTarget* Morph : SourceMesh->GetMorphTargets())
        {
            if (Morph && !Source.Morphs.Contains(Morph->GetFName())) RenderOnly.Add(Morph->GetName());
        }
        TArray<FName> Available;
        Source.Morphs.GetKeys(Available);
        TArray<FString> AvailableText;
        for (const FName& Name : Available) AvailableText.Add(Name.ToString());
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(
            TEXT("%s has no source-data morph target %s. Source-data morphs: %s. Render-only morphs (made by set_morph_target_deltas, which a rebuild drops, so they cannot be transferred): %s."),
            *SourceMesh->GetName(), Missing.Num() > 0 ? *FString::Join(Missing, TEXT(", ")) : TEXT("at all"),
            AvailableText.Num() > 0 ? *FString::Join(AvailableText, TEXT(", ")) : TEXT("none"),
            RenderOnly.Num() > 0 ? *FString::Join(RenderOnly, TEXT(", ")) : TEXT("none")), TEXT("NO_MORPH_TARGETS"));
        return true;
    }
    const float Diagonal = McpSkinSource::BoundsDiagonal(Source.Positions);
    const McpSkinSource::FNearest Nearest(Source.Positions, Diagonal * 0.01f);
    TArray<int32> Match;
    int32 Unmatched = 0;
    for (const FVector3f& Position : Target.Positions)
    {
        float Distance = 0.0f;
        Match.Add(Nearest.Find(Position, Distance));
        Unmatched += Distance > Diagonal * 0.02f ? 1 : 0;
    }
    TArray<TSharedPtr<FJsonValue>> Imported;
    TArray<TSharedPtr<FJsonValue>> Replaced;
    for (const FName& Name : Wanted)
    {
        const TArray<FVector3f>& SourceDeltas = Source.Morphs[Name];
        TArray<FVector3f>& Deltas = Target.Morphs.FindOrAdd(Name);
        if (Deltas.Num() > 0) Replaced.Add(MakeShared<FJsonValueString>(Name.ToString()));
        Deltas.SetNumZeroed(Target.Positions.Num());
        int32 Affected = 0;
        for (int32 Vertex = 0; Vertex < Target.Positions.Num(); ++Vertex)
        {
            Deltas[Vertex] = SourceDeltas[Match[Vertex]];
            Affected += Deltas[Vertex].SizeSquared() > 1e-8f ? 1 : 0;
        }
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Name.ToString());
        Entry->SetNumberField(TEXT("verticesAffected"), Affected);
        Imported.Add(MakeShared<FJsonValueObject>(Entry));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("morphTargetsDropped"), McpSkinSource::RenderOnlyMorphs(TargetMesh, Target));
    if (!McpSkinSource::Write(TargetMesh, LOD, Target, false, Wanted, Error))
    {
        Subsystem->SendAutomationError(Socket, RequestId, Error, TEXT("WRITE_FAILED"));
        return true;
    }
    // The rebuild turns the source-data morphs into UMorphTarget objects; read them back.
    TArray<TSharedPtr<FJsonValue>> Built;
    for (const UMorphTarget* Morph : TargetMesh->GetMorphTargets())
    {
        if (Morph && Wanted.Contains(Morph->GetFName())) Built.Add(MakeShared<FJsonValueString>(Morph->GetName()));
    }
    Result->SetArrayField(TEXT("imported"), Imported);
    Result->SetArrayField(TEXT("replaced"), Replaced);
    Result->SetArrayField(TEXT("builtMorphTargets"), Built);
    Result->SetNumberField(TEXT("unmatchedVertices"), Unmatched);
    Result->SetStringField(TEXT("skeletalMeshPath"), TargetMesh->GetPathName());
    Result->SetBoolField(TEXT("saved"), GetJsonBoolField(Payload, TEXT("save"), true) && SaveIfRequested(TargetMesh, Payload));
    McpHandlerUtils::AddVerification(Result, TargetMesh);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, FString::Printf(
        TEXT("Imported %d morph targets from %s into %s (%d built)"), Wanted.Num(), *SourceMesh->GetName(), *TargetMesh->GetName(), Built.Num()), Result);
    return true;
}
}
