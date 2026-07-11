#include "MCP/Transport/McpNativeTransportPrivate.h"

bool FMcpNativeTransport::NegotiateInitializeProtocolVersion(
	const TSharedPtr<FJsonObject>& Params, FString& OutNegotiated, FString& OutError)
{
	FString Requested;
	if (!Params.IsValid() || !Params->TryGetStringField(TEXT("protocolVersion"), Requested))
	{
		OutError = TEXT("initialize requires a string 'protocolVersion' field");
		OutNegotiated.Reset();
		return false;
	}
	if (Requested.IsEmpty())
	{
		OutError = TEXT("initialize 'protocolVersion' must be a non-empty string");
		OutNegotiated.Reset();
		return false;
	}
	// Echo a supported version; negotiate down to the latest supported one
	// for any well-formed (non-empty string) request version.
	if (McpIsSupportedProtocolVersion(Requested))
	{
		OutNegotiated = Requested;
	}
	else
	{
		OutNegotiated = McpLatestProtocolVersion();
	}
	OutError.Reset();
	return true;
}

bool FMcpNativeTransport::ResolveRequestProtocolVersion(
	const FString& HeaderValue, const FString& SessionId,
	FString& OutVersion, FString& OutError)
{
	if (!HeaderValue.IsEmpty())
	{
		if (McpIsSupportedProtocolVersion(HeaderValue))
		{
			OutVersion = HeaderValue;
			OutError.Reset();
			return true;
		}
		OutVersion.Reset();
		OutError = FString::Printf(
			TEXT("Unsupported or invalid MCP-Protocol-Version: %s"), *HeaderValue);
		return false;
	}
	// Absent header: derive from the negotiated session version if known,
	// otherwise assume the default version.
	FScopeLock Lock(&SessionMutex);
	if (const FString* Negotiated = SessionProtocolVersions.Find(SessionId))
	{
		OutVersion = *Negotiated;
	}
	else
	{
		OutVersion = McpDefaultProtocolVersion();
	}
	OutError.Reset();
	return true;
}

bool FMcpNativeTransport::GuardProtocolVersionHeader(
	FSocket* ClientSocket, const FParsedHttpRequest& Req,
	const TSharedPtr<FJsonValue>& Id, bool bJsonBody)
{
	FString Version, Error;
	if (ResolveRequestProtocolVersion(Req.ProtocolVersion, Req.SessionId, Version, Error))
	{
		return true;
	}
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (bJsonBody)
	{
		SendHttpResponse(ClientSocket, 400, TEXT("application/json"),
			FMcpJsonRpc::BuildError(
				Id.IsValid() ? Id : MakeShared<FJsonValueNull>(),
				FMcpJsonRpc::ErrorInvalidRequest, Error),
			{}, Req.Origin);
	}
	else
	{
		SendHttpResponse(ClientSocket, 400, TEXT("text/plain"), Error, {}, Req.Origin);
	}
	ClientSocket->Close();
	if (SocketSub) SocketSub->DestroySocket(ClientSocket);
	return false;
}
