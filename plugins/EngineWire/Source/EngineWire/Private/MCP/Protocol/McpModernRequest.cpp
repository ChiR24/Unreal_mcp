#include "MCP/Protocol/McpModernRequest.h"

#include "Containers/StringConv.h"
#include "MCP/Protocol/McpJsonRpc.h"
#include "Misc/Base64.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace McpModern
{
namespace
{
const TCHAR* const MetaVersionKey = TEXT("io.modelcontextprotocol/protocolVersion");

TSharedPtr<FJsonObject> ObjectField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
{
    const TSharedPtr<FJsonObject>* Found = nullptr;
    return Object.IsValid() && Object->TryGetObjectField(Name, Found) && Found ? *Found : TSharedPtr<FJsonObject>();
}

// A header value may carry the =?base64?...?= sentinel (a name outside the header-safe characters).
FString DecodeHeaderValue(const FString& Value)
{
    if (!Value.StartsWith(TEXT("=?base64?"), ESearchCase::CaseSensitive) ||
        !Value.EndsWith(TEXT("?="), ESearchCase::CaseSensitive) || Value.Len() < 11)
    {
        return Value;
    }
    TArray<uint8> Bytes;
    if (!FBase64::Decode(Value.Mid(9, Value.Len() - 11), Bytes))
    {
        return Value;
    }
    const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
    return FString(Text.Length(), Text.Get());
}
} // namespace

bool IsModern(const TSharedPtr<FJsonObject>& Params)
{
    const TSharedPtr<FJsonObject> Meta = ObjectField(Params, TEXT("_meta"));
    return Meta.IsValid() && Meta->HasTypedField<EJson::String>(MetaVersionKey);
}

FMeta ReadMeta(const TSharedPtr<FJsonObject>& Params)
{
    FMeta Out;
    const TSharedPtr<FJsonObject> Meta = ObjectField(Params, TEXT("_meta"));
    if (!Meta.IsValid())
    {
        return Out;
    }
    Meta->TryGetStringField(MetaVersionKey, Out.ProtocolVersion);
    const TSharedPtr<FJsonObject> Caps = ObjectField(Meta, TEXT("io.modelcontextprotocol/clientCapabilities"));
    const TSharedPtr<FJsonObject> Extensions = ObjectField(Caps, TEXT("extensions"));
    Out.bTasks = Extensions.IsValid() && Extensions->HasField(TEXT("io.modelcontextprotocol/tasks"));
    // An elicitation capability with no mode named means form, as the spec keeps it for older clients.
    const TSharedPtr<FJsonObject> Elicitation = ObjectField(Caps, TEXT("elicitation"));
    Out.bElicitationForm = Elicitation.IsValid() &&
        (Elicitation->HasField(TEXT("form")) || !Elicitation->HasField(TEXT("url")));
    return Out;
}

FString HeaderMismatch(const FString& HeaderVersion, const FString& HeaderMethod, const FString& HeaderName,
                       const FString& Method, const TSharedPtr<FJsonObject>& Params, const FString& MetaVersion)
{
    if (HeaderVersion != MetaVersion)
    {
        return HeaderVersion.IsEmpty()
            ? FString(TEXT("Header mismatch: the MCP-Protocol-Version header is missing"))
            : FString::Printf(TEXT("Header mismatch: MCP-Protocol-Version header value '%s' does not match body value '%s'"),
                  *HeaderVersion, *MetaVersion);
    }
    if (HeaderMethod != Method)
    {
        return HeaderMethod.IsEmpty()
            ? FString(TEXT("Header mismatch: the Mcp-Method header is missing"))
            : FString::Printf(TEXT("Header mismatch: Mcp-Method header value '%s' does not match body value '%s'"),
                  *HeaderMethod, *Method);
    }
    // The tasks methods name their task in Mcp-Name too: a mismatched taskId was served here while the SDK refused it.
    const TCHAR* NameField = Method == TEXT("resources/read") ? TEXT("uri")
        : (Method == TEXT("tools/call") || Method == TEXT("prompts/get")) ? TEXT("name")
        : (Method == TEXT("tasks/get") || Method == TEXT("tasks/update") || Method == TEXT("tasks/cancel")) ? TEXT("taskId") : nullptr;
    if (!NameField)
    {
        return FString();
    }
    FString BodyName;
    if (Params.IsValid())
    {
        Params->TryGetStringField(NameField, BodyName);
    }
    const FString Decoded = DecodeHeaderValue(HeaderName);
    if (Decoded != BodyName)
    {
        return HeaderName.IsEmpty()
            ? FString(TEXT("Header mismatch: the Mcp-Name header is missing"))
            : FString::Printf(TEXT("Header mismatch: Mcp-Name header value '%s' does not match body value '%s'"),
                  *Decoded, *BodyName);
    }
    return FString();
}

FString UnsupportedVersionError(const TSharedPtr<FJsonValue>& Id, const FString& Requested,
                                const TArray<FString>& Supported)
{
    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Versions;
    for (const FString& Each : Supported)
    {
        Versions.Add(MakeShared<FJsonValueString>(Each));
    }
    Data->SetArrayField(TEXT("supported"), Versions);
    Data->SetStringField(TEXT("requested"), Requested);
    return FMcpJsonRpc::BuildError(Id, ErrorUnsupportedProtocolVersion, TEXT("Unsupported protocol version"), Data);
}

void StampResult(const FString& Method, const TSharedPtr<FJsonObject>& Result)
{
    if (!Result.IsValid())
    {
        return;
    }
    if (!Result->HasField(TEXT("resultType")))
    {
        Result->SetStringField(TEXT("resultType"), TEXT("complete"));
    }
    // The tool, prompt and template listings never change while the editor runs; the resource list follows the
    // editor's state loosely and a read is live state, so neither is shared and a read is never reused.
    double TtlMs = -1.0;
    const TCHAR* Scope = TEXT("private");
    if (Method == TEXT("tools/list") || Method == TEXT("prompts/list") || Method == TEXT("resources/templates/list"))
    {
        TtlMs = 3600000.0;
        Scope = TEXT("public");
    }
    else if (Method == TEXT("resources/list"))
    {
        TtlMs = 60000.0;
    }
    else if (Method == TEXT("resources/read"))
    {
        TtlMs = 0.0;
    }
    if (TtlMs >= 0.0 && !Result->HasField(TEXT("ttlMs")))
    {
        Result->SetNumberField(TEXT("ttlMs"), TtlMs);
        Result->SetStringField(TEXT("cacheScope"), Scope);
    }
}

FString StampResponseBody(const FString& Body, const FString& Method)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    const TSharedPtr<FJsonObject>* Result = nullptr;
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() ||
        !Root->TryGetObjectField(TEXT("result"), Result) || !Result)
    {
        return Body;
    }
    StampResult(Method, *Result);
    return FMcpJsonRpc::JsonToString(Root);
}

