#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Resources/McpResourceCatalog.h"
#include "MCP/Resources/McpResourceUri.h"
#include "MCP/Primitives/McpResourceRevision.h"
#include "MCP/Primitives/McpPromptCatalog.h"
#include "MCP/Primitives/McpCompletionProvider.h"
#include "MCP/Primitives/McpSubscriptionStore.h"

// Task 37 (native mirror of primitive-handlers.ts + primitive-wiring.ts): the
// JSON-RPC handlers for resources/*, prompts/*, and completion/complete. Each
// delegates to the pure Tasks 31-36 primitives; none re-implements their
// algorithms. Static capability/project reads are served safely from the socket
// thread; editor-state URIs return a typed RESOURCE_UNAVAILABLE rather than
// scanning editor APIs off-thread. resources/updated delivery lives in the
// sibling McpNativeTransportPrimitiveNotifications.cpp.

namespace
{
// The only URIs whose read is a bounded static/project payload safe to serve from
// the socket thread. Every editor-state URI needs a game-thread scan we must not
// run here, so it returns a typed RESOURCE_UNAVAILABLE instead.
bool IsSocketThreadReadableResource(const FString& Uri)
{
	return Uri == TEXT("ue://capability/catalog") || Uri == TEXT("ue://project");
}

TSharedPtr<FJsonValue> ResourceEntry(const FMcpResourceDefinition& Def)
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
}  // namespace

void FMcpNativeTransport::InitializePrimitivesIfNeeded()
{
	FScopeLock Lock(&PrimitiveStateMutex);
	if (NotificationCoalescer.IsValid())
	{
		return;
	}
	// Seed the per-session configure overlay ONCE from the same registry the
	// global ToolManager reads. SeedFrom empties overlays, so it belongs in this
	// construct-once seam; each session's overlay is cloned lazily on its first
	// configure mutation and carries its own catalog-state revision.
	const FMcpToolRegistry& Registry = FMcpToolRegistry::Get();
	TArray<FMcpSessionConfigureStore::FSeedEntry> ConfigureSeed;
	for (const FString& ToolName : Registry.GetToolNames())
	{
		ConfigureSeed.Add({ ToolName, Registry.GetToolCategory(ToolName) });
	}
	SessionConfigureStore.SeedFrom(ConfigureSeed);
	// Releasing a (session, URI) drops its coalescer pending so a released
	// subscription can never flush a late update.
	SubscriptionStore.SetReleaseHook(
		[this](const FString& InSessionId, const FString& InUri)
		{
			if (NotificationCoalescer.IsValid())
			{
				NotificationCoalescer->DropPending(InSessionId, InUri);
			}
		});
	NotificationCoalescer = MakeUnique<FMcpNotificationCoalescer>(
		SubscriptionStore,
		[](const FString&) -> FMcpResourceRevision { return McpInitialResourceRevision; },
		SessionConfigureStore,
		[this](const FString& InSessionId, const FMcpResourceUpdatedPayload& Payload)
		{
			SendResourceUpdatedNotification(InSessionId, Payload.Uri);
		},
		[]() -> int64 { return static_cast<int64>(FPlatformTime::Seconds() * 1000.0); });
}

void FMcpNativeTransport::ReleaseSessionPrimitives(const FString& SessionId)
{
	if (SessionId.IsEmpty())
	{
		return;
	}
	// ClearSession fires the store release hook (dropping matching pending); the
	// coalescer ClearSession then drops the session's cursor. Both idempotent.
	SubscriptionStore.ClearSession(SessionId);
	// Drop this session's configure overlay so a reused session id restarts
	// pristine; the coalescer cursor is dropped just below, so no revision leaks.
	SessionConfigureStore.ClearSession(SessionId);
	FScopeLock Lock(&PrimitiveStateMutex);
	if (NotificationCoalescer.IsValid())
	{
		NotificationCoalescer->ClearSession(SessionId);
	}
}

