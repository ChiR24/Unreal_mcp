#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"

#if WITH_EDITOR
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlOperations.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleAssetQueryAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("asset_query"), ESearchCase::IgnoreCase)) return false;

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction = Payload->GetStringField(TEXT("subAction"));

    if (SubAction == TEXT("get_dependencies"))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
        bool bRecursive = false;
        Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FName> Dependencies;
        UE::AssetRegistry::EDependencyQuery Query = bRecursive ? UE::AssetRegistry::EDependencyQuery::Hard : UE::AssetRegistry::EDependencyQuery::Hard; // Simplified
        
        AssetRegistryModule.Get().GetDependencies(FName(*AssetPath), Dependencies, UE::AssetRegistry::EDependencyCategory::Package, Query);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> DepArray;
        for (const FName& Dep : Dependencies)
        {
            DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
        }
        Result->SetArrayField(TEXT("dependencies"), DepArray);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Dependencies retrieved."), Result);
        return true;
    }
    else if (SubAction == TEXT("search_assets"))
    {
        FARFilter Filter;
        
        // Parse Class Names
        const TArray<TSharedPtr<FJsonValue>>* ClassNamesPtr;
        if (Payload->TryGetArrayField(TEXT("classNames"), ClassNamesPtr) && ClassNamesPtr)
        {
            for (const TSharedPtr<FJsonValue>& Val : *ClassNamesPtr)
            {
                const FString ClassName = Val->AsString();
                if (!ClassName.IsEmpty())
                {
                    // Support both full paths and short names
                    if (ClassName.Contains(TEXT("/")))
                    {
                        Filter.ClassPaths.Add(FTopLevelAssetPath(ClassName));
                    }
                    else
                    {
                        // Fallback for short names is tricky in UE5.1+, but we can try to guess or rely on ClassNames for legacy
                        // For now, we assume full paths for ClassPaths. 
                        // If just "StaticMesh" is passed, it might fail with strict ClassPaths.
                        // But let's add it to the filter and let AR handle it (some versions support short names in ClassNames deprecated)
                        PRAGMA_DISABLE_DEPRECATION_WARNINGS
                        Filter.ClassNames.Add(FName(*ClassName));
                        PRAGMA_ENABLE_DEPRECATION_WARNINGS
                    }
                }
            }
        }

        // Parse Package Paths
        const TArray<TSharedPtr<FJsonValue>>* PackagePathsPtr;
        if (Payload->TryGetArrayField(TEXT("packagePaths"), PackagePathsPtr) && PackagePathsPtr)
        {
            for (const TSharedPtr<FJsonValue>& Val : *PackagePathsPtr)
            {
                Filter.PackagePaths.Add(FName(*Val->AsString()));
            }
        }

        // Parse Recursion
        bool bRecursivePaths = true;
        if (Payload->HasField(TEXT("recursivePaths"))) Payload->TryGetBoolField(TEXT("recursivePaths"), bRecursivePaths);
        Filter.bRecursivePaths = bRecursivePaths;

        bool bRecursiveClasses = false;
        if (Payload->HasField(TEXT("recursiveClasses"))) Payload->TryGetBoolField(TEXT("recursiveClasses"), bRecursiveClasses);
        Filter.bRecursiveClasses = bRecursiveClasses;

        // Execute Query
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        TArray<FAssetData> AssetDataList;
        AssetRegistryModule.Get().GetAssets(Filter, AssetDataList);

        // Apply Limit
        int32 Limit = 100;
        if (Payload->HasField(TEXT("limit"))) Payload->TryGetNumberField(TEXT("limit"), Limit);
        if (Limit > 0 && AssetDataList.Num() > Limit)
        {
            AssetDataList.SetNum(Limit);
        }

        // Build Response
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> AssetsArray;

        for (const FAssetData& Data : AssetDataList)
        {
            TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
            AssetObj->SetStringField(TEXT("assetName"), Data.AssetName.ToString());
            AssetObj->SetStringField(TEXT("assetPath"), Data.GetSoftObjectPath().ToString());
            AssetObj->SetStringField(TEXT("classPath"), Data.AssetClassPath.ToString());
            AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
        }

        Result->SetArrayField(TEXT("assets"), AssetsArray);
        Result->SetNumberField(TEXT("count"), AssetsArray.Num());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Assets found."), Result);
        return true;
    }
#if WITH_EDITOR
    else if (SubAction == TEXT("get_source_control_state"))
    {
        FString AssetPath;
        Payload->TryGetStringField(TEXT("assetPath"), AssetPath);

        if (ISourceControlModule::Get().IsEnabled())
        {
            ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
            FSourceControlStatePtr State = Provider.GetState(AssetPath, EStateCacheUsage::Use);
            
            if (State.IsValid())
            {
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("isCheckedOut"), State->IsCheckedOut());
                Result->SetBoolField(TEXT("isAdded"), State->IsAdded());
                Result->SetBoolField(TEXT("isDeleted"), State->IsDeleted());
                Result->SetBoolField(TEXT("isModified"), State->IsModified());
                // Result->SetStringField(TEXT("whoCheckedOut"), State->GetCheckOutUser());
                
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Source control state retrieved."), Result);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Could not get source control state."), TEXT("STATE_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Source control not enabled."), TEXT("SC_DISABLED"));
        }
        return true;
    }
#endif

    SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}
