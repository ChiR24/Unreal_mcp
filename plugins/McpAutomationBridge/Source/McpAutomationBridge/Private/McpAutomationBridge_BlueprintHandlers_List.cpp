#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "Templates/SharedPointer.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Blueprint.h"
#include "UObject/UObjectIterator.h"
#endif

// ============================================================================
// LIST BLUEPRINTS
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleListBlueprints(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    // Parse filters
    FString PathFilter;
    FString ClassFilter = TEXT("Blueprint"); // Default to Blueprint
    FString TagFilter;
    FString PathStartsWith;

    const TSharedPtr<FJsonObject>* FilterObj;
    if (Payload->TryGetObjectField(TEXT("filter"), FilterObj) && FilterObj)
    {
        (*FilterObj)->TryGetStringField(TEXT("path"), PathFilter);
        
        FString UserClass;
        if ((*FilterObj)->TryGetStringField(TEXT("class"), UserClass) && !UserClass.IsEmpty())
        {
            ClassFilter = UserClass;
        }
        
        (*FilterObj)->TryGetStringField(TEXT("tag"), TagFilter);
        (*FilterObj)->TryGetStringField(TEXT("pathStartsWith"), PathStartsWith);
    }

    bool bRecursive = true;
    Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

    // Parse pagination
    int32 Offset = 0;
    int32 Limit = 50;
    const TSharedPtr<FJsonObject>* PaginationObj;
    if (Payload->TryGetObjectField(TEXT("pagination"), PaginationObj) && PaginationObj)
    {
        (*PaginationObj)->TryGetNumberField(TEXT("offset"), Offset);
        (*PaginationObj)->TryGetNumberField(TEXT("limit"), Limit);
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    FARFilter Filter;
    Filter.bRecursivePaths = bRecursive;
    Filter.bRecursiveClasses = true;
    
    if (!PathFilter.IsEmpty())
    {
        Filter.PackagePaths.Add(FName(*PathFilter));
    }
    else if (!PathStartsWith.IsEmpty())
    {
        Filter.PackagePaths.Add(FName(*PathStartsWith));
    }

    // Class filter
    // Note: FTopLevelAssetPath and ClassPaths were introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (ClassFilter == TEXT("Blueprint"))
    {
        Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Blueprint")));
    }
    else if (!ClassFilter.IsEmpty())
    {
        FTopLevelAssetPath ClassPath(ClassFilter);
        if (ClassPath.IsValid())
        {
             Filter.ClassPaths.Add(ClassPath);
        }
        else
        {
            UClass* FoundClass = FindObject<UClass>(nullptr, *ClassFilter);
            if (!FoundClass) FoundClass = FindObject<UClass>(nullptr, *(TEXT("/Script/Engine.") + ClassFilter));
            
            if (FoundClass)
            {
                Filter.ClassPaths.Add(FoundClass->GetClassPathName());
            }
        }
    }
#else
    // UE 5.0 fallback - use ClassNames
    if (ClassFilter == TEXT("Blueprint"))
    {
        Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
    }
    else if (!ClassFilter.IsEmpty())
    {
        UClass* FoundClass = FindObject<UClass>(nullptr, *ClassFilter);
        if (!FoundClass) FoundClass = FindObject<UClass>(nullptr, *(TEXT("/Script/Engine.") + ClassFilter));
        
        if (FoundClass)
        {
            Filter.ClassNames.Add(FoundClass->GetFName());
        }
    }
#endif

    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    // Post-filter if needed (e.g. if ClassPath logic was skipped or ambiguous)
    // ...

    int32 TotalCount = AssetList.Num();
    
    if (Offset > 0)
    {
        if (Offset < AssetList.Num())
        {
            AssetList.RemoveAt(0, Offset);
        }
        else
        {
            AssetList.Empty();
        }
    }

    if (Limit >= 0 && AssetList.Num() > Limit)
    {
        AssetList.SetNum(Limit);
    }

    TArray<TSharedPtr<FJsonValue>> BlueprintsArray;
    for (const FAssetData& Asset : AssetList)
    {
        TSharedPtr<FJsonObject> BpObj = MakeShared<FJsonObject>();
        BpObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        BpObj->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
        BpObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
        // UE 5.0 fallback
        BpObj->SetStringField(TEXT("path"), Asset.ObjectPath.ToString());
        BpObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
        BpObj->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());
        
        FString ParentClass;
        if (Asset.GetTagValue(TEXT("ParentClass"), ParentClass))
        {
            BpObj->SetStringField(TEXT("parentClass"), ParentClass);
        }

        BlueprintsArray.Add(MakeShared<FJsonValueObject>(BpObj));
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetArrayField(TEXT("blueprints"), BlueprintsArray);
    Resp->SetNumberField(TEXT("totalCount"), TotalCount);
    Resp->SetNumberField(TEXT("count"), BlueprintsArray.Num());
    
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Blueprints listed"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("list_blueprints requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
