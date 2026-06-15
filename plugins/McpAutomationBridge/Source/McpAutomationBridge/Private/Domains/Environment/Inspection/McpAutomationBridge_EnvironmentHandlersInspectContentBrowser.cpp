#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserItemPath.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"

namespace
{
FString McpGetAssetObjectPath(const FAssetData& Asset)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const FString ObjectPath = Asset.ObjectPath.ToString();
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    return ObjectPath;
#else
    return Asset.GetObjectPathString();
#endif
}

FString McpGetAssetClassPath(const FAssetData& Asset)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    const FString ClassPath = Asset.AssetClass.ToString();
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    return ClassPath;
#else
    return Asset.AssetClassPath.ToString();
#endif
}

TSharedPtr<FJsonObject> McpDescribeSelectedAsset(const FAssetData& Asset)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetName"), Asset.AssetName.ToString());
    Result->SetStringField(TEXT("objectPath"), McpGetAssetObjectPath(Asset));
    Result->SetStringField(TEXT("packageName"), Asset.PackageName.ToString());
    Result->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());
    Result->SetStringField(TEXT("classPath"), McpGetAssetClassPath(Asset));
    return Result;
}
}

namespace McpEnvironmentHandlers {

bool HandleInspectContentBrowserAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp)
{
    if (!LowerSubAction.Equals(TEXT("get_content_browser_state")))
    {
        return false;
    }

    FContentBrowserModule* ContentBrowserModule =
        FModuleManager::LoadModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
    if (ContentBrowserModule == nullptr)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Content Browser module is unavailable"),
            TEXT("CONTENT_BROWSER_UNAVAILABLE"));
        return true;
    }

    IContentBrowserSingleton& ContentBrowser = ContentBrowserModule->Get();
    TArray<FAssetData> SelectedAssets;
    TArray<FString> SelectedFolders;
    ContentBrowser.GetSelectedAssets(SelectedAssets);
    ContentBrowser.GetSelectedFolders(SelectedFolders);

    TArray<TSharedPtr<FJsonValue>> FolderValues;
    FolderValues.Reserve(SelectedFolders.Num());
    for (const FString& Folder : SelectedFolders)
    {
        FolderValues.Add(MakeShared<FJsonValueString>(Folder));
    }

    TArray<TSharedPtr<FJsonValue>> AssetValues;
    AssetValues.Reserve(SelectedAssets.Num());
    for (const FAssetData& Asset : SelectedAssets)
    {
        AssetValues.Add(MakeShared<FJsonValueObject>(McpDescribeSelectedAsset(Asset)));
    }

    Resp->SetStringField(TEXT("action"), TEXT("inspect"));
    Resp->SetStringField(TEXT("subAction"), SubAction);
    Resp->SetStringField(TEXT("message"), TEXT("Content Browser state retrieved"));
    Resp->SetStringField(TEXT("currentPath"), ContentBrowser.GetCurrentPath().GetVirtualPathString());
    Resp->SetArrayField(TEXT("selectedFolders"), FolderValues);
    Resp->SetNumberField(TEXT("selectedFolderCount"), FolderValues.Num());
    Resp->SetArrayField(TEXT("selectedAssets"), AssetValues);
    Resp->SetNumberField(TEXT("selectedAssetCount"), AssetValues.Num());
    Resp->SetBoolField(TEXT("success"), true);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("Content Browser state retrieved"), Resp, FString());
    return true;
}

}
#endif
