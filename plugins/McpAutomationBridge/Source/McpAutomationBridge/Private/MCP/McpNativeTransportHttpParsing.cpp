#include "MCP/McpNativeTransportPrivate.h"

bool FMcpNativeTransport::ReadHttpRequest(FSocket* Socket, FParsedHttpRequest& OutRequest)
{
	// Read headers (up to 8KB)
	static constexpr int32 MaxHeaderSize = 8192;
	TArray<uint8> HeaderBuf;
	HeaderBuf.Reserve(MaxHeaderSize);

	const double Deadline = FPlatformTime::Seconds() + 5.0;  // 5s read timeout

	while (HeaderBuf.Num() < MaxHeaderSize)
	{
		if (FPlatformTime::Seconds() > Deadline)
		{
			UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP header read timeout"));
			return false;
		}

		uint32 PendingSize = 0;
		if (!Socket->HasPendingData(PendingSize))
		{
			FPlatformProcess::Sleep(0.001f);
			continue;
		}

		uint8 Byte;
		int32 BytesRead = 0;
		if (!Socket->Recv(&Byte, 1, BytesRead) || BytesRead <= 0)
		{
			FPlatformProcess::Sleep(0.001f);
			continue;
		}

		HeaderBuf.Add(Byte);

		// Check for \r\n\r\n
		const int32 Len = HeaderBuf.Num();
		if (Len >= 4
			&& HeaderBuf[Len - 4] == '\r'
			&& HeaderBuf[Len - 3] == '\n'
			&& HeaderBuf[Len - 2] == '\r'
			&& HeaderBuf[Len - 1] == '\n')
		{
			break;
		}
	}

	if (HeaderBuf.Num() >= MaxHeaderSize)
	{
		UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP headers too large"));
		return false;
	}

	// Parse headers
	FUTF8ToTCHAR HeaderConverter(reinterpret_cast<const ANSICHAR*>(HeaderBuf.GetData()), HeaderBuf.Num());
	FString HeaderStr(HeaderConverter.Length(), HeaderConverter.Get());

	TArray<FString> Lines;
	HeaderStr.ParseIntoArray(Lines, TEXT("\r\n"));
	if (Lines.Num() == 0)
	{
		return false;
	}

	// Request line: "POST /mcp HTTP/1.1"
	TArray<FString> RequestParts;
	Lines[0].ParseIntoArrayWS(RequestParts);
	if (RequestParts.Num() < 2)
	{
		return false;
	}
	OutRequest.Method = RequestParts[0];
	OutRequest.Path = RequestParts[1];

	// Parse headers
	OutRequest.ContentLength = 0;
	bool bHasContentLength = false;
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		FString Key, Value;
		if (Lines[i].Split(TEXT(":"), &Key, &Value))
		{
			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();

			if (Key.Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase))
			{
				if (bHasContentLength)
				{
					UE_LOG(LogMcpNativeTransport, Warning, TEXT("Duplicate Content-Length header"));
					return false;
				}

				int32 ParsedContentLength = 0;
				if (!LexTryParseString(ParsedContentLength, *Value))
				{
					UE_LOG(LogMcpNativeTransport, Warning,
						TEXT("Invalid Content-Length header: %s"), *Value);
					return false;
				}

				OutRequest.ContentLength = ParsedContentLength;
				bHasContentLength = true;
			}
			else if (Key.Equals(TEXT("Mcp-Session-Id"), ESearchCase::IgnoreCase))
			{
				OutRequest.SessionId = Value;
			}
			else if (Key.Equals(TEXT("Accept"), ESearchCase::IgnoreCase))
			{
				OutRequest.Accept = Value;
			}
			else if (Key.Equals(TEXT("X-MCP-Capability-Token"), ESearchCase::IgnoreCase))
			{
				OutRequest.CapabilityToken = Value;
			}
			else if (Key.Equals(TEXT("Origin"), ESearchCase::IgnoreCase))
			{
				OutRequest.Origin = Value;
			}
		}
	}

	// Read body
	static constexpr int32 MaxBodySize = 5 * 1024 * 1024;  // 5MB
	if (OutRequest.ContentLength < 0 || OutRequest.ContentLength > MaxBodySize)
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("Invalid HTTP body size: %d bytes"), OutRequest.ContentLength);
		return false;
	}

	if (OutRequest.ContentLength > 0)
	{
		TArray<uint8> BodyBuf;
		BodyBuf.SetNumUninitialized(OutRequest.ContentLength);
		int32 TotalRead = 0;
		uint32 PendingData = 0;

		while (TotalRead < OutRequest.ContentLength)
		{
			if (FPlatformTime::Seconds() > Deadline)
			{
				UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP body read timeout"));
				return false;
			}

			int32 BytesRead = 0;
			if (Socket->Recv(BodyBuf.GetData() + TotalRead,
				OutRequest.ContentLength - TotalRead, BytesRead))
			{
				if (BytesRead > 0)
				{
					TotalRead += BytesRead;
				}
				else if (!Socket->HasPendingData(PendingData))
				{
					UE_LOG(LogMcpNativeTransport, Warning, TEXT("HTTP body read: peer closed connection (read %d/%d)"), TotalRead, OutRequest.ContentLength);
					return false;
				}
				else
				{
					FPlatformProcess::Sleep(0.001f);
				}
			}
			else
			{
				FPlatformProcess::Sleep(0.001f);
			}
		}

		FUTF8ToTCHAR BodyConverter(reinterpret_cast<const ANSICHAR*>(BodyBuf.GetData()), TotalRead);
		OutRequest.Body = FString(BodyConverter.Length(), BodyConverter.Get());
	}

	return true;
}

// ─── HTTP Response Helpers ──────────────────────────────────────────────────
