#include "Transport/Connection/McpConnectionManagerPrivate.h"

#include "Core/Security/McpPrequeueGate.h"
#include "Foundation/McpCapabilityPrincipal.h"
#include "Misc/App.h"

bool FMcpConnectionManager::AuthenticateSocketPrincipal(
    FMcpBridgeWebSocket* SocketPtr, const FString& PresentedToken, bool bLegacyTokenMatch)
{
	const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();

	FMcpPrincipalResolveRequest Request;
	Request.PresentedToken = PresentedToken;
	Request.bRequireToken = Settings->bRequireCapabilityToken;
	// The listen socket binds loopback-only unless bAllowNonLoopback is set (and
	// the bind gate then forces a token), so a no-token connection only ever
	// reaches here on loopback.
	Request.bIsLoopback = !Settings->bAllowNonLoopback;

	const FMcpCapabilityPrincipal Principal =
	    McpCapabilityPrincipal::Resolve(Request, *Settings);

	// A token that was PRESENTED but resolves to nothing is refused even when no
	// token is required. Admitting it produced an authenticated socket carrying
	// an unauthenticated, zero-scope principal, so every later request failed as
	// SCOPE_NOT_GRANTED — a token problem misreported as a scope problem.
	if ((Settings->bRequireCapabilityToken || !PresentedToken.IsEmpty()) &&
		!Principal.bAuthenticated)
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("Capability token mismatch."));
		return false;
	}

	// Enforced at the handshake so a principal that may not touch this project
	// never obtains an authenticated socket at all.
	const FMcpAuthorizationDecision ProjectDecision =
	    McpCapabilityAuthorization::CheckProject(Principal, FApp::GetProjectName());
	if (!ProjectDecision.bAllowed)
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("%s"), *ProjectDecision.Message);
		return false;
	}

	if (bLegacyTokenMatch && Principal.bDeprecated)
	{
		UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
			TEXT("bridge_hello authenticated with the deprecated all-or-nothing capability "
			     "token; migrate to a scoped token."));
	}

	if (SocketPtr)
	{
		FScopeLock Lock(&AuthSocketsMutex);
		SocketPrincipals.Add(SocketPtr, Principal);
	}
	return true;
}

FMcpCapabilityPrincipal FMcpConnectionManager::GetSocketPrincipal(FMcpBridgeWebSocket* SocketPtr)
{
	FScopeLock Lock(&AuthSocketsMutex);
	if (const FMcpCapabilityPrincipal* Found = SocketPrincipals.Find(SocketPtr))
	{
		return *Found;
	}
	return FMcpCapabilityPrincipal{};
}

void FMcpConnectionManager::ForgetSocketPrincipal(FMcpBridgeWebSocket* SocketPtr)
{
	FScopeLock Lock(&AuthSocketsMutex);
	SocketPrincipals.Remove(SocketPtr);
}

void FMcpConnectionManager::SendBridgeAck(
    TSharedPtr<FMcpBridgeWebSocket> Socket, FMcpBridgeWebSocket* SocketPtr)
{
	const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
	const FMcpCapabilityPrincipal Principal = GetSocketPrincipal(SocketPtr);

	const TSharedRef<FJsonObject> Ack = MakeShared<FJsonObject>();
	Ack->SetStringField(TEXT("type"), TEXT("bridge_ack"));
	Ack->SetStringField(TEXT("message"), TEXT("Automation bridge ready"));
	Ack->SetStringField(TEXT("serverName"), TEXT("UnrealEditor"));
	Ack->SetStringField(TEXT("serverVersion"), TEXT("unreal-engine"));

	if (ActiveSessionId.IsEmpty()) ActiveSessionId = FGuid::NewGuid().ToString();
	Ack->SetStringField(TEXT("sessionId"), ActiveSessionId);
	Ack->SetNumberField(TEXT("protocolVersion"), 1);

	TArray<TSharedPtr<FJsonValue>> SupportedOps;
	SupportedOps.Add(MakeShared<FJsonValueString>(TEXT("automation_request")));
	Ack->SetArrayField(TEXT("supportedOpcodes"), SupportedOps);

	TArray<TSharedPtr<FJsonValue>> ExpectedOps;
	ExpectedOps.Add(MakeShared<FJsonValueString>(TEXT("automation_response")));
	Ack->SetArrayField(TEXT("expectedResponseOpcodes"), ExpectedOps);

	TArray<TSharedPtr<FJsonValue>> Caps;
	Caps.Add(MakeShared<FJsonValueString>(TEXT("console_commands")));
	Caps.Add(MakeShared<FJsonValueString>(TEXT("native_plugin")));
	Ack->SetArrayField(TEXT("capabilities"), Caps);

	Ack->SetNumberField(TEXT("heartbeatIntervalMs"), 0);

	// Additive and secret-free: the resolved profile, its granted scopes, and
	// boolean flags only. The token, the path prefixes and the numeric limits are
	// never emitted.
	const TSharedRef<FJsonObject> Authority = MakeShared<FJsonObject>();
	Authority->SetStringField(TEXT("profile"), Principal.Identity);
	TArray<TSharedPtr<FJsonValue>> ScopeValues;
	for (const EMcpCapabilityScope Scope : Principal.Scopes)
	{
		ScopeValues.Add(MakeShared<FJsonValueString>(McpCapabilityPrincipal::ScopeToString(Scope)));
	}
	Authority->SetArrayField(TEXT("scopes"), ScopeValues);
	Authority->SetBoolField(TEXT("deprecated"), Principal.bDeprecated);
	Authority->SetBoolField(TEXT("tokenRequired"), Settings->bRequireCapabilityToken);
	Authority->SetBoolField(TEXT("pathRestricted"), Principal.IsPathRestricted());
	Authority->SetBoolField(TEXT("projectRestricted"), Principal.IsProjectRestricted());
	Ack->SetObjectField(TEXT("authority"), Authority);

	FString Serialized;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	FJsonSerializer::Serialize(Ack, Writer);
	if (Socket.IsValid())
	{
		Socket->Send(Serialized);
	}
}

