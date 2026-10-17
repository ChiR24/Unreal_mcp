#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// MCP 2026-07-28 ("modern") requests: no initialize and no session; each request carries its protocol version and
// client capabilities in params._meta, and on HTTP mirrors its method and name into headers. Earlier versions keep
// their initialize-and-session path; a server serving both picks per request (versioning: dual-era).
namespace McpModern
{
inline const TCHAR* const Version = TEXT("2026-07-28");
constexpr int32 ErrorHeaderMismatch = -32020;
constexpr int32 ErrorUnsupportedProtocolVersion = -32022;

struct FMeta
{
    FString ProtocolVersion;
    bool bTasks = false;            // extensions["io.modelcontextprotocol/tasks"]
    bool bElicitationForm = false;  // elicitation, form mode
};

// Whether a request's params carry the modern protocol version, and what its _meta declares.
bool IsModern(const TSharedPtr<FJsonObject>& Params);
FMeta ReadMeta(const TSharedPtr<FJsonObject>& Params);

// The reason the HTTP headers disagree with the body (HeaderMismatch), empty when they agree.
FString HeaderMismatch(const FString& HeaderVersion, const FString& HeaderMethod, const FString& HeaderName,
                       const FString& Method, const TSharedPtr<FJsonObject>& Params, const FString& MetaVersion);

// The error body for a version this server does not speak, listing the ones it does.
FString UnsupportedVersionError(const TSharedPtr<FJsonValue>& Id, const FString& Requested,
                                const TArray<FString>& Supported);

// Gives a result the fields 2026-07-28 requires: resultType, and ttlMs/cacheScope on listings and reads.
void StampResult(const FString& Method, const TSharedPtr<FJsonObject>& Result);
// The same on a serialized JSON-RPC response; anything that is not one comes back unchanged.
FString StampResponseBody(const FString& Body, const FString& Method);

// A task as the tasks extension carries it: the handle a long call answers (ResultType "task"), or the answer to
// tasks/get (ResultType "complete").
TSharedPtr<FJsonObject> TaskObject(const FString& TaskId, const FString& Status, const FString& StatusMessage,
                                   const FDateTime& CreatedAt, const FDateTime& UpdatedAt, const TCHAR* ResultType);

// The modern request the transport thread is serving, set for the synchronous part of its handling so the shared
// legacy code paths (SendHttpResponse, StreamToolCall, the execute consent check) can shape their replies for it.
struct FCall
{
    bool bActive = false;
    bool bTasks = false;
    bool bElicitationForm = false;
    FString Method;
    // A retry's answer to an input_required result (params.requestState and params.inputResponses).
    FString RequestState;
    TSharedPtr<FJsonObject> InputResponses;
};
const FCall& Current();

struct FScope
{
    explicit FScope(const FCall& Call);
    ~FScope();

private:
    FCall Saved;
};
} // namespace McpModern
