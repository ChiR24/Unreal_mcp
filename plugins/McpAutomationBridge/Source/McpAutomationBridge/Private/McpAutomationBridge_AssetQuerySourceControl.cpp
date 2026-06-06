#include "McpAutomationBridge_AssetQueryHandlersPrivate.h"

#if WITH_EDITOR
namespace McpAssetQueryHandlers
{
bool HandleGetSourceControlState(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString AssetPath;
    Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
    if (AssetPath.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("Missing assetPath."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedAssetPath.IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Invalid assetPath: '%s' contains traversal or invalid characters."), *AssetPath),
            TEXT("INVALID_PATH"));
        return true;
    }

    if (!ISourceControlModule::Get().IsEnabled())
    {
        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("Source control not enabled."), TEXT("SC_DISABLED"));
        return true;
    }

    ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
    const FString PackageName = FPackageName::ObjectPathToPackageName(SanitizedAssetPath);
    FString FilePath;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension()) &&
        !FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetMapPackageExtension()))
    {
        Bridge->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("Could not convert assetPath to source-control filename: %s"), *SanitizedAssetPath),
            TEXT("INVALID_PATH"));
        return true;
    }

    FSourceControlStatePtr State = Provider.GetState(FilePath, EStateCacheUsage::Use);
    if (!State.IsValid())
    {
        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("Could not get source control state."), TEXT("STATE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), SanitizedAssetPath);
    Result->SetStringField(TEXT("filePath"), FilePath);
    Result->SetBoolField(TEXT("isCheckedOut"), State->IsCheckedOut());
    Result->SetBoolField(TEXT("isAdded"), State->IsAdded());
    Result->SetBoolField(TEXT("isDeleted"), State->IsDeleted());
    Result->SetBoolField(TEXT("isModified"), State->IsModified());

    Bridge->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Source control state retrieved."), Result);
    return true;
}
}
#endif
