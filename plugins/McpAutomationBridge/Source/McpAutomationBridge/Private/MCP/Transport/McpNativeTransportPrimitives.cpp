#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Execute/McpNativeGatewayAuthorization.h"
#include "MCP/Resources/McpResourceCatalog.h"
#include "MCP/Resources/McpResourceUri.h"
#include "MCP/Resources/McpResourceReadContent.h"
#include "MCP/Primitives/McpResourceRevision.h"
#include "MCP/Primitives/McpPromptCatalog.h"
#include "MCP/Primitives/McpPromptRender.h"
#include "MCP/Primitives/McpCompletionProvider.h"
#include "MCP/Primitives/McpCompletionPools.h"
#include "MCP/Primitives/McpSubscriptionStore.h"

// Task 37 (native mirror of primitive-handlers.ts + primitive-wiring.ts): the
// JSON-RPC handlers for resources/*, prompts/*, and completion/complete. Each
// delegates to the pure Tasks 31-36 primitives; none re-implements their
// algorithms. Static capability/project reads are served safely from the socket
// thread; editor-state URIs return a typed RESOURCE_UNAVAILABLE rather than
// scanning editor APIs off-thread. resources/updated delivery lives in the
// sibling McpNativeTransportPrimitiveNotifications.cpp.

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
	TaskSurface.CloseSession(SessionId);
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

	// Anything outside these families is not ours: return false FIRST so the
	// caller still reports method-not-found instead of a policy refusal.
	if (!Method.StartsWith(TEXT("resources/")) && !Method.StartsWith(TEXT("prompts/")) &&
		Method != TEXT("completion/complete"))
	{
		return false;
	}
	// Every primitive below reads project data, so every one of them is gated on
	// the session principal. Before this, only tools/call consulted the
	// principal, so a write-only or path-confined token could still read actor,
	// asset and level state outside its grant.
	{
		const FMcpCapabilityPrincipal Principal = GetSessionPrincipal(SessionId);
		const FString Refusal = McpAuthorizePrimitiveRead(Principal, Uri, Id);
		if (!Refusal.IsEmpty())
		{
			Reply(Refusal);
			return true;
		}
	}

	if (Method == TEXT("resources/list"))
	{
		auto Result = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const FMcpResourceDefinition& Def : McpResourceCatalog::AllListedResources())
		{
			Items.Add(McpResourceRead::ListEntry(Def));
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
			Items.Add(McpResourceRead::TemplateEntry(Def));
		}
		Result->SetArrayField(TEXT("resourceTemplates"), Items);
		Reply(FMcpJsonRpc::BuildResponse(Id, Result));
		return true;
	}
	if (Method == TEXT("resources/read"))
	{
		const McpResourceRead::EReadKind Kind = McpResourceRead::Classify(Uri);
		if (Kind == McpResourceRead::EReadKind::Unknown)
		{
			auto Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("code"), TEXT("RESOURCE_NOT_FOUND"));
			Reply(FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
				FString::Printf(TEXT("RESOURCE_NOT_FOUND: unknown resource: %s"), *Uri), Data));
			return true;
		}
		if (Kind == McpResourceRead::EReadKind::EditorUnavailable)
		{
			auto Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("code"), TEXT("RESOURCE_UNAVAILABLE"));
			Reply(FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidRequest,
				TEXT("RESOURCE_UNAVAILABLE: editor-state resource is not readable from the transport thread"), Data));
			return true;
		}
			const McpResourceRead::FReadBody ReadBody =
				McpResourceRead::BuildReadBody(Uri, McpInitialResourceRevision);
			auto Content = MakeShared<FJsonObject>();
			Content->SetStringField(TEXT("uri"), Uri);
			Content->SetStringField(TEXT("mimeType"), McpResourceCatalog::JsonMimeType());
			Content->SetNumberField(TEXT("revision"), ReadBody.Revision);
			Content->SetStringField(TEXT("text"), ReadBody.Text);
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
		Result->SetArrayField(TEXT("prompts"), McpBuildPromptListEntries());
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
		TMap<FString, FString> Args;
		const TSharedPtr<FJsonObject>* ArgsObj = nullptr;
		if (Params.IsValid() && Params->TryGetObjectField(TEXT("arguments"), ArgsObj) && ArgsObj)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ArgsObj)->Values)
			{
				FString Val;
				if (Pair.Value.IsValid() && Pair.Value->TryGetString(Val))
				{
					Args.Add(Pair.Key, Val);
				}
			}
		}
		const FMcpPromptRenderResult Render = McpRenderWorkflowPrompt(Name, Args);
		if (!Render.bOk)
		{
			auto Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("code"), Render.ErrorCode);
			Reply(FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams, Render.ErrorMessage, Data));
			return true;
		}
		auto ContentObj = MakeShared<FJsonObject>();
		ContentObj->SetStringField(TEXT("type"), TEXT("text"));
		ContentObj->SetStringField(TEXT("text"), Render.Body);
		auto MsgObj = MakeShared<FJsonObject>();
		MsgObj->SetStringField(TEXT("role"), TEXT("user"));
		MsgObj->SetObjectField(TEXT("content"), ContentObj);
		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(MsgObj));
		auto Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("description"), Render.Description);
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
		const TSet<FString> Enabled = McpEnabledCapabilityIds(
			[this, &SessionId](const FString& Parent) { return SessionConfigureStore.IsToolEnabled(SessionId, Parent); });
		const FMcpCompletionOutcome Outcome = McpCompleteFromPool(
			RefType, RefId, ArgName, Value,
			McpCapabilityCompletionPool(), McpProjectHandleCompletionPool(), Enabled);
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
