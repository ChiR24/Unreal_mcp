// McpNativeTransportGateway.cpp — route tools/call for the 'unreal' gateway tool

#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Gateway/McpNativeGatewayDefinition.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayDescribe.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "MCP/Gateway/McpNativeGatewayDirectCallMigration.h"

namespace
{
// The visibility-changing configure actions (native mirror of the TS
// TOOL_LIST_CHANGED_ACTIONS set). Only these fold into a catalog revision.
bool McpIsVisibilityConfigureAction(const FString& Action)
{
	return Action == TEXT("enable_tools") || Action == TEXT("disable_tools")
		|| Action == TEXT("enable_category") || Action == TEXT("disable_category")
		|| Action == TEXT("reset");
}

// Mirror a successful visibility change onto this session's revisioned overlay.
// The store advances its per-session revision only when the enabled-flag
// fingerprint actually moves, so a no-op configure leaves the revision (and the
// downstream SyncCatalog) idle. The store has no EnableCategory mutator, so
// enable_category re-enables the category's tools via EnableTools (which also
// re-enables their category), keeping parity with the global manager.
void McpApplyConfigureVisibility(
	FMcpSessionConfigureStore& Store, const FString& Action,
	const FString& SessionId, const TSharedPtr<FJsonObject>& Args)
{
	auto ToolNames = [&Args]()
	{
		TArray<FString> Names;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Args.IsValid() && Args->TryGetArrayField(TEXT("tools"), Arr) && Arr)
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V->TryGetString(S)) Names.Add(S);
			}
		}
		return Names;
	};

	if (Action == TEXT("enable_tools")) { Store.EnableTools(SessionId, ToolNames()); return; }
	if (Action == TEXT("disable_tools")) { Store.DisableTools(SessionId, ToolNames()); return; }
	if (Action == TEXT("reset")) { Store.Reset(SessionId); return; }

	FString Category;
	if (Args.IsValid()) Args->TryGetStringField(TEXT("category"), Category);
	if (Action == TEXT("disable_category")) { Store.DisableCategory(SessionId, Category); return; }
	if (Action == TEXT("enable_category"))
	{
		const FMcpToolRegistry& Registry = FMcpToolRegistry::Get();
		TArray<FString> InCategory;
		// `all` is a wildcard, not a category name. DisableCategory already
		// special-cases it in the store, but the mirror here only matched exact
		// category names -- so `enable_category all` re-enabled everything in the
		// global manager while leaving the session overlay fully disabled, and
		// because the overlay fingerprint never moved, no resources/updated
		// announced the divergence.
		const bool bAll = Category == TEXT("all");
		for (const FString& ToolName : Registry.GetToolNames())
		{
			if (bAll || Registry.GetToolCategory(ToolName) == Category) InCategory.Add(ToolName);
		}
		Store.EnableTools(SessionId, InCategory);
	}
}
}  // namespace

