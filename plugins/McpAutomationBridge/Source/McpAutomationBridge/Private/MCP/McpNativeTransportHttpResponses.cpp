#include "MCP/McpNativeTransportPrivate.h"

bool FMcpNativeTransport::SendHttpResponse(FSocket* Socket, int32 StatusCode,
	const FString& ContentType, const FString& Body,
	const TMap<FString, FString>& ExtraHeaders,
	const FString& CorsOrigin)
{
	FString StatusText;
	switch (StatusCode)
	{
	case 200: StatusText = TEXT("OK"); break;
	case 202: StatusText = TEXT("Accepted"); break;
	case 204: StatusText = TEXT("No Content"); break;
	case 400: StatusText = TEXT("Bad Request"); break;
	case 403: StatusText = TEXT("Forbidden"); break;
	case 404: StatusText = TEXT("Not Found"); break;
	case 405: StatusText = TEXT("Method Not Allowed"); break;
	case 401: StatusText = TEXT("Unauthorized"); break;
	case 406: StatusText = TEXT("Not Acceptable"); break;
	case 429: StatusText = TEXT("Too Many Requests"); break;
	case 500: StatusText = TEXT("Internal Server Error"); break;
	case 503: StatusText = TEXT("Service Unavailable"); break;
	default:  StatusText = TEXT("Unknown"); break;
	}

	FTCHARToUTF8 BodyUtf8(*Body);
	const int32 BodyLength = BodyUtf8.Length();

	FString Response = FString::Printf(
		TEXT("HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nConnection: close\r\n"),
		StatusCode, *StatusText, *ContentType, BodyLength);
	AppendCorsHeaders(Response, CorsOrigin);

	for (const auto& [Key, Value] : ExtraHeaders)
	{
		Response += FString::Printf(TEXT("%s: %s\r\n"), *Key, *Value);
	}
	Response += TEXT("\r\n");

	// Send header + body
	FTCHARToUTF8 HeaderUtf8(*Response);
	if (!SendAllBytes(Socket, reinterpret_cast<const uint8*>(HeaderUtf8.Get()),
		HeaderUtf8.Length()))
	{
		return false;
	}

	if (BodyLength > 0)
	{
		if (!SendAllBytes(Socket, reinterpret_cast<const uint8*>(BodyUtf8.Get()),
			BodyLength))
		{
			return false;
		}
	}

	return true;
}

bool FMcpNativeTransport::SendSSEHeaders(FSocket* Socket, const FString& SessionId,
	const FString& CorsOrigin)
{
	FString Headers = FString::Printf(
		TEXT("HTTP/1.1 200 OK\r\n")
		TEXT("Content-Type: text/event-stream\r\n")
		TEXT("Cache-Control: no-cache\r\n")
		TEXT("Connection: keep-alive\r\n")
		TEXT("Mcp-Session-Id: %s\r\n"),
		*SessionId);
	AppendCorsHeaders(Headers, CorsOrigin);
	Headers += TEXT("\r\n");

	FTCHARToUTF8 Utf8(*Headers);
	return SendAllBytes(Socket, reinterpret_cast<const uint8*>(Utf8.Get()),
		Utf8.Length());
}

bool FMcpNativeTransport::IsCorsEnabled() const
{
	const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
	return (ListenHost == TEXT("127.0.0.1") || ListenHost == TEXT("::1"))
		&& Settings
		&& Settings->bRequireCapabilityToken
		&& !Settings->CapabilityToken.IsEmpty();
}

bool FMcpNativeTransport::IsAllowedCorsOrigin(const FString& Origin) const
{
	if (!IsCorsEnabled())
	{
		return false;
	}

	const FString TrimmedOrigin = Origin.TrimStartAndEnd();
	if (TrimmedOrigin.IsEmpty() || TrimmedOrigin.Equals(TEXT("null"), ESearchCase::IgnoreCase) ||
		TrimmedOrigin.Contains(TEXT("\r")) || TrimmedOrigin.Contains(TEXT("\n")))
	{
		return false;
	}

	FString Scheme;
	FString Remainder;
	if (!TrimmedOrigin.Split(TEXT("://"), &Scheme, &Remainder))
	{
		return false;
	}
	if (!Scheme.Equals(TEXT("http"), ESearchCase::IgnoreCase) &&
		!Scheme.Equals(TEXT("https"), ESearchCase::IgnoreCase))
	{
		return false;
	}
	if (Remainder.IsEmpty() || Remainder.Contains(TEXT("/")))
	{
		return false;
	}

	auto IsDigitsOnly = [](const FString& Value) -> bool
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : Value)
		{
			if (Character < '0' || Character > '9')
			{
				return false;
			}
		}
		return true;
	};
	auto IsValidPort = [&IsDigitsOnly](const FString& PortText) -> bool
	{
		if (!IsDigitsOnly(PortText))
		{
			return false;
		}

		int32 PortNumber = 0;
		if (!LexTryParseString(PortNumber, *PortText))
		{
			return false;
		}
		return PortNumber > 0 && PortNumber <= 65535;
	};

	FString Host = Remainder;
	if (Host.StartsWith(TEXT("[")))
	{
		int32 EndBracket = INDEX_NONE;
		if (!Host.FindChar(TEXT(']'), EndBracket) || EndBracket <= 1)
		{
			return false;
		}

		const FString PortSuffix = Host.Mid(EndBracket + 1);
		if (!PortSuffix.IsEmpty())
		{
			if (!PortSuffix.StartsWith(TEXT(":")) || !IsValidPort(PortSuffix.Mid(1)))
			{
				return false;
			}
		}

		const FString BracketedHost = Host.Mid(1, EndBracket - 1);
		if (BracketedHost != TEXT("::1"))
		{
			return false;
		}
		Host = BracketedHost;
	}
	else
	{
		FString HostOnly;
		FString Port;
		if (Host.Split(TEXT(":"), &HostOnly, &Port))
		{
			if (!IsValidPort(Port))
			{
				return false;
			}
			Host = HostOnly;
		}
	}

	return Host.Equals(TEXT("localhost"), ESearchCase::IgnoreCase) ||
		Host == TEXT("127.0.0.1") ||
		Host == TEXT("::1");
}

void FMcpNativeTransport::AppendCorsHeaders(FString& Response, const FString& Origin) const
{
	if (!IsAllowedCorsOrigin(Origin))
	{
		return;
	}

	Response += FString::Printf(TEXT("Access-Control-Allow-Origin: %s\r\n"), *Origin.TrimStartAndEnd());
	Response += TEXT("Vary: Origin\r\n");
	Response += TEXT("Access-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n");
	Response += TEXT("Access-Control-Allow-Headers: Content-Type, Accept, Mcp-Session-Id, MCP-Protocol-Version, X-MCP-Capability-Token\r\n");
	Response += TEXT("Access-Control-Expose-Headers: Mcp-Session-Id, MCP-Protocol-Version\r\n");
}

bool FMcpNativeTransport::WriteSSEEvent(FSSEConnection& Conn, const FString& EventData)
{
	FString Frame = FString::Printf(
		TEXT("event: message\ndata: %s\n\n"), *EventData);

	FTCHARToUTF8 Utf8(*Frame);

	FScopeLock Lock(&Conn.WriteMutex);
	if (!Conn.Socket)
	{
		return false;
	}
	return SendAllBytes(Conn.Socket, reinterpret_cast<const uint8*>(Utf8.Get()),
		Utf8.Length());
}

// ─── Persistent Notification Streams (GET /mcp) ────────────────────────────
