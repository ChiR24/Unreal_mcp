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

bool FMcpNativeTransport::GuardProtocolVersionHeader(
	FSocket* ClientSocket, const FParsedHttpRequest& Req,
	const TSharedPtr<FJsonValue>& Id, bool bJsonBody)
{
	// An absent header is accepted (the session's negotiated version applies);
	// only a present-but-unsupported one is refused.
	if (Req.ProtocolVersion.IsEmpty() || McpIsSupportedProtocolVersion(Req.ProtocolVersion))
	{
		return true;
	}
	const FString Error = FString::Printf(
		TEXT("Unsupported or invalid MCP-Protocol-Version: %s"), *Req.ProtocolVersion);
	if (bJsonBody)
	{
		SendAndClose(ClientSocket, 400, TEXT("application/json"),
			FMcpJsonRpc::BuildError(
				Id.IsValid() ? Id : MakeShared<FJsonValueNull>(),
				FMcpJsonRpc::ErrorInvalidRequest, Error),
			{}, Req.Origin);
	}
	else
	{
		SendAndClose(ClientSocket, 400, TEXT("text/plain"), Error, {}, Req.Origin);
	}
	return false;
}
