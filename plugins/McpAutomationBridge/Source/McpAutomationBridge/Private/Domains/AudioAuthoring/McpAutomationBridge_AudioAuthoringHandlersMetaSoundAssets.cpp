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
	FString Path = NormalizeAudioPath(GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Audio/MetaSounds")), false);
	bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

	if (Name.IsEmpty())
	{
		return McpHandlerUtils::BuildErrorResponse(TEXT("MISSING_NAME"), TEXT("Name is required"));
	}

	FString PackagePath = Path / Name;
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
