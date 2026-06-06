#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PerformanceHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#endif

namespace McpPerformanceHandlers
{
#if WITH_EDITOR
bool SaveMergedActorMesh(
    const FPerformanceActionContext& Context,
    const FString& RequestedPackageName,
    const FString& MergeBasePackageName,
    UStaticMesh* MergedMesh,
    const TArray<UObject*>& AssetsToSync,
    const TArray<AActor*>& ActorsToMerge,
    const TArray<UPrimitiveComponent*>& ComponentsToMerge)
{
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

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetNumberField(TEXT("mergedActorCount"), ActorsToMerge.Num());
    Resp->SetNumberField(TEXT("mergedComponentCount"), ComponentsToMerge.Num());
    Resp->SetBoolField(TEXT("replaceSourceActors"), bReplaceSources);
    Resp->SetStringField(TEXT("requestedPackageName"), RequestedPackageName);
    Resp->SetStringField(TEXT("mergeBasePackageName"), MergeBasePackageName);
    Resp->SetStringField(TEXT("packageName"), MergedMesh->GetOutermost()->GetName());
    Resp->SetStringField(TEXT("assetPath"), MergedMesh->GetPathName());
    Resp->SetBoolField(TEXT("saved"), bSaved);

    TArray<TSharedPtr<FJsonValue>> AssetPaths;
    for (UObject* Asset : AssetsToSync)
    {
        if (Asset)
        {
            AssetPaths.Add(MakeShared<FJsonValueString>(Asset->GetPathName()));
        }
    }
    Resp->SetArrayField(TEXT("assets"), AssetPaths);

    if (ActorsToMerge.Num() > 0 && ActorsToMerge[0])
    {
        McpHandlerUtils::AddVerification(Resp, ActorsToMerge[0]);
    }

    Context.Bridge.SendAutomationResponse(
        Context.RequestingSocket, Context.RequestId, true,
        TEXT("Actors merged to static mesh"), Resp, FString());
    return true;
}
#endif
}
