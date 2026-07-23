#include "MCP/Resources/McpResourceReadContent.h"
#include "MCP/Resources/McpResourceCatalog.h"
#include "MCP/Resources/McpResourceUri.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"

namespace
{
	const TCHAR* CatalogUri = TEXT("ue://capability/catalog");
	const TCHAR* ProjectUri = TEXT("ue://project");
	constexpr int32 MaxCatalogCapabilities = 50;

	FString SerializeCompact(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	TSharedRef<FJsonObject> BuildCatalogData()
	{
		const FMcpCapabilityStore& Store = FMcpCapabilityStore::Get();
		const TArray<FMcpCapabilityRecord>& Records = Store.GetRecords();
		TArray<TSharedPtr<FJsonValue>> Capabilities;
		for (const FMcpCapabilityRecord& Record : Records)
		{
			if (Capabilities.Num() >= MaxCatalogCapabilities)
			{
				break;
			}
			Capabilities.Add(MakeShared<FJsonValueString>(Record.Id));
		}
		auto Data = MakeShared<FJsonObject>();
		Data->SetArrayField(TEXT("capabilities"), Capabilities);
		Data->SetNumberField(TEXT("count"), Capabilities.Num());
		Data->SetNumberField(TEXT("totalCount"), Records.Num());
		Data->SetBoolField(TEXT("truncated"), Capabilities.Num() < Records.Num());
		return Data;
	}

	TSharedRef<FJsonObject> BuildProjectData()
	{
		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("projectName"), McpResourceUri::RedactProjectName(FApp::GetProjectName()));
		const FEngineVersion& Version = FEngineVersion::Current();
		Data->SetStringField(TEXT("engineVersion"),
			FString::Printf(TEXT("%d.%d"), Version.GetMajor(), Version.GetMinor()));
		Data->SetStringField(TEXT("contentRoot"), TEXT("/Game"));
		Data->SetBoolField(TEXT("connected"), true);
		return Data;
	}
}  // namespace

namespace McpResourceRead
{
	EReadKind Classify(const FString& Uri)
	{
		if (Uri == CatalogUri || Uri == ProjectUri)
		{
			return EReadKind::SocketReadable;
		}
		if (McpResourceCatalog::IsListedResourceUri(Uri) || McpResourceCatalog::MatchesKnownTemplate(Uri))
		{
			return EReadKind::EditorUnavailable;
		}
		return EReadKind::Unknown;
	}

	FString BuildReadBodyText(const FString& Uri, FMcpResourceRevision Revision)
	{
		auto Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("revision"), static_cast<double>(Revision));
		Root->SetObjectField(TEXT("data"), Uri == ProjectUri ? BuildProjectData() : BuildCatalogData());
		return SerializeCompact(Root);
	}

	TSharedPtr<FJsonValue> ListEntry(const FMcpResourceDefinition& Def)
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("uri"), Def.Uri);
		Obj->SetStringField(TEXT("name"), Def.Name);
		Obj->SetStringField(TEXT("description"), Def.Description);
		Obj->SetStringField(TEXT("mimeType"), Def.MimeType);
		return MakeShared<FJsonValueObject>(Obj);
	}

	TSharedPtr<FJsonValue> TemplateEntry(const FMcpResourceTemplateDefinition& Def)
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("uriTemplate"), Def.UriTemplate);
		Obj->SetStringField(TEXT("name"), Def.Name);
		Obj->SetStringField(TEXT("description"), Def.Description);
		Obj->SetStringField(TEXT("mimeType"), Def.MimeType);
		return MakeShared<FJsonValueObject>(Obj);
	}
}  // namespace McpResourceRead