TSharedPtr<FJsonObject> TaskObject(const FString& TaskId, const FString& Status, const FString& StatusMessage,
                                   const FDateTime& CreatedAt, const FDateTime& UpdatedAt, const TCHAR* ResultType)
{
    TSharedPtr<FJsonObject> Task = MakeShared<FJsonObject>();
    Task->SetStringField(TEXT("resultType"), ResultType);
    Task->SetStringField(TEXT("taskId"), TaskId);
    Task->SetStringField(TEXT("status"), Status);
    if (!StatusMessage.IsEmpty())
    {
        Task->SetStringField(TEXT("statusMessage"), StatusMessage);
    }
    Task->SetStringField(TEXT("createdAt"), CreatedAt.ToIso8601());
    Task->SetStringField(TEXT("lastUpdatedAt"), UpdatedAt.ToIso8601());
    // ponytail: tasks are kept by count (FMcpTaskResults::MaxKept), not by time, so no ttl is promised; give
    // them a lifetime and report it here if a client needs one.
    Task->SetField(TEXT("ttlMs"), MakeShared<FJsonValueNull>());
    if (Status == TEXT("working"))
    {
        Task->SetNumberField(TEXT("pollIntervalMs"), 2000);
    }
    return Task;
}

namespace
{
FCall& Mutable()
{
    thread_local FCall Call;
    return Call;
}
} // namespace

const FCall& Current()
{
    return Mutable();
}

FScope::FScope(const FCall& Call)
    : Saved(Mutable())
{
    Mutable() = Call;
}

FScope::~FScope()
{
    Mutable() = Saved;
}
} // namespace McpModern
