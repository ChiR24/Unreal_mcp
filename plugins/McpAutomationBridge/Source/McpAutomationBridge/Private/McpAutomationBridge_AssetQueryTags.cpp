#include "McpAutomationBridge_AssetQueryHandlersPrivate.h"

namespace McpAssetQueryHandlers
{
bool HandleFindByMetadataTag(
    UMcpAutomationBridgeSubsystem* Bridge,
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

    FString RawPath;
    Payload->TryGetStringField(TEXT("path"), RawPath);

    FString Path;
    if (!RawPath.IsEmpty())
    {
        Path = SanitizeProjectRelativePath(RawPath);
        if (Path.IsEmpty())
        {
            Bridge->SendAutomationError(Socket, RequestId,
                FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *RawPath),
                TEXT("INVALID_PATH"));
            return true;
        }
    }
    else
    {
        Path = TEXT("/Game");
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*Path));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetDataList;
    AssetRegistry.GetAssets(Filter, AssetDataList);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    const FName TagFName(*Tag);

    for (const FAssetData& Data : AssetDataList)
    {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
        const FString AssetPath = Data.GetSoftObjectPath().ToString();
#else
        const FString AssetPath = Data.ToSoftObjectPath().ToString();
#endif

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
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            AssetObj->SetStringField(TEXT("classPath"), Data.AssetClassPath.ToString());
#else
            AssetObj->SetStringField(TEXT("classPath"), Data.AssetClass.ToString());
#endif
            AssetObj->SetStringField(TEXT("tagValue"), MetadataValue);
            AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
        }
    }

    Result->SetArrayField(TEXT("assets"), AssetsArray);
    Result->SetNumberField(TEXT("count"), AssetsArray.Num());

    Bridge->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Assets found by tag"), Result);
    return true;
}
}