void FMcpNativeTransport::HandleGatewayCall(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	auto SendOneShot = [&](const TSharedPtr<FJsonObject>& ToolResult, int32 Status = 200)
	{
		const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, Status, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
	};

	if (!Params.IsValid())
	{
		SendOneShot(FMcpJsonRpc::BuildToolResult(false,
			TEXT("operation must be search, describe, execute, or configure."),
			nullptr, TEXT("UNKNOWN_OPERATION")));
		return;
	}

	FString Operation;
	if (!Params->TryGetStringField(TEXT("operation"), Operation) || Operation.IsEmpty())
	{
		SendOneShot(FMcpJsonRpc::BuildToolResult(false,
			TEXT("operation must be search, describe, execute, or configure."),
			nullptr, TEXT("UNKNOWN_OPERATION")));
		return;
	}

	// Every gateway operation requires a valid session.
	{
		FScopeLock SessionLock(&SessionMutex);
		if (!ActiveSessions.Contains(SessionId))
		{
			const FString Body = FMcpJsonRpc::BuildError(
				Id, FMcpJsonRpc::ErrorInvalidRequest,
				TEXT("Invalid or expired session ID"));
			SendHttpResponse(ClientSocket, 404, TEXT("application/json"), Body, {}, CorsOrigin);
			ClientSocket->Close();
			if (SocketSub) SocketSub->DestroySocket(ClientSocket);
			return;
		}
	}

	const FMcpToolRegistry& Registry = FMcpToolRegistry::Get();

	// Discovery reads the generated capability store on this thread: pure data,
	// no editor API, so it never blocks the socket thread on Unreal work.
	const FMcpCapabilityStore& CapabilityStore = FMcpCapabilityStore::Get();
	auto IsToolEnabled = [this](const FString& ToolName) { return ToolManager.IsToolEnabled(ToolName); };

	auto SendDiscoveryResult = [&](const TSharedPtr<FJsonObject>& Result)
	{
		bool bOk = false;
		if (Result.IsValid()) Result->TryGetBoolField(TEXT("success"), bOk);
		const FString Msg = bOk ? TEXT("ok")
			: (Result.IsValid() ? Result->GetStringField(TEXT("message")) : TEXT("discovery failed"));
		const FString Code = bOk ? FString()
			: (Result.IsValid() ? Result->GetStringField(TEXT("errorCode")) : FString());
		SendOneShot(FMcpJsonRpc::BuildToolResult(bOk, Msg, Result, Code));
	};

	if (Operation == TEXT("search"))
	{
		FMcpDiscoveryQuery DiscoveryQuery;
		Params->TryGetStringField(TEXT("query"), DiscoveryQuery.Query);
		DiscoveryQuery.bHasDomain = Params->TryGetStringField(TEXT("domain"), DiscoveryQuery.Domain);
		DiscoveryQuery.bHasFamily = Params->TryGetStringField(TEXT("family"), DiscoveryQuery.Family);
		DiscoveryQuery.Limit = McpSearchDefaultLimit;
		if (Params->HasField(TEXT("limit")))
		{
			int32 L = 0;
			if (Params->TryGetNumberField(TEXT("limit"), L)) DiscoveryQuery.Limit = FMath::Clamp(L, 1, McpSearchMaxLimit);
		}
		if (Params->HasField(TEXT("offset")))
		{
			int32 O = 0;
			if (Params->TryGetNumberField(TEXT("offset"), O)) DiscoveryQuery.Offset = FMath::Max(0, O);
		}
		if (Params->HasField(TEXT("maxBytes")))
		{
			int32 B = 0;
			if (Params->TryGetNumberField(TEXT("maxBytes"), B)) DiscoveryQuery.MaxBytes = FMath::Clamp(B, 512, 262144);
		}
		SendDiscoveryResult(McpGatewaySearchCapabilities(DiscoveryQuery, CapabilityStore, IsToolEnabled));
		return;
	}

	if (Operation == TEXT("describe"))
	{
		FMcpDiscoveryQuery DiscoveryQuery;
		Params->TryGetStringField(TEXT("tool"), DiscoveryQuery.Tool);
		DiscoveryQuery.bHasAction = Params->TryGetStringField(TEXT("action"), DiscoveryQuery.Action);
		DiscoveryQuery.bHasParam = Params->TryGetStringField(TEXT("param"), DiscoveryQuery.Param);
		Params->TryGetStringField(TEXT("query"), DiscoveryQuery.Query);
		DiscoveryQuery.Limit = McpDescribeDefaultLimit;
		if (Params->HasField(TEXT("limit")))
		{
			int32 L = 0;
			if (Params->TryGetNumberField(TEXT("limit"), L)) DiscoveryQuery.Limit = FMath::Clamp(L, 1, McpDescribeMaxLimit);
		}
		// Accept `actionOffset` as an alias of `offset`: the tool summary response
		// echoes its own paging state as actionOffset/actionLimit/actionHasMore,
		// so a client paging through a parent's action list naturally replays that
		// field name. Silently ignoring it made actions beyond the first page
		// unreachable for any client that followed the response's own field names.
		if (Params->HasField(TEXT("offset")))
		{
			int32 O = 0;
			if (Params->TryGetNumberField(TEXT("offset"), O)) DiscoveryQuery.Offset = FMath::Max(0, O);
		}
		else if (Params->HasField(TEXT("actionOffset")))
		{
			int32 O = 0;
			if (Params->TryGetNumberField(TEXT("actionOffset"), O)) DiscoveryQuery.Offset = FMath::Max(0, O);
		}
		SendDiscoveryResult(McpGatewayDescribeCapability(DiscoveryQuery, CapabilityStore, IsToolEnabled));
		return;
	}

	if (Operation == TEXT("configure"))
	{
		FString Action;
		Params->TryGetStringField(TEXT("action"), Action);
		if (Action.IsEmpty())
		{
			SendOneShot(FMcpJsonRpc::BuildToolResult(false,
				TEXT("configure requires a manage_tools action."), nullptr, TEXT("MISSING_ACTION")));
			return;
		}
		TSharedPtr<FJsonObject> ManageArgs = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* Nested = nullptr;
		if (Params->TryGetObjectField(TEXT("params"), Nested) && *Nested)
		{
			ManageArgs->Values = (*Nested)->Values;
		}
		ManageArgs->SetStringField(TEXT("action"), Action);
		TSharedPtr<FJsonObject> Result = ToolManager.HandleAction(Action, ManageArgs);
		bool bOk = false;
		if (Result.IsValid()) Result->TryGetBoolField(TEXT("success"), bOk);

		// A successful visibility change is mirrored onto this session's revisioned
		// configure overlay, then folded into one coalesced ue://capability/catalog
		// resources/updated for subscribed sessions. The overlay fingerprint compare
		// plus SyncCatalog's per-session cursor make this effective-change-only: a
		// no-op configure advances no revision and enqueues no notification.
		if (bOk && McpIsVisibilityConfigureAction(Action))
		{
			InitializePrimitivesIfNeeded();
			McpApplyConfigureVisibility(SessionConfigureStore, Action, SessionId, ManageArgs);
			FMcpNotificationCoalescer* Coalescer = nullptr;
			{
				FScopeLock PrimitiveLock(&PrimitiveStateMutex);
				Coalescer = NotificationCoalescer.Get();
			}
			if (Coalescer) Coalescer->SyncCatalog(SessionId);
		}

		const FString Msg = bOk ? TEXT("ok")
			: (Result.IsValid() ? Result->GetStringField(TEXT("error")) : TEXT("configure failed"));
		SendOneShot(FMcpJsonRpc::BuildToolResult(bOk, Msg, Result));
		return;
	}

	if (Operation == TEXT("execute"))
	{
		HandleGatewayExecute(Params, Id, ClientSocket, SessionId, CorsOrigin, ProgressToken);
		return;
	}

	SendOneShot(FMcpJsonRpc::BuildToolResult(false,
		TEXT("operation must be search, describe, execute, or configure."),
		nullptr, TEXT("UNKNOWN_OPERATION")));
}

bool FMcpNativeTransport::HandleGatewayModePreDispatch(
	const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
	const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
	const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	// The public surface is permanently the single static 'unreal' gateway tool;
	// route it through the gateway's search/describe/execute/configure operations.
	if (ToolName == TEXT("unreal"))
	{
		HandleGatewayCall(Arguments, Id, ClientSocket, SessionId, CorsOrigin, ProgressToken);
		return true;
	}

	// Every other (removed) direct tool name gets a bounded, executable migration
	// receipt built by the shared builder (mirrors TS buildDirectCallMigration).
	// Total and non-dispatching: no legacy direct-call path runs after this.
	const TArray<FString> ParentNames = FMcpToolRegistry::Get().GetToolNames().Array();
	const TSharedPtr<FJsonObject> Migration =
		McpBuildDirectCallMigration(ToolName, Arguments, ParentNames);
	FString Message;
	Migration->TryGetStringField(TEXT("message"), Message);
	const TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
		false, Message, Migration, TEXT("DIRECT_TOOL_CALL_REMOVED"));
	const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
	SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
	ClientSocket->Close();
	if (SocketSub) SocketSub->DestroySocket(ClientSocket);
	return true;
}
