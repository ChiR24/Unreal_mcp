#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h"

namespace McpAudioAuthoring
{
TSharedPtr<FJsonObject> HandleMetaSoundAssetActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
	if (SubAction != TEXT("create_metasound"))
	{
		return nullptr;
	}

#if MCP_HAS_METASOUND
	FString Name = GetJsonStringField(Params, TEXT("name"), TEXT(""));
	FString Path = GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/MetaSounds"));
	bool bSave = GetJsonBoolField(Params, TEXT("save"), true);
	// The assetPath every other edit takes names the new MetaSound as well.
	const FString AssetPath = NormalizeAudioPath(GetJsonStringField(Params, TEXT("assetPath"), TEXT("")), false);
	if (Name.IsEmpty() && !AssetPath.IsEmpty())
	{
		const FString AssetPackage = FPackageName::ObjectPathToPackageName(AssetPath);
		Name = FPackageName::GetShortName(AssetPackage);
		Path = FPackageName::GetLongPackagePath(AssetPackage);
	}

	FString PackagePath, PathError;
	if (!BuildAudioCreationPath(Path, Name, PackagePath, PathError))
	{
		return Name.IsEmpty()
			? McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("name (with path), or assetPath, is required"))
			: McpHandlerUtils::BuildErrorResponse(TEXT("INVALID_PATH"), PathError);
	}
	// NewObject over a loaded MetaSound rebuilt it in place, and a new package over one on disk replaced it on save.
	if (McpAssetExists(PackagePath))
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("ASSET_EXISTS"), FString::Printf(
			TEXT("A MetaSound already exists at %s: edit it with assetPath, or pick another name."), *PackagePath));
	}
	Name = FPackageName::GetShortName(PackagePath);

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("PACKAGE_ERROR"), TEXT("Failed to create package"));
	}

	UMetaSoundSource* MetaSound = NewObject<UMetaSoundSource>(Package, FName(*Name), RF_Public | RF_Standalone);
	if (!MetaSound)
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("CREATE_FAILED"), TEXT("Failed to create MetaSound asset"));
	}

#if MCP_HAS_METASOUND_FRONTEND
	TScriptInterface<IMetaSoundDocumentInterface> DocInterface(MetaSound);
	if (DocInterface)
	{
		FMetaSoundFrontendDocumentBuilder Builder(DocInterface);
		Builder.InitDocument();
	}
#endif

	MetaSound->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(MetaSound);
	// bSave was read and never used: the new MetaSound lived in memory only.
	if (bSave && !McpSafeAssetSave(MetaSound))
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("SAVE_FAILED"), FString::Printf(TEXT("MetaSound '%s' was created but could not be saved"), *Name));
	}

	FString FullPath = MetaSound->GetPathName();
	Response->SetStringField(TEXT("assetPath"), FullPath);
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("MetaSound '%s' created"), *Name));
	McpHandlerUtils::AddVerification(Response, MetaSound);
	return Response;
#else
	return McpHandlerUtils::BuildErrorResponse(TEXT("METASOUND_NOT_AVAILABLE"), TEXT("MetaSound support not available in this engine version"));
#endif
}
}
