#include "MCP/Resources/McpResourceReadContent.h"
#include "MCP/Resources/McpResourceCatalog.h"
#include "MCP/Resources/McpResourceBridgeContent.h"
#include "MCP/Resources/McpResourceHealthContent.h"
#include "MCP/Resources/McpResourceUri.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "Foundation/McpLiveStateRevisions.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"

namespace
{
	const TCHAR* CatalogUri = TEXT("ue://capability/catalog");
	const TCHAR* ProjectUri = TEXT("ue://project");
	const TCHAR* VersionUri = TEXT("ue://version");
	const TCHAR* AutomationBridgeUri = TEXT("ue://automation-bridge");
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
		// Advertised implies readable. IsListedResourceUri is the SAME filter
		// resources/list loops, so a resource cannot be advertised without
		// landing in this branch, and BuildReadBody below must handle it.
		if (McpResourceCatalog::IsListedResourceUri(Uri))
		{
			return EReadKind::SocketReadable;
		}
		if (McpResourceCatalog::IsNativeUnservedUri(Uri) || McpResourceCatalog::MatchesKnownTemplate(Uri))
		{
			return EReadKind::EditorUnavailable;
		}
		return EReadKind::Unknown;
	}

	FString UnavailableMessage(const FString& Uri)
	{
		if (McpResourceCatalog::IsNativeUnservedUri(Uri))
		{
			return FString::Printf(
				TEXT("RESOURCE_UNAVAILABLE: %s is live editor state that only the game thread can read, so the "
				     "native /mcp transport does not advertise or serve it; read it over the stdio transport"),
				*Uri);
		}
		return TEXT("RESOURCE_UNAVAILABLE: editor-state resource is not readable from the transport thread");
	}

	FReadBody BuildReadBody(const FString& Uri, FMcpResourceRevision Revision)
	{
		auto Root = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
		if (Uri == ProjectUri)
		{
			Data = BuildProjectData();
		}
		else if (Uri == McpResourceCatalog::HealthUri())
		{
			Data = McpResourceHealth::BuildHealthData();
		}
		else if (Uri == VersionUri)
		{
			Data = McpResourceBridge::BuildEngineVersionData();
		}
		else if (Uri == AutomationBridgeUri)
		{
			Data = McpResourceBridge::BuildAutomationBridgeData();
		}
		else if (Uri == McpResourceCatalog::LiveStateRevisionUri())
		{
			const FMcpLiveStateRevisionSnapshot Snapshot = FMcpLiveStateRevisions::Get().Snapshot();
			Revision = Snapshot.Max();
			Data = Snapshot.ToJson();
		}
		else
		{
			Data = BuildCatalogData();
		}
		Root->SetNumberField(TEXT("revision"), static_cast<double>(Revision));
		Root->SetObjectField(TEXT("data"), Data);
		return { Revision, SerializeCompact(Root) };
	}

	FString BuildReadBodyText(const FString& Uri, FMcpResourceRevision Revision)
	{
		return BuildReadBody(Uri, Revision).Text;
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