bool FMcpNativeTransport::HandlePrimitiveMethod(
	const FString& Method, const TSharedPtr<FJsonObject>& Params,
	const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
	const FString& SessionId, const FString& CorsOrigin)
{
	InitializePrimitivesIfNeeded();
	auto Reply = [&](const FString& Body)
	{
		SendAndClose(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
	};
	FString Uri;
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("uri"), Uri);
	}

	if (Method == TEXT("resources/list"))
	{
		auto Result = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const FMcpResourceDefinition& Def : McpResourceCatalog::NewStaticResources())
		{
			Items.Add(ResourceEntry(Def));
		}
		Result->SetArrayField(TEXT("resources"), Items);
		Reply(FMcpJsonRpc::BuildResponse(Id, Result));
		return true;
	}
	if (Method == TEXT("resources/templates/list"))
	{
		auto Result = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const FMcpResourceTemplateDefinition& Def : McpResourceCatalog::Templates())
		{
			Items.Add(TemplateEntry(Def));
		}
		Result->SetArrayField(TEXT("resourceTemplates"), Items);
		Reply(FMcpJsonRpc::BuildResponse(Id, Result));
		return true;
	}
	if (Method == TEXT("resources/read"))
	{
		if (!IsSocketThreadReadableResource(Uri))
		{
			auto Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("code"), TEXT("RESOURCE_UNAVAILABLE"));
			Reply(FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidRequest,
				TEXT("RESOURCE_UNAVAILABLE: editor-state resource is not readable from the transport thread"), Data));
			return true;
		}
		auto Content = MakeShared<FJsonObject>();
		Content->SetStringField(TEXT("uri"), Uri);
		Content->SetStringField(TEXT("mimeType"), McpResourceCatalog::JsonMimeType());
		Content->SetNumberField(TEXT("revision"), McpInitialResourceRevision);
		Content->SetStringField(TEXT("text"),
			FString::Printf(TEXT("{\"uri\":\"%s\",\"revision\":1}"), *Uri));
		TArray<TSharedPtr<FJsonValue>> Contents;
		Contents.Add(MakeShared<FJsonValueObject>(Content));
		auto Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("contents"), Contents);
		Reply(FMcpJsonRpc::BuildResponse(Id, Result));
		return true;
	}
	if (Method == TEXT("resources/subscribe"))
	{
		const FMcpSubscribeResult Sub = SubscriptionStore.Subscribe(SessionId, Uri);
		if (!Sub.bAccepted)
		{
			// A server-originated error (not method-not-found): the handler exists
			// and discriminated the URI against the Task 31 allowlist.
			Reply(FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
				FString::Printf(TEXT("Resource is not subscribable: %s"), *Uri)));
			return true;
		}
		Reply(FMcpJsonRpc::BuildResponse(Id, MakeShared<FJsonObject>()));
		return true;
	}
	if (Method == TEXT("resources/unsubscribe"))
	{
		SubscriptionStore.Unsubscribe(SessionId, Uri);
		Reply(FMcpJsonRpc::BuildResponse(Id, MakeShared<FJsonObject>()));
		return true;
	}
	if (Method == TEXT("prompts/list"))
	{
		auto Result = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const FMcpWorkflowPrompt& Prompt : McpWorkflowPrompts())
		{
			auto Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("name"), Prompt.Id);
			Obj->SetStringField(TEXT("title"), Prompt.Title);
			Items.Add(MakeShared<FJsonValueObject>(Obj));
		}
		Result->SetArrayField(TEXT("prompts"), Items);
		Reply(FMcpJsonRpc::BuildResponse(Id, Result));
		return true;
	}
	if (Method == TEXT("prompts/get"))
	{
		FString Name;
		if (Params.IsValid())
		{
			Params->TryGetStringField(TEXT("name"), Name);
		}
		if (!McpIsWorkflowPromptId(Name))
		{
			Reply(FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
				FString::Printf(TEXT("Unknown workflow prompt: %s"), *Name)));
			return true;
		}
		const FString Body = FString::Printf(
			TEXT("# %s\nGuidance only. Discover parameters with the unreal gateway describe operation, then run one execute call at a time."),
			*Name);
		auto ContentObj = MakeShared<FJsonObject>();
		ContentObj->SetStringField(TEXT("type"), TEXT("text"));
		ContentObj->SetStringField(TEXT("text"), Body);
		auto MsgObj = MakeShared<FJsonObject>();
		MsgObj->SetStringField(TEXT("role"), TEXT("user"));
		MsgObj->SetObjectField(TEXT("content"), ContentObj);
		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(MsgObj));
		auto Result = MakeShared<FJsonObject>();
		Result->SetArrayField(TEXT("messages"), Messages);
		Reply(FMcpJsonRpc::BuildResponse(Id, Result));
		return true;
	}
	if (Method == TEXT("completion/complete"))
	{
		FString RefType, RefId, ArgName, Value;
		const TSharedPtr<FJsonObject>* RefObj = nullptr;
		if (Params.IsValid() && Params->TryGetObjectField(TEXT("ref"), RefObj) && RefObj)
		{
			(*RefObj)->TryGetStringField(TEXT("type"), RefType);
			if (!(*RefObj)->TryGetStringField(TEXT("name"), RefId))
			{
				(*RefObj)->TryGetStringField(TEXT("uri"), RefId);
			}
		}
		const TSharedPtr<FJsonObject>* ArgObj = nullptr;
		if (Params.IsValid() && Params->TryGetObjectField(TEXT("argument"), ArgObj) && ArgObj)
		{
			(*ArgObj)->TryGetStringField(TEXT("name"), ArgName);
			(*ArgObj)->TryGetStringField(TEXT("value"), Value);
		}
		const FMcpCompletionOutcome Outcome = McpCompleteFromPool(
			RefType, RefId, ArgName, Value,
			TArray<FMcpCompletionCandidate>(), TArray<FMcpCompletionCandidate>(), TSet<FString>());
		auto Completion = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& Candidate : Outcome.Result.Values)
		{
			Values.Add(MakeShared<FJsonValueString>(Candidate));
		}
		Completion->SetArrayField(TEXT("values"), Values);
		Completion->SetNumberField(TEXT("total"), Outcome.Result.Total);
		Completion->SetBoolField(TEXT("hasMore"), Outcome.Result.bHasMore);
		auto Result = MakeShared<FJsonObject>();
		Result->SetObjectField(TEXT("completion"), Completion);
		Reply(FMcpJsonRpc::BuildResponse(Id, Result));
		return true;
	}
	return false;
}
