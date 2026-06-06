#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PerformanceHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

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

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#endif

namespace McpPerformanceHandlers
{
bool HandleActorMergeAction(const FPerformanceActionContext& Context)
{
#if !WITH_EDITOR
    return false;
#else
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
    for (const TSharedPtr<FJsonValue>& Val : *NamesArray)
    {
        if (!Val.IsValid() || Val->Type != EJson::String)
        {
            continue;
        }

        const FString RawName = Val->AsString().TrimStartAndEnd();
        if (RawName.IsEmpty())
        {
            continue;
        }

        if (AActor* Resolved = ResolveMergeActorByName(World, RawName))
        {
            ActorsToMerge.AddUnique(Resolved);
        }
    }

    if (ActorsToMerge.Num() < 2)
    {
        Context.Bridge.SendAutomationResponse(
            Context.RequestingSocket, Context.RequestId, false,
            TEXT("merge_actors resolved fewer than 2 valid actors"), nullptr,
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString RequestedPackageName;
    Context.Payload->TryGetStringField(TEXT("packageName"), RequestedPackageName);
    if (RequestedPackageName.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("outputPath"), RequestedPackageName);
    }
    if (RequestedPackageName.IsEmpty())
    {
        RequestedPackageName = FString::Printf(
            TEXT("/Game/MCPTest/MergedActors/SM_Merged_%s"),
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
    FAssetRegistryModule::AssetCreated(MergedMesh);

    bool bSaved = false;
    if (UPackage* MergedPackage = MergedMesh->GetOutermost())
    {
        MergedPackage->ClearFlags(RF_Transient);
        MergedPackage->SetDirtyFlag(true);
        bSaved = McpSafeAssetSave(MergedMesh);

        if (bSaved)
        {
            TArray<FString> PathsToScan;
            PathsToScan.Add(FPaths::GetPath(MergedPackage->GetName()));
            FAssetRegistryModule& AssetRegistryModule =
                FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
            AssetRegistryModule.Get().ScanPathsSynchronous(PathsToScan, false);
        }
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

    bool bReplaceSources = false;
    Context.Payload->TryGetBoolField(TEXT("replaceSourceActors"), bReplaceSources);
    if (bReplaceSources)
    {
        for (AActor* Actor : ActorsToMerge)
        {
            if (Actor)
            {
                Actor->Destroy();
            }
        }
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
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

    if (ActorsToMerge.Num() > 0 && ActorsToMerge[0])
    {
        McpHandlerUtils::AddVerification(Response, ActorsToMerge[0]);
    }

    Context.Bridge.SendAutomationResponse(
        Context.RequestingSocket, Context.RequestId, true,
        TEXT("Actors merged to static mesh"), Response, FString());
    return true;
#endif
}
}
