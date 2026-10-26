#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
// McpNativeTransportGateway.cpp — route tools/call for the 'unreal' gateway tool

#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Gateway/McpNativeGatewayDefinition.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayDescribe.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "MCP/Gateway/McpNativeGatewayDirectCallMigration.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"


void FMcpNativeTransport::HandleGatewayCall(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken)
{
	auto SendOneShot = [&](const TSharedPtr<FJsonObject>& ToolResult, int32 Status = 200)
	{
		const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendAndClose(ClientSocket, Status, TEXT("application/json"), Body, {}, CorsOrigin);
	};

	const auto UnknownOperation = [&]()
	{
		SendOneShot(FMcpJsonRpc::BuildToolResult(false,
			TEXT("operation must be search, describe, execute, or configure."),
			nullptr, TEXT("UNKNOWN_OPERATION")));
	};
	FString Operation;
	if (!Params.IsValid() || !Params->TryGetStringField(TEXT("operation"), Operation) || Operation.IsEmpty())
	{
		UnknownOperation();
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
			SendAndClose(ClientSocket, 404, TEXT("application/json"), Body, {}, CorsOrigin);
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
		DiscoveryQuery.bHasTool = Params->TryGetStringField(TEXT("tool"), DiscoveryQuery.Tool);
		DiscoveryQuery.bHasAction = Params->TryGetStringField(TEXT("action"), DiscoveryQuery.Action);
		DiscoveryQuery.bHasCapability = Params->TryGetStringField(TEXT("capability"), DiscoveryQuery.Capability);
		DiscoveryQuery.bHasEffect = Params->TryGetStringField(TEXT("effect"), DiscoveryQuery.Effect);
		// A cursor supersedes offset; only a string is one (resolveOffset in gateway-search.ts).
		const TSharedPtr<FJsonValue> Cursor = Params->TryGetField(TEXT("cursor"));
		DiscoveryQuery.bHasCursor = Cursor.IsValid() && Cursor->Type == EJson::String;
		if (DiscoveryQuery.bHasCursor) DiscoveryQuery.Cursor = Cursor->AsString();
		// An integer past its bounds is clamped and the reply says so (integerCoercion in gateway-shared.ts).
		auto ReadBounded = [&](const TCHAR* Name, int32 Min, int32 Max, int32& Out)
		{
			int32 Requested = 0;
			if (!Params->HasField(Name) || !Params->TryGetNumberField(Name, Requested)) return;
			Out = FMath::Clamp(Requested, Min, Max);
			if (Out == Requested) return;
			auto Coercion = MakeShared<FJsonObject>();
			Coercion->SetStringField(TEXT("parameter"), Name);
			Coercion->SetNumberField(TEXT("requested"), Requested);
			Coercion->SetNumberField(TEXT("applied"), Out);
			Coercion->SetStringField(TEXT("reason"), Requested < Min
				? FString::Printf(TEXT("%s=%d is below the minimum of %d."), Name, Requested, Min)
				: FString::Printf(TEXT("%s=%d is above the maximum of %d."), Name, Requested, Max));
			DiscoveryQuery.Coercions.Add(MakeShared<FJsonValueObject>(Coercion));
		};
		if (!DiscoveryQuery.bHasCursor) ReadBounded(TEXT("offset"), 0, MAX_int32, DiscoveryQuery.Offset);
		DiscoveryQuery.Limit = McpSearchDefaultLimit;
		ReadBounded(TEXT("limit"), 1, McpSearchMaxLimit, DiscoveryQuery.Limit);
		ReadBounded(TEXT("maxBytes"), 512, 262144, DiscoveryQuery.MaxBytes);
		// Saved recipes ride on search only for a caller that may read them (Write does not imply Read).
		DiscoveryQuery.bMayReadProject = GetSessionPrincipal(SessionId).IsScopeAuthorized(EMcpCapabilityScope::Read);
		SendDiscoveryResult(McpGatewaySearchCapabilities(DiscoveryQuery, CapabilityStore, IsToolEnabled));
		return;
	}

	if (Operation == TEXT("describe"))
	{
		FMcpDiscoveryQuery DiscoveryQuery;
		Params->TryGetStringField(TEXT("tool"), DiscoveryQuery.Tool);
		DiscoveryQuery.bHasAction = Params->TryGetStringField(TEXT("action"), DiscoveryQuery.Action);
		DiscoveryQuery.bHasParam = Params->TryGetStringField(TEXT("param"), DiscoveryQuery.Param);
		DiscoveryQuery.bHasDomain = Params->TryGetStringField(TEXT("domain"), DiscoveryQuery.Domain);
		DiscoveryQuery.bHasFamily = Params->TryGetStringField(TEXT("family"), DiscoveryQuery.Family);
		Params->TryGetStringField(TEXT("query"), DiscoveryQuery.Query);
		if (Params->HasField(TEXT("limit")))
		{
			int32 L = 0;
			DiscoveryQuery.bHasLimit = Params->TryGetNumberField(TEXT("limit"), L);
			if (DiscoveryQuery.bHasLimit) DiscoveryQuery.Limit = FMath::Clamp(L, 1, McpDescribeMaxLimit);
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
		// describe takes a capability ID as execute does: it is the ID every search row and execute nextCall carries,
		// and it wins over tool and action (describeCapabilityReference in gateway-describe-capability.ts).
		FString Capability;
		FString ResolvedFromAlias;
		if (Params->TryGetStringField(TEXT("capability"), Capability))
		{
			const FMcpCanonicalRecordIndex& Index = FMcpCanonicalRecordIndex::Get();
			const FMcpCapabilityRecord* Record = Index.FindById(Capability);
			FString AliasTarget;
			if (!Record && Index.ResolveAlias(Capability, AliasTarget) == EMcpAliasResolution::Unique)
			{
				Record = Index.FindById(AliasTarget);
				ResolvedFromAlias = Capability;
			}
			if (!Record)
			{
				TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("UNKNOWN_CAPABILITY"),
					FString::Printf(TEXT("Unknown capability '%s'. Call search to find canonical capability IDs."), *Capability));
				const TArray<FString> Suggestions = GatewayClosestMatches(Capability, Index.GetCapabilityIds(), 3);
				Error->SetArrayField(TEXT("suggestions"), GatewayStringArray(Suggestions));
				const TSharedPtr<FJsonObject> Next = GatewayBuildNextCall(Suggestions.Num() > 0 ? TEXT("describe") : TEXT("search"),
					FString(), FString(), FString());
				Next->SetStringField(Suggestions.Num() > 0 ? TEXT("capability") : TEXT("query"), Suggestions.Num() > 0 ? Suggestions[0] : Capability);
				Error->SetObjectField(TEXT("nextCall"), Next);
				SendDiscoveryResult(Error);
				return;
			}
			DiscoveryQuery.Tool = Record->Parent;
			DiscoveryQuery.Action = McpCapabilityPublicAction(*Record);
			DiscoveryQuery.bHasAction = true;
		}
		// param selects one parameter of one capability (paramWithoutCapabilityError in gateway-describe.ts).
		if (DiscoveryQuery.bHasParam && !DiscoveryQuery.bHasAction)
		{
			TSharedPtr<FJsonObject> Error = GatewayError(TEXT("describe"), TEXT("MISSING_ACTION"),
				TEXT("param selects one parameter of one capability, so it needs a capability, or a tool with an action."));
			Error->SetObjectField(TEXT("nextCall"), DiscoveryQuery.Tool.IsEmpty()
				? GatewayBuildNextCall(TEXT("search"), FString(), FString(), FString())
				: GatewayBuildNextCall(TEXT("describe"), DiscoveryQuery.Tool, FString(), FString()));
			SendDiscoveryResult(Error);
			return;
		}
		// As on execute, the namespace of a capability ID (asset for asset.list) names its parent tool.
		const FString AskedTool = DiscoveryQuery.Tool;
		DiscoveryQuery.Tool = FMcpCanonicalRecordIndex::Get().ResolveToolNamespace(AskedTool);
		TSharedPtr<FJsonObject> Described = McpGatewayDescribeCapability(DiscoveryQuery, CapabilityStore, IsToolEnabled);
		bool bDescribed = false;
		if (Described.IsValid()) Described->TryGetBoolField(TEXT("success"), bDescribed);
		if (bDescribed && !DiscoveryQuery.bHasAction && DiscoveryQuery.Tool != AskedTool)
		{
			Described->SetStringField(TEXT("resolvedFromNamespace"), AskedTool);
		}
		if (bDescribed && !ResolvedFromAlias.IsEmpty()) Described->SetStringField(TEXT("resolvedFromAlias"), ResolvedFromAlias);
		SendDiscoveryResult(Described);
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

	UnknownOperation();
}

void FMcpNativeTransport::HandleGatewayModePreDispatch(
	const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
	const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
	const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken)
{
	// The public surface is permanently the single static 'unreal' gateway tool;
	// route it through the gateway's search/describe/execute/configure operations.
	if (ToolName == TEXT("unreal"))
	{
		HandleGatewayCall(Arguments, Id, ClientSocket, SessionId, CorsOrigin, ProgressToken);
		return;
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
	SendAndClose(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
}