bool FMcpConnectionManager::AuthorizeAutomationRequest(
    TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& RootObj)
{
	if (!RootObj.IsValid())
	{
		return false;
	}

	FString RequestId;
	FString Action;
	RootObj->TryGetStringField(TEXT("requestId"), RequestId);
	RootObj->TryGetStringField(TEXT("action"), Action);

	const FMcpCapabilityPrincipal Principal = GetSocketPrincipal(Socket.Get());

	FMcpPrequeueRequest Request;
	Request.Principal = &Principal;
	Request.DispatchAction = Action;

	const TSharedPtr<FJsonObject>* PayloadField = nullptr;
	if (RootObj->TryGetObjectField(TEXT("payload"), PayloadField) && PayloadField)
	{
		Request.Payload = *PayloadField;
	}

	// THE GATE AND THE DISPATCHER MUST RESOLVE THE SAME ACTION, and on this
	// transport nothing between them reconciles the two fields they read. The gate
	// resolves the sub-action through McpHandlerUtils::NormalizeAction
	// (`payload.subAction`, else the envelope action). The domain dispatchers were
	// split on which field they read — some took `payload.action`, some
	// `payload.subAction` — so a payload carrying BOTH with different values once
	// authorized one capability and executed another: `subAction:"screenshot"`
	// (read) could buy `action:"execute_python"` (write, in-process code
	// execution). This path applies no schema validation and no post-queue
	// re-authorization, so the split is reconciled HERE, before the gate resolves
	// a demand.
	//
	// It is NORMALIZED, not refused, because the split is also how legitimate
	// alias traffic arrives: the gateway dispatches the canonical action in
	// `action` and the handler rewrites `subAction` to the native name
	// (`add_socket` -> `create_socket`, `add_niagara_module` -> `add_module`,
	// `add_material_node` -> `add_node`), so refusing any disagreement would break
	// 11+ shipped capabilities on this transport. The authoritative field is the
	// one NormalizeAction and every dispatcher now read FIRST: `subAction`. When
	// they disagree, `action` is OVERWRITTEN from `subAction`, so the decoy is
	// destroyed rather than trusted — the gate and the dispatcher then resolve the
	// same string by construction, and a client can never raise what runs past
	// what was authorized.
	//
	// LOAD-BEARING ALIASING: Request.Payload is a TSharedPtr<FJsonObject> that
	// aliases the same object inside RootObj (extracted via TryGetObjectField,
	// which hands back the stored pointer, not a copy). SetStringField below
	// therefore mutates the queued request's payload in place — the dispatcher
	// sees the normalized fields by construction. Do NOT insert a deep copy of
	// Request.Payload between this point and QueueAutomationRequest: doing so
	// would silently re-open the bypass (the copy would carry the original,
	// un-normalized action/subAction, while the gate authorized the in-place
	// normalized values). No test protects this invariant; the comment is the
	// guard.
	if (Request.Payload.IsValid())
	{
		FString PayloadAction;
		FString PayloadSubAction;
		const bool bHasAction =
			Request.Payload->TryGetStringField(TEXT("action"), PayloadAction) && !PayloadAction.IsEmpty();
		const bool bHasSubAction =
			Request.Payload->TryGetStringField(TEXT("subAction"), PayloadSubAction) && !PayloadSubAction.IsEmpty();
		if (bHasSubAction &&
			(!bHasAction || !PayloadAction.Equals(PayloadSubAction, ESearchCase::IgnoreCase)))
		{
			Request.Payload->SetStringField(TEXT("action"), PayloadSubAction);
			UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
				TEXT("Normalized automation request payload.action from the authoritative subAction."));
		}
		else if (bHasAction && !bHasSubAction)
		{
			// Single-field payload: make the two fields agree so a dispatcher
			// reading `subAction` resolves the same action the gate does.
			Request.Payload->SetStringField(TEXT("subAction"), PayloadAction);
		}
	}

	// Consent is an envelope sibling, never a handler param, and is revalidated
	// here rather than trusted from the TypeScript layer.
	const TSharedPtr<FJsonObject>* ConsentField = nullptr;
	if (RootObj->TryGetObjectField(TEXT("consent"), ConsentField) && ConsentField)
	{
		Request.Consent = *ConsentField;
	}

	const FMcpAuthorizationDecision Decision = McpPrequeueGate::Authorize(Request);
	if (Decision.bAllowed)
	{
		return true;
	}

	// Only the stable code reaches the log; no request-supplied text is echoed.
	UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
		TEXT("Refused automation request before dispatch (%s)."), *Decision.ErrorCode);
	SendAutomationResponse(Socket, RequestId, false, Decision.Message, nullptr, Decision.ErrorCode);
	return false;
}
