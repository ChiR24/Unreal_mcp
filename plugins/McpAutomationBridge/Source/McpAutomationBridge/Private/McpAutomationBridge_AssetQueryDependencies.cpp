#include "McpAutomationBridge_AssetQueryHandlersPrivate.h"

namespace McpAssetQueryHandlers
{
bool HandleGetDependencies(
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

    bool bIncludeSoftDependencies = false;
    Payload->TryGetBoolField(TEXT("includeSoftDependencies"), bIncludeSoftDependencies);

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    TArray<FName> Dependencies;
    const UE::AssetRegistry::EDependencyQuery Query = bIncludeSoftDependencies
        ? UE::AssetRegistry::EDependencyQuery::Soft
        : UE::AssetRegistry::EDependencyQuery::Hard;

    AssetRegistryModule.Get().GetDependencies(
        FName(*SanitizedAssetPath),
        Dependencies,
        UE::AssetRegistry::EDependencyCategory::Package,
        Query);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    TArray<TSharedPtr<FJsonValue>> DepArray;
    for (const FName& Dep : Dependencies)
    {
        DepArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
    }
    Result->SetArrayField(TEXT("dependencies"), DepArray);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Dependencies retrieved."), Result);
    return true;
}
}
