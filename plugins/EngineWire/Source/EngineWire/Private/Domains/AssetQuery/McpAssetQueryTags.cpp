#include "Domains/AssetQuery/McpAssetQueryPrivate.h"

namespace McpAssetQueryHandlers
{
bool HandleFindByMetadataTag(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString Tag;
    Payload->TryGetStringField(TEXT("tag"), Tag);

    FString ExpectedValue;
    Payload->TryGetStringField(TEXT("value"), ExpectedValue);

    if (Tag.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("tag required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    const FName TagFName(*Tag);

    for (const FAssetData& Data : AssetDataList)
    {
        const FString AssetPath = MCP_ASSET_DATA_GET_SOFT_PATH(Data);

        FString MetadataValue;
        const bool bHasTag = Data.GetTagValue(TagFName, MetadataValue);
        bool bMatches = bHasTag;
        if (bMatches && !ExpectedValue.IsEmpty())
        {
            bMatches = MetadataValue.Equals(ExpectedValue, ESearchCase::IgnoreCase);
        }

        if (bMatches)
        {
            TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
            AssetObj->SetStringField(TEXT("assetName"), Data.AssetName.ToString());
            AssetObj->SetStringField(TEXT("assetPath"), AssetPath);
            AssetObj->SetStringField(TEXT("classPath"), MCP_ASSET_DATA_GET_CLASS_PATH(Data));
            AssetObj->SetStringField(TEXT("tagValue"), MetadataValue);
            AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
        }
    }

    Result->SetArrayField(TEXT("assets"), AssetsArray);
    Result->SetNumberField(TEXT("count"), AssetsArray.Num());

    Bridge->SendAutomationResponse(Socket, RequestId, true,
        AssetsArray.Num() == 0 ? FString(TEXT("No assets carry that tag."))
                               : FString::Printf(TEXT("Found %d assets by tag."), AssetsArray.Num()),
        Result);
    return true;
}
}
