#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"


bool HandleStructSerializationActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("list_structs"))
    {
        FString PathFilter = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/Structs"));

        // Enumerate UserDefinedStruct assets via the Asset Registry without
        // loading each asset. The FARFilter scopes the query to the requested
        // package hierarchy (bRecursivePaths) so sibling paths like
        // /Game/StructsExtra are excluded, and only the intended subtree matches.
        FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        IAssetRegistry& AR = ARM.GetRegistry();

        FARFilter Filter;
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
        Filter.ClassPaths.Add(UUserDefinedStruct::StaticClass()->GetClassPathName());
#else
        Filter.ClassNames.Add(UUserDefinedStruct::StaticClass()->GetFName());
#endif
        Filter.bRecursiveClasses = true;
        if (!PathFilter.IsEmpty())
        {
            Filter.PackagePaths.Add(FName(*PathFilter));
            Filter.bRecursivePaths = true;
        }

        TArray<FAssetData> StructAssets;
        AR.GetAssets(Filter, StructAssets);

        TArray<TSharedPtr<FJsonValue>> Arr;
        for (const FAssetData& AssetData : StructAssets)
        {
            // Use the asset registry's path/name directly — no LoadObject needed,
            // so unloaded structs are still discovered without materializing them.
            TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
            O->SetStringField(TEXT("assetPath"), MCP_ASSET_DATA_GET_OBJECT_PATH(AssetData));
            O->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
            Arr.Add(MakeShared<FJsonValueObject>(O));
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("path"), PathFilter);
        Result->SetArrayField(TEXT("structs"), Arr);
        Result->SetNumberField(TEXT("count"), Arr.Num());
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Structs enumerated"), Result);
        return true;
    }

    return false;
}

