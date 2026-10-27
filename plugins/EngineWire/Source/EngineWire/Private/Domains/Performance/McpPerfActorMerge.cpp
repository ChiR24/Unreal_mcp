#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Performance/McpPerfPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Subsystems/EditorActorSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "HAL/FileManager.h"
#include "IMeshMergeUtilities.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
#include "MeshMerge/MeshMergingSettings.h"
#else
#include "Engine/MeshMerging.h"
#endif
#include "MeshMergeModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "StaticMeshCompiler.h"

#include "Editor/UnrealEd/Public/Editor.h"

namespace McpPerformanceHandlers
{
bool HandleActorMergeAction(const FPerformanceActionContext& Context)
{
    if (Context.Lower != TEXT("merge_actors"))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
    if (!Context.Payload->TryGetArrayField(TEXT("actors"), NamesArray) ||
        !NamesArray ||
        NamesArray->Num() < 2)
    {
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, false,
            TEXT("merge_actors requires an 'actors' array with at least 2 entries"),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, false,
            TEXT("Editor world not available for merge_actors"), nullptr,
            TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    TArray<AActor*> ActorsToMerge;
    TArray<FString> Unresolved;
    for (const TSharedPtr<FJsonValue>& Val : *NamesArray)
    {
        const FString RawName = Val.IsValid() && Val->Type == EJson::String ? Val->AsString().TrimStartAndEnd() : FString();
        AActor* Resolved = RawName.IsEmpty() ? nullptr : ResolveMergeActorByName(World, RawName);
        if (Resolved)
        {
            ActorsToMerge.AddUnique(Resolved);
        }
        else
        {
            Unresolved.Add(RawName.IsEmpty() ? FString(TEXT("(empty)")) : RawName);
        }
    }
    // A name that matched no actor was skipped, and the merge went ahead without it.
    if (Unresolved.Num() > 0)
    {
        Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false,
            FString::Printf(TEXT("No actor named %s in the level; nothing was merged."), *FString::Join(Unresolved, TEXT(", "))),
            nullptr, TEXT("ACTOR_NOT_FOUND"));
        return true;
    }

    if (ActorsToMerge.Num() < 2)
    {
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, false,
            TEXT("merge_actors resolved fewer than 2 valid actors"), nullptr,
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // packageName is the asset (a /Game path, or a name placed in the outputPath folder, default /Game/MergedActors);
    // outputPath alone is the asset path. A bare name was refused, the record's own example among them.
    FString RequestedPackageName;
    FString OutputPath;
    Context.Payload->TryGetStringField(TEXT("packageName"), RequestedPackageName);
    Context.Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    if (RequestedPackageName.IsEmpty())
    {
        Swap(RequestedPackageName, OutputPath);
    }
    if (!RequestedPackageName.IsEmpty() && !RequestedPackageName.Contains(TEXT("/")))
    {
        RequestedPackageName = (OutputPath.IsEmpty() ? FString(TEXT("/Game/MergedActors")) : OutputPath) / RequestedPackageName;
    }
    if (RequestedPackageName.IsEmpty())
    {
        RequestedPackageName = FString::Printf(
            TEXT("/Game/MergedActors/SM_Merged_%s"),
            *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    }
    if (!FPackageName::IsValidLongPackageName(RequestedPackageName))
    {
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, false,
            TEXT("merge_actors requires packageName/outputPath to be a valid long package name"),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // MergeComponentsToStaticMesh prefixes the asset with SM_ itself, so a requested SM_X is merged as X.
    FString MergeBasePackageName = RequestedPackageName;
    const FString RequestedAssetName = FPackageName::GetShortName(RequestedPackageName);
    if (RequestedAssetName.StartsWith(TEXT("SM_")))
    {
        const FString BaseAssetName = RequestedAssetName.RightChop(3);
        if (!BaseAssetName.IsEmpty())
        {
            MergeBasePackageName =
                FPackageName::GetLongPackagePath(RequestedPackageName) / BaseAssetName;
        }
    }
    if (!FPackageName::IsValidLongPackageName(MergeBasePackageName))
    {
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, false,
            TEXT("merge_actors normalized packageName/outputPath to an invalid merge base package name"),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // The merge names its mesh SM_<name> and builds it over a static mesh already there.
    const FString FinalPackageName = FPackageName::GetLongPackagePath(MergeBasePackageName) / (TEXT("SM_") + FPackageName::GetShortName(MergeBasePackageName));
    if (FPackageName::DoesPackageExist(FinalPackageName) || FindObject<UPackage>(nullptr, *FinalPackageName))
    {
        Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false,
            FString::Printf(TEXT("%s already exists; pick another packageName. Nothing was merged."), *FinalPackageName), nullptr, TEXT("ASSET_ALREADY_EXISTS"));
        return true;
    }

    TArray<UPrimitiveComponent*> ComponentsToMerge;
    CollectMergeComponents(ActorsToMerge, ComponentsToMerge);
    if (ComponentsToMerge.Num() < 2)
    {
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, false,
            TEXT("merge_actors requires at least 2 static mesh components"),
            nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    IMeshMergeModule& MeshMergeModule =
        FModuleManager::LoadModuleChecked<IMeshMergeModule>(TEXT("MeshMergeUtilities"));
    const IMeshMergeUtilities& MeshMergeUtilities = MeshMergeModule.GetUtilities();

    FMeshMergingSettings MergeSettings;
    MergeSettings.bMergeMaterials = false;
    MergeSettings.bGenerateLightMapUV = true;
    MergeSettings.bBakeVertexDataToMesh = true;
    MergeSettings.bMergePhysicsData = true;
    MergeSettings.LODSelectionType = EMeshLODSelectionType::AllLODs;
    MergeSettings.TargetLightMapResolution = 64;

    TArray<UObject*> AssetsToSync;
    FVector MergedActorLocation = FVector::ZeroVector;
    const float ScreenAreaSize = TNumericLimits<float>::Max();
    MeshMergeUtilities.MergeComponentsToStaticMesh(
        ComponentsToMerge, World, MergeSettings, nullptr, nullptr,
        MergeBasePackageName, AssetsToSync, MergedActorLocation, ScreenAreaSize,
        true);

    UStaticMesh* MergedMesh = nullptr;
    for (UObject* Asset : AssetsToSync)
    {
        if (!Asset)
        {
            continue;
        }
        if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
        {
            MergedMesh = StaticMesh;
        }
        FAssetRegistryModule::AssetCreated(Asset);
        Asset->MarkPackageDirty();
    }

    if (!MergedMesh)
    {
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, false,
            TEXT("Actor merge produced no static mesh asset"), nullptr,
            TEXT("MERGE_FAILED"));
        return true;
    }

    TArray<UStaticMesh*> MeshesToFinish;
    MeshesToFinish.Add(MergedMesh);
    FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
    FlushRenderingCommands();

    MergedMesh->SetFlags(RF_Public | RF_Standalone);
    MergedMesh->ClearFlags(RF_Transient);
    MergedMesh->MarkPackageDirty();

    bool bSaved = false;
    if (UPackage* MergedPackage = MergedMesh->GetOutermost())
    {
        MergedPackage->ClearFlags(RF_Transient);
        MergedPackage->SetDirtyFlag(true);
        bSaved = McpSafeAssetSave(MergedMesh);
    }

    if (!bSaved)
    {
        TSharedPtr<FJsonObject> Failure = McpHandlerUtils::CreateResultObject();
        Failure->SetStringField(TEXT("requestedPackageName"), RequestedPackageName);
        Failure->SetStringField(TEXT("mergeBasePackageName"), MergeBasePackageName);
        Failure->SetStringField(TEXT("actualPackageName"), MergedMesh->GetOutermost()->GetName());
        Failure->SetStringField(TEXT("assetPath"), MergedMesh->GetPathName());
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, false,
            TEXT("Merged static mesh was created but could not be saved"),
            Failure, TEXT("SAVE_FAILED"));
        return true;
    }

    // Replacing put nothing in the sources' place: it destroyed them and the geometry was gone from the level. One undo
    // step now places the merged mesh where the merge put it and removes the sources.
    bool bReplaceSources = false;
    Context.Payload->TryGetBoolField(TEXT("replaceSourceActors"), bReplaceSources);
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    AActor* ResultActor = ActorsToMerge[0];
    if (bReplaceSources)
    {
        FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Merge Actors")), EMcpMutationDurability::EditorStateOnly, TArray<UObject*>(ActorsToMerge));
        FActorSpawnParameters SpawnParams;
        SpawnParams.ObjectFlags |= RF_Transactional;
        AStaticMeshActor* Merged = World->SpawnActor<AStaticMeshActor>(MergedActorLocation, FRotator::ZeroRotator, SpawnParams);
        if (!Merged)
        {
            Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false,
                TEXT("The merged mesh was saved but its actor could not be placed; the source actors were kept."), nullptr, TEXT("SPAWN_FAILED"));
            return true;
        }
        Merged->GetStaticMeshComponent()->SetStaticMesh(MergedMesh);
        Merged->SetActorLabel(ActorsToMerge[0]->GetActorLabel() + TEXT("_Merged"));
        UEditorActorSubsystem* ActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
        for (AActor* Actor : ActorsToMerge)
        {
            ActorSubsystem->DestroyActor(Actor);
        }
        ResultActor = Merged;
        Transaction.DescribeInto(Response);
    }
    Response->SetNumberField(TEXT("mergedActorCount"), ActorsToMerge.Num());
    Response->SetNumberField(TEXT("mergedComponentCount"), ComponentsToMerge.Num());
    Response->SetBoolField(TEXT("replaceSourceActors"), bReplaceSources);
    Response->SetStringField(TEXT("requestedPackageName"), RequestedPackageName);
    Response->SetStringField(TEXT("mergeBasePackageName"), MergeBasePackageName);
    Response->SetStringField(TEXT("packageName"), MergedMesh->GetOutermost()->GetName());
    Response->SetStringField(TEXT("assetPath"), MergedMesh->GetPathName());
    Response->SetBoolField(TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> AssetPaths;
    for (UObject* Asset : AssetsToSync)
    {
        if (Asset)
        {
            AssetPaths.Add(MakeShared<FJsonValueString>(Asset->GetPathName()));
        }
    }
    Response->SetArrayField(TEXT("assets"), AssetPaths);

    // The actor the level now holds: the merged one when it replaced the sources (it named a destroyed source).
    McpHandlerUtils::AddVerification(Response, ResultActor);

    Context.Bridge.SendAutomationResponse(
        Context.RequestingSocket, Context.RequestId, true,
        TEXT("Actors merged to static mesh"), Response, FString());
    return true;
}
}
