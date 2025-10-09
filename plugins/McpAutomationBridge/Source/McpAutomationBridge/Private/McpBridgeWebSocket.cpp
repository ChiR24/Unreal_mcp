#include "McpBridgeWebSocket.h"

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/RunnableThread.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "Misc/ScopeLock.h"
#include "Misc/StringBuilder.h"
#include "Misc/Timespan.h"
#include "String/LexFromString.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace
{
constexpr const TCHAR* WebSocketGuid = TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
constexpr uint8 OpCodeContinuation = 0x0;
constexpr uint8 OpCodeText = 0x1;
constexpr uint8 OpCodeBinary = 0x2;
constexpr uint8 OpCodeClose = 0x8;
constexpr uint8 OpCodePing = 0x9;
constexpr uint8 OpCodePong = 0xA;

struct FParsedWebSocketUrl
{
    FString Host;
    int32 Port = 80;
    FString PathWithQuery;
};

bool ParseWebSocketUrl(const FString& InUrl, FParsedWebSocketUrl& OutParsed, FString& OutError)
{
    const FString Trimmed = InUrl.TrimStartAndEnd();
    if (Trimmed.IsEmpty())
    {
        OutError = TEXT("WebSocket URL is empty.");
        return false;
    }

    static const FString SchemePrefix(TEXT("ws://"));
    if (!Trimmed.StartsWith(SchemePrefix, ESearchCase::IgnoreCase))
    {
        OutError = TEXT("Only ws:// scheme is supported.");
        return false;
    }

    const FString Remainder = Trimmed.Mid(SchemePrefix.Len());
    FString HostPort;
    FString PathRemainder;
    if (!Remainder.Split(TEXT("/"), &HostPort, &PathRemainder, ESearchCase::CaseSensitive, ESearchDir::FromStart))
    {
        HostPort = Remainder;
        PathRemainder.Reset();
    }

    HostPort = HostPort.TrimStartAndEnd();
    if (HostPort.IsEmpty())
    {
        OutError = TEXT("WebSocket URL missing host.");
        return false;
    }

    FString Host;
    int32 Port = 80;

    if (HostPort.StartsWith(TEXT("[")))
    {
        int32 ClosingBracketIndex = INDEX_NONE;
        if (!HostPort.FindChar(TEXT(']'), ClosingBracketIndex))
        {
            OutError = TEXT("Invalid IPv6 WebSocket host.");
            return false;
        }

        Host = HostPort.Mid(1, ClosingBracketIndex - 1);
        const int32 PortSeparatorIndex = HostPort.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        if (PortSeparatorIndex > ClosingBracketIndex)
        {
            const FString PortString = HostPort.Mid(PortSeparatorIndex + 1);
            if (!PortString.IsEmpty() && !LexTryParseString(Port, *PortString))
            {
                OutError = TEXT("Invalid WebSocket port.");
                return false;
            }
        }
    }
    else
    {
        FString PortString;
        if (HostPort.Split(TEXT(":"), &Host, &PortString, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
        {
            Host = Host.TrimStartAndEnd();
            PortString = PortString.TrimStartAndEnd();

            if (!PortString.IsEmpty())
            {
                if (!LexTryParseString(Port, *PortString))
                {
                    OutError = TEXT("Invalid WebSocket port.");
                    return false;
                }
            }
        }
        else
        {
            Host = HostPort;
        }
    }

    Host = Host.TrimStartAndEnd();
    if (Host.IsEmpty())
    {
        OutError = TEXT("WebSocket URL missing host.");
        return false;
    }

    if (Port <= 0)
    {
        OutError = TEXT("WebSocket port must be positive.");
        return false;
    }

    FString PathWithQuery;
    if (PathRemainder.IsEmpty())
    {
        PathWithQuery = TEXT("/");
    }
    else
    {
        PathWithQuery = TEXT("/") + PathRemainder;
    }

    OutParsed.Host = Host;
    OutParsed.Port = Port;
    OutParsed.PathWithQuery = PathWithQuery;
    return true;
}

uint16 ToNetwork16(uint16 Value)
{
#if PLATFORM_LITTLE_ENDIAN
    return static_cast<uint16>(((Value & 0x00FF) << 8) | ((Value & 0xFF00) >> 8));
#else
    return Value;
#endif
}

uint16 FromNetwork16(uint16 Value)
{
    return ToNetwork16(Value);
}

uint64 ToNetwork64(uint64 Value)
{
#if PLATFORM_LITTLE_ENDIAN
    return ((Value & 0x00000000000000FFULL) << 56) |
           ((Value & 0x000000000000FF00ULL) << 40) |
           ((Value & 0x0000000000FF0000ULL) << 24) |
           ((Value & 0x00000000FF000000ULL) << 8) |
           ((Value & 0x000000FF00000000ULL) >> 8) |
           ((Value & 0x0000FF0000000000ULL) >> 24) |
           ((Value & 0x00FF000000000000ULL) >> 40) |
           ((Value & 0xFF00000000000000ULL) >> 56);
#else
    return Value;
#endif
}

uint64 FromNetwork64(uint64 Value)
{
    return ToNetwork64(Value);
}

FString BytesToStringView(const TArray<uint8>& Data)
{
    if (Data.Num() == 0)
    {
        return FString();
    }
    return FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Data.GetData())));
}

void DispatchOnGameThread(TFunction<void()>&& Fn)
{
    if (IsInGameThread())
    {
        Fn();
        return;
    }

    AsyncTask(ENamedThreads::GameThread, MoveTemp(Fn));
}
}

FMcpBridgeWebSocket::FMcpBridgeWebSocket(const FString& InUrl, const FString& InProtocols, const TMap<FString, FString>& InHeaders)
    : Url(InUrl)
    , Protocols(InProtocols)
    , Headers(InHeaders)
    , bServerMode(false)
{
}

FMcpBridgeWebSocket::FMcpBridgeWebSocket(int32 InPort)
    : Port(InPort)
    , Protocols({ TEXT("mcp-automation") })
    , bServerMode(true)
{
}

FMcpBridgeWebSocket::FMcpBridgeWebSocket(FSocket* InClientSocket)
    : Socket(InClientSocket)
    , Port(0)
    , bServerMode(false)
    , bServerAcceptedConnection(true)
{
    bConnected = true;
}

FMcpBridgeWebSocket::~FMcpBridgeWebSocket()
{
    Close();
    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }
    if (StopEvent)
    {
        FPlatformProcess::ReturnSynchEventToPool(StopEvent);
        StopEvent = nullptr;
    }
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }
}

void FMcpBridgeWebSocket::InitializeWeakSelf(const TSharedPtr<FMcpBridgeWebSocket>& InShared)
{
    SelfWeakPtr = InShared;
}

void FMcpBridgeWebSocket::Connect()
{
    if (Thread)
    {
        return;
    }

    bStopping = false;
    StopEvent = FPlatformProcess::GetSynchEventFromPool(true);
    Thread = FRunnableThread::Create(this, TEXT("FMcpBridgeWebSocketWorker"), 0, TPri_Normal);
    if (!Thread)
    {
        DispatchOnGameThread([WeakThis = SelfWeakPtr]
        {
            if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
            {
                Pinned->ConnectionErrorDelegate.Broadcast(TEXT("Failed to create WebSocket worker thread."));
            }
        });
    }
}

void FMcpBridgeWebSocket::Listen()
{
    if (Thread || !bServerMode)
    {
        return;
    }

    bStopping = false;
    StopEvent = FPlatformProcess::GetSynchEventFromPool(true);
    Thread = FRunnableThread::Create(this, TEXT("FMcpBridgeWebSocketServerWorker"), 0, TPri_Normal);
    if (!Thread)
    {
        DispatchOnGameThread([WeakThis = SelfWeakPtr]
        {
            if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
            {
                Pinned->ConnectionErrorDelegate.Broadcast(TEXT("Failed to create WebSocket server worker thread."));
            }
        });
    }
}

void FMcpBridgeWebSocket::Close(int32 StatusCode, const FString& Reason)
{
    bStopping = true;
    if (StopEvent)
    {
        StopEvent->Trigger();
    }

    if (Socket && bConnected)
    {
        SendCloseFrame(StatusCode, Reason);
    }

    if (Socket)
    {
        Socket->Close();
    }
}

bool FMcpBridgeWebSocket::Send(const FString& Data)
{
    FTCHARToUTF8 Converter(*Data);
    return Send(Converter.Get(), Converter.Length());
}

bool FMcpBridgeWebSocket::Send(const void* Data, SIZE_T Length)
{
    if (!IsConnected() || !Socket)
    {
        return false;
    }

    return SendTextFrame(Data, Length);
}

bool FMcpBridgeWebSocket::IsConnected() const
{
    return bConnected;
}

bool FMcpBridgeWebSocket::IsListening() const
{
    return bListening;
}

void FMcpBridgeWebSocket::SendHeartbeatPing()
{
    SendControlFrame(OpCodePing, TArray<uint8>());
}

bool FMcpBridgeWebSocket::Init()
{
    return true;
}

uint32 FMcpBridgeWebSocket::Run()
{
    if (bServerMode)
    {
        return RunServer();
    }
    else
    {
        return RunClient();
    }
}

uint32 FMcpBridgeWebSocket::RunClient()
{
    if (bServerAcceptedConnection)
    {
        if (!PerformServerHandshake())
        {
            return 0;
        }
    }
    else
    {
        if (!PerformHandshake())
        {
            return 0;
        }
    }

    bConnected = true;
    DispatchOnGameThread([WeakThis = SelfWeakPtr]
    {
        if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
        {
            Pinned->ConnectedDelegate.Broadcast(Pinned);
        }
    });

    while (!bStopping)
    {
        if (!ReceiveFrame())
        {
            break;
        }
    }

    TearDown(TEXT("Socket loop finished."), true, 1000);
    return 0;
}

uint32 FMcpBridgeWebSocket::RunServer()
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    ListenSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("McpAutomationBridgeListenSocket"), false);
    if (!ListenSocket)
    {
        DispatchOnGameThread([WeakThis = SelfWeakPtr]
        {
            if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
            {
                Pinned->ConnectionErrorDelegate.Broadcast(TEXT("Failed to create listen socket."));
            }
        });
        return 0;
    }

    ListenSocket->SetReuseAddr(true);
    ListenSocket->SetNonBlocking(false);

    TSharedRef<FInternetAddr> ListenAddr = SocketSubsystem->CreateInternetAddr();
    ListenAddr->SetAnyAddress();
    ListenAddr->SetPort(Port);

    if (!ListenSocket->Bind(*ListenAddr))
    {
        DispatchOnGameThread([WeakThis = SelfWeakPtr]
        {
            if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
            {
                Pinned->ConnectionErrorDelegate.Broadcast(TEXT("Failed to bind listen socket."));
            }
        });
        return 0;
    }

    if (!ListenSocket->Listen(10))
    {
        DispatchOnGameThread([WeakThis = SelfWeakPtr]
        {
            if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
            {
                Pinned->ConnectionErrorDelegate.Broadcast(TEXT("Failed to listen on socket."));
            }
        });
        return 0;
    }

    bListening = true;
    DispatchOnGameThread([WeakThis = SelfWeakPtr]
    {
        if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
        {
            Pinned->ConnectedDelegate.Broadcast(Pinned); // Server ready event
        }
    });

    while (!bStopping)
    {
        // Accept incoming connections
        FSocket* ClientSocket = ListenSocket->Accept(TEXT("McpAutomationBridgeClient"));
        if (ClientSocket)
        {
            // Create a new WebSocket instance for this client connection
            auto ClientWebSocket = MakeShared<FMcpBridgeWebSocket>(ClientSocket);
            ClientWebSocket->InitializeWeakSelf(ClientWebSocket);
            ClientWebSocket->bServerMode = false; // Client connections are not in server mode
            ClientWebSocket->bServerAcceptedConnection = true; // This is a server-accepted connection

            // Start the client WebSocket thread to handle the handshake and communication
            ClientWebSocket->Connect();

            // Notify that a new client has connected
            DispatchOnGameThread([WeakThis = SelfWeakPtr, ClientWebSocket]
            {
                if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
                {
                    Pinned->ClientConnectedDelegate.Broadcast(ClientWebSocket);
                }
            });
        }
        else
        {
            // Sleep briefly to avoid busy waiting
            FPlatformProcess::Sleep(0.01f);
        }
    }

    if (ListenSocket)
    {
        ListenSocket->Close();
        SocketSubsystem->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }

    return 0;
}

void FMcpBridgeWebSocket::Stop()
{
    bStopping = true;
    if (StopEvent)
    {
        StopEvent->Trigger();
    }
}

void FMcpBridgeWebSocket::TearDown(const FString& Reason, bool bWasClean, int32 StatusCode)
{
    if (Socket)
    {
        Socket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
        Socket = nullptr;
    }

    const bool bWasConnected = bConnected;
    bConnected = false;
    ResetFragmentState();

    DispatchOnGameThread([WeakThis = SelfWeakPtr, Reason, bWasClean, StatusCode, bWasConnected]
    {
        if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
        {
            if (!bWasConnected)
            {
                Pinned->ConnectionErrorDelegate.Broadcast(Reason);
            }
            Pinned->ClosedDelegate.Broadcast(Pinned, StatusCode, Reason, bWasClean);
        }
    });
}

bool FMcpBridgeWebSocket::PerformHandshake()
{
    FParsedWebSocketUrl ParsedUrl;
    FString ParseError;
    if (!ParseWebSocketUrl(Url, ParsedUrl, ParseError))
    {
        TearDown(ParseError, false, 4000);
        return false;
    }

    HostHeader = ParsedUrl.Host;
    Port = ParsedUrl.Port;
    HandshakePath = ParsedUrl.PathWithQuery;

    TSharedPtr<FInternetAddr> Endpoint;
    if (!ResolveEndpoint(Endpoint) || !Endpoint.IsValid())
    {
        TearDown(TEXT("Unable to resolve WebSocket host."), false, 4000);
        return false;
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("McpAutomationBridgeSocket"), false);
    if (!Socket)
    {
        TearDown(TEXT("Failed to create socket."), false, 4000);
        return false;
    }
    Socket->SetReuseAddr(true);
    Socket->SetNonBlocking(false);
    Socket->SetNoDelay(true);

    Endpoint->SetPort(Port);
    if (!Socket->Connect(*Endpoint))
    {
        TearDown(TEXT("Unable to connect to WebSocket endpoint."), false, 4000);
        return false;
    }

    TArray<uint8> KeyBytes;
    KeyBytes.SetNumUninitialized(16);
    for (uint8& Byte : KeyBytes)
    {
        Byte = static_cast<uint8>(FMath::RandRange(0, 255));
    }
    HandshakeKey = FBase64::Encode(KeyBytes.GetData(), KeyBytes.Num());

    FString HostLine = HostHeader;
    const bool bIsIpv6Host = HostLine.Contains(TEXT(":"));
    if (bIsIpv6Host && !HostLine.StartsWith(TEXT("[")))
    {
        HostLine = FString::Printf(TEXT("[%s]"), *HostLine);
    }
    if (!(Port == 80 || Port == 0))
    {
        HostLine += FString::Printf(TEXT(":%d"), Port);
    }

    TStringBuilder<512> RequestBuilder;
    RequestBuilder << TEXT("GET ") << HandshakePath << TEXT(" HTTP/1.1\r\n");
    RequestBuilder << TEXT("Host: ") << HostLine << TEXT("\r\n");
    RequestBuilder << TEXT("Upgrade: websocket\r\n");
    RequestBuilder << TEXT("Connection: Upgrade\r\n");
    RequestBuilder << TEXT("Sec-WebSocket-Version: 13\r\n");
    RequestBuilder << TEXT("Sec-WebSocket-Key: ") << HandshakeKey << TEXT("\r\n");

    if (!Protocols.IsEmpty())
    {
        RequestBuilder << TEXT("Sec-WebSocket-Protocol: ") << Protocols << TEXT("\r\n");
    }

    for (const TPair<FString, FString>& HeaderPair : Headers)
    {
        RequestBuilder << HeaderPair.Key << TEXT(": ") << HeaderPair.Value << TEXT("\r\n");
    }

    RequestBuilder << TEXT("\r\n");

    FTCHARToUTF8 HandshakeUtf8(RequestBuilder.ToString());
    int32 BytesSent = 0;
    if (!Socket->Send(reinterpret_cast<const uint8*>(HandshakeUtf8.Get()), HandshakeUtf8.Length(), BytesSent) || BytesSent != HandshakeUtf8.Length())
    {
        TearDown(TEXT("Failed to send WebSocket handshake."), false, 4000);
        return false;
    }

    TArray<uint8> ResponseBuffer;
    ResponseBuffer.Reserve(512);
    constexpr int32 TempSize = 256;
    uint8 Temp[TempSize];
    bool bHandshakeComplete = false;
    while (!bHandshakeComplete)
    {
        if (bStopping)
        {
            return false;
        }
        int32 BytesRead = 0;
        if (!Socket->Recv(Temp, TempSize, BytesRead))
        {
            TearDown(TEXT("WebSocket handshake failed while reading response."), false, 4000);
            return false;
        }
        if (BytesRead <= 0)
        {
            continue;
        }
        ResponseBuffer.Append(Temp, BytesRead);
        if (ResponseBuffer.Num() >= 4)
        {
            const int32 Count = ResponseBuffer.Num();
            if (ResponseBuffer[Count - 4] == '\r' && ResponseBuffer[Count - 3] == '\n' && ResponseBuffer[Count - 2] == '\r' && ResponseBuffer[Count - 1] == '\n')
            {
                bHandshakeComplete = true;
            }
        }
    }

    FString ResponseString = FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(ResponseBuffer.GetData())));
    FString HeaderSection;
    FString ExtraData;
    if (!ResponseString.Split(TEXT("\r\n\r\n"), &HeaderSection, &ExtraData))
    {
        HeaderSection = ResponseString;
    }

    TArray<FString> HeaderLines;
    HeaderSection.ParseIntoArrayLines(HeaderLines, false);
    if (HeaderLines.Num() == 0)
    {
        TearDown(TEXT("Malformed WebSocket handshake response."), false, 4000);
        return false;
    }

    const FString& StatusLine = HeaderLines[0];
    if (!StatusLine.Contains(TEXT("101")))
    {
        TearDown(TEXT("WebSocket server rejected handshake."), false, 4000);
        return false;
    }

    FString ExpectedAccept;
    {
        FTCHARToUTF8 AcceptUtf8(*(HandshakeKey + WebSocketGuid));
        FSHA1 Hash;
        Hash.Update(reinterpret_cast<const uint8*>(AcceptUtf8.Get()), AcceptUtf8.Length());
        Hash.Final();
        uint8 Digest[FSHA1::DigestSize];
        Hash.GetHash(Digest);
        ExpectedAccept = FBase64::Encode(Digest, FSHA1::DigestSize);
    }

    bool bAcceptValid = false;
    for (int32 i = 1; i < HeaderLines.Num(); ++i)
    {
        FString Key;
        FString Value;
        if (HeaderLines[i].Split(TEXT(":"), &Key, &Value))
        {
            Key = Key.TrimStartAndEnd();
            Value = Value.TrimStartAndEnd();
            if (Key.Equals(TEXT("Sec-WebSocket-Accept"), ESearchCase::IgnoreCase))
            {
                bAcceptValid = Value.Equals(ExpectedAccept, ESearchCase::CaseSensitive);
            }
        }
    }

    if (!bAcceptValid)
    {
        TearDown(TEXT("WebSocket handshake validation failed."), false, 4000);
        return false;
    }

    if (!ExtraData.IsEmpty())
    {
        const FTCHARToUTF8 ExtraUtf8(*ExtraData);
        PendingReceived.Append(reinterpret_cast<const uint8*>(ExtraUtf8.Get()), ExtraUtf8.Length());
    }

    return true;
}

bool FMcpBridgeWebSocket::PerformServerHandshake()
{
    // Read the client's WebSocket upgrade request
    TArray<uint8> RequestBuffer;
    RequestBuffer.Reserve(1024);
    constexpr int32 TempSize = 256;
    uint8 Temp[TempSize];
    bool bRequestComplete = false;
    FString ClientKey;

    while (!bRequestComplete)
    {
        if (bStopping)
        {
            return false;
        }

        int32 BytesRead = 0;
        if (!Socket->Recv(Temp, TempSize, BytesRead))
        {
            TearDown(TEXT("Failed to read WebSocket upgrade request."), false, 4000);
            return false;
        }

        if (BytesRead <= 0)
        {
            continue;
        }

        RequestBuffer.Append(Temp, BytesRead);

        // Check if we have a complete HTTP request (double CRLF)
        if (RequestBuffer.Num() >= 4)
        {
            const int32 Count = RequestBuffer.Num();
            if (RequestBuffer[Count - 4] == '\r' && RequestBuffer[Count - 3] == '\n' && 
                RequestBuffer[Count - 2] == '\r' && RequestBuffer[Count - 1] == '\n')
            {
                bRequestComplete = true;
            }
        }
    }

    FString RequestString = FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(RequestBuffer.GetData())));
    TArray<FString> RequestLines;
    RequestString.ParseIntoArrayLines(RequestLines, false);

    if (RequestLines.Num() == 0)
    {
        TearDown(TEXT("Malformed WebSocket upgrade request."), false, 4000);
        return false;
    }

    // Parse the request
    bool bValidUpgrade = false;
    bool bValidConnection = false;
    bool bValidVersion = false;

    for (int32 i = 1; i < RequestLines.Num(); ++i)
    {
        FString Key, Value;
        if (RequestLines[i].Split(TEXT(":"), &Key, &Value))
        {
            Key = Key.TrimStartAndEnd();
            Value = Value.TrimStartAndEnd();

            if (Key.Equals(TEXT("Upgrade"), ESearchCase::IgnoreCase) && Value.Equals(TEXT("websocket"), ESearchCase::IgnoreCase))
            {
                bValidUpgrade = true;
            }
            else if (Key.Equals(TEXT("Connection"), ESearchCase::IgnoreCase) && Value.Equals(TEXT("Upgrade"), ESearchCase::IgnoreCase))
            {
                bValidConnection = true;
            }
            else if (Key.Equals(TEXT("Sec-WebSocket-Version"), ESearchCase::IgnoreCase) && Value.Equals(TEXT("13"), ESearchCase::CaseSensitive))
            {
                bValidVersion = true;
            }
            else if (Key.Equals(TEXT("Sec-WebSocket-Key"), ESearchCase::IgnoreCase))
            {
                ClientKey = Value;
            }
        }
    }

    if (!bValidUpgrade || !bValidConnection || !bValidVersion || ClientKey.IsEmpty())
    {
        TearDown(TEXT("Invalid WebSocket upgrade request."), false, 4000);
        return false;
    }

    // Generate the accept key
    const FString AcceptGuid = TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    FString AcceptKey;
    {
        FTCHARToUTF8 AcceptUtf8(*(ClientKey + AcceptGuid));
        FSHA1 Hash;
        Hash.Update(reinterpret_cast<const uint8*>(AcceptUtf8.Get()), AcceptUtf8.Length());
        Hash.Final();
        uint8 Digest[FSHA1::DigestSize];
        Hash.GetHash(Digest);
        AcceptKey = FBase64::Encode(Digest, FSHA1::DigestSize);
    }

    // Send the upgrade response
    FString Response = FString::Printf(TEXT("HTTP/1.1 101 Switching Protocols\r\n"
                                             "Upgrade: websocket\r\n"
                                             "Connection: Upgrade\r\n"
                                             "Sec-WebSocket-Accept: %s\r\n"
                                             "\r\n"), *AcceptKey);

    FTCHARToUTF8 ResponseUtf8(*Response);
    int32 BytesSent = 0;
    if (!Socket->Send(reinterpret_cast<const uint8*>(ResponseUtf8.Get()), ResponseUtf8.Length(), BytesSent) || 
        BytesSent != ResponseUtf8.Length())
    {
        TearDown(TEXT("Failed to send WebSocket upgrade response."), false, 4000);
        return false;
    }

    return true;
}

bool FMcpBridgeWebSocket::ResolveEndpoint(TSharedPtr<FInternetAddr>& OutAddr)
{
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        return false;
    }

    const FString ServiceName = FString::FromInt(Port);
    FAddressInfoResult AddrInfo = SocketSubsystem->GetAddressInfo(*HostHeader, *ServiceName, EAddressInfoFlags::Default, NAME_None, ESocketType::SOCKTYPE_Streaming);
    if (AddrInfo.Results.Num() == 0)
    {
        return false;
    }

    OutAddr = AddrInfo.Results[0].Address;
    if (OutAddr.IsValid())
    {
        OutAddr->SetPort(Port);
    }
    return OutAddr.IsValid();
}

bool FMcpBridgeWebSocket::SendFrame(const TArray<uint8>& Frame)
{
    if (!Socket)
    {
        return false;
    }

    int32 BytesSent = 0;
    if (!Socket->Send(Frame.GetData(), Frame.Num(), BytesSent))
    {
        return false;
    }

    return BytesSent == Frame.Num();
}

bool FMcpBridgeWebSocket::SendCloseFrame(int32 StatusCode, const FString& Reason)
{
    TArray<uint8> Payload;
    Payload.Reserve(2 + Reason.Len() * 4);

    const uint16 Code = ToNetwork16(static_cast<uint16>(StatusCode));
    Payload.Append(reinterpret_cast<const uint8*>(&Code), sizeof(uint16));

    FTCHARToUTF8 ReasonUtf8(*Reason);
    const int32 ReasonBytes = FMath::Min<int32>(ReasonUtf8.Length(), 123); // ensure control frame payload stays within 125 bytes
    if (ReasonBytes > 0)
    {
        Payload.Append(reinterpret_cast<const uint8*>(ReasonUtf8.Get()), ReasonBytes);
    }

    return SendControlFrame(OpCodeClose, Payload);
}

bool FMcpBridgeWebSocket::SendTextFrame(const void* Data, SIZE_T Length)
{
    const uint8* Raw = static_cast<const uint8*>(Data);
    TArray<uint8> Frame;

    const uint8 Header = 0x80 | OpCodeText;
    Frame.Add(Header);

    if (Length <= 125)
    {
        Frame.Add(0x80 | static_cast<uint8>(Length));
    }
    else if (Length <= 0xFFFF)
    {
        Frame.Add(0x80 | 126);
        const uint16 SizeShort = ToNetwork16(static_cast<uint16>(Length));
        Frame.Append(reinterpret_cast<const uint8*>(&SizeShort), sizeof(uint16));
    }
    else
    {
        Frame.Add(0x80 | 127);
        const uint64 SizeLong = ToNetwork64(static_cast<uint64>(Length));
        Frame.Append(reinterpret_cast<const uint8*>(&SizeLong), sizeof(uint64));
    }

    uint8 MaskKey[4];
    for (uint8& Byte : MaskKey)
    {
        Byte = static_cast<uint8>(FMath::RandRange(0, 255));
    }
    Frame.Append(MaskKey, 4);

    const int64 Offset = Frame.Num();
    Frame.AddUninitialized(Length);
    for (SIZE_T Index = 0; Index < Length; ++Index)
    {
        Frame[Offset + Index] = Raw[Index] ^ MaskKey[Index % 4];
    }

    FScopeLock Guard(&SendMutex);
    return SendFrame(Frame);
}

bool FMcpBridgeWebSocket::SendControlFrame(const uint8 ControlOpCode, const TArray<uint8>& Payload)
{
    if (!Socket)
    {
        return false;
    }

    if (Payload.Num() > 125)
    {
        return false;
    }

    FScopeLock Guard(&SendMutex);

    TArray<uint8> Frame;
    Frame.Reserve(2 + 4 + Payload.Num());
    Frame.Add(0x80 | (ControlOpCode & 0x0F));
    Frame.Add(0x80 | static_cast<uint8>(Payload.Num()));

    uint8 MaskKey[4];
    for (uint8& Byte : MaskKey)
    {
        Byte = static_cast<uint8>(FMath::RandRange(0, 255));
    }

    Frame.Append(MaskKey, 4);
    const int32 PayloadOffset = Frame.Num();
    Frame.AddUninitialized(Payload.Num());
    for (int32 Index = 0; Index < Payload.Num(); ++Index)
    {
        Frame[PayloadOffset + Index] = Payload[Index] ^ MaskKey[Index % 4];
    }

    return SendFrame(Frame);
}

void FMcpBridgeWebSocket::HandleTextPayload(const TArray<uint8>& Payload)
{
    const FString Message = BytesToStringView(Payload);
    // Process message directly without dispatching to game thread
    // The MCP server should handle threading appropriately
    if (TSharedPtr<FMcpBridgeWebSocket> Pinned = SelfWeakPtr.Pin())
    {
        Pinned->MessageDelegate.Broadcast(Pinned, Message);
    }
}

void FMcpBridgeWebSocket::ResetFragmentState()
{
    FragmentAccumulator.Reset();
    bFragmentMessageActive = false;
}

bool FMcpBridgeWebSocket::ReceiveFrame()
{
    uint8 Header[2];
    if (!ReceiveExact(Header, 2))
    {
        TearDown(TEXT("Failed to read WebSocket frame header."), false, 4001);
        return false;
    }

    const bool bFinalFrame = (Header[0] & 0x80) != 0;
    const uint8 OpCode = Header[0] & 0x0F;
    uint64 PayloadLength = Header[1] & 0x7F;
    const bool bMasked = (Header[1] & 0x80) != 0;

    if (PayloadLength == 126)
    {
        uint8 Extended[2];
        if (!ReceiveExact(Extended, sizeof(Extended)))
        {
            TearDown(TEXT("Failed to read extended payload length."), false, 4001);
            return false;
        }
        uint16 ShortVal = 0;
        FMemory::Memcpy(&ShortVal, Extended, sizeof(uint16));
        PayloadLength = FromNetwork16(ShortVal);
    }
    else if (PayloadLength == 127)
    {
        uint8 Extended[8];
        if (!ReceiveExact(Extended, sizeof(Extended)))
        {
            TearDown(TEXT("Failed to read extended payload length."), false, 4001);
            return false;
        }
        uint64 LongVal = 0;
        FMemory::Memcpy(&LongVal, Extended, sizeof(uint64));
        PayloadLength = FromNetwork64(LongVal);
    }

    uint8 MaskKey[4] = {0, 0, 0, 0};
    if (bMasked)
    {
        if (!ReceiveExact(MaskKey, 4))
        {
            TearDown(TEXT("Failed to read masking key."), false, 4001);
            return false;
        }
    }

    TArray<uint8> Payload;
    if (PayloadLength > 0)
    {
        Payload.SetNumUninitialized(static_cast<int32>(PayloadLength));
        if (!ReceiveExact(Payload.GetData(), PayloadLength))
        {
            TearDown(TEXT("Failed to read WebSocket payload."), false, 4001);
            return false;
        }
        if (bMasked)
        {
            for (uint64 Index = 0; Index < PayloadLength; ++Index)
            {
                Payload[Index] ^= MaskKey[Index % 4];
            }
        }
    }

    if (OpCode == OpCodeClose)
    {
        TearDown(TEXT("WebSocket closed by peer."), true, 1000);
        return false;
    }

    // Handle control frames immediately (they must not be fragmented)
    if ((OpCode & 0x08) != 0)
    {
        if (!bFinalFrame)
        {
            TearDown(TEXT("Control frames must not be fragmented."), false, 4002);
            return false;
        }

        if (OpCode == OpCodePing)
        {
            SendControlFrame(OpCodePong, Payload);
            return true;
        }

        if (OpCode == OpCodePong)
        {
            // In server mode, receiving a pong means the client is responding to our ping
            // In client mode, receiving a pong means the server responded to our ping
            HeartbeatDelegate.Broadcast(SelfWeakPtr.Pin());
            return true;
        }

        // Unknown control frame
        return true;
    }

    if (OpCode == OpCodeContinuation)
    {
        if (!bFragmentMessageActive)
        {
            TearDown(TEXT("Unexpected continuation frame."), false, 4002);
            return false;
        }

        FragmentAccumulator.Append(Payload);

        if (bFinalFrame)
        {
            HandleTextPayload(FragmentAccumulator);
            ResetFragmentState();
        }
        return true;
    }

    if (bFragmentMessageActive)
    {
        TearDown(TEXT("Received new data frame before completing fragmented message."), false, 4002);
        return false;
    }

    if (OpCode == OpCodeText)
    {
        if (bFinalFrame)
        {
            HandleTextPayload(Payload);
        }
        else
        {
            FragmentAccumulator = Payload;
            bFragmentMessageActive = true;
        }
        return true;
    }

    if (OpCode == OpCodeBinary)
    {
        TearDown(TEXT("Binary frames are not supported."), false, 4003);
        return false;
    }

    TearDown(TEXT("Unsupported WebSocket opcode."), false, 4003);
    return false;
}

bool FMcpBridgeWebSocket::ReceiveExact(uint8* Buffer, SIZE_T Length)
{
    SIZE_T Collected = 0;

    {
        FScopeLock Guard(&ReceiveMutex);
        const SIZE_T Existing = FMath::Min(static_cast<SIZE_T>(PendingReceived.Num()), Length);
        if (Existing > 0)
        {
            FMemory::Memcpy(Buffer, PendingReceived.GetData(), Existing);
            PendingReceived.RemoveAt(0, Existing, EAllowShrinking::No);
            Collected += Existing;
        }
    }

    while (Collected < Length)
    {
        if (bStopping)
        {
            return false;
        }

        uint32 PendingSize = 0;
        if (!Socket->HasPendingData(PendingSize))
        {
            if (StopEvent && StopEvent->Wait(FTimespan::FromMilliseconds(50)))
            {
                return false;
            }
            continue;
        }

        const uint32 ReadSize = FMath::Min<uint32>(PendingSize, 4096);
        TArray<uint8> Temp;
        Temp.SetNumUninitialized(ReadSize);
        int32 BytesRead = 0;
        if (!Socket->Recv(Temp.GetData(), ReadSize, BytesRead))
        {
            return false;
        }

        if (BytesRead <= 0)
        {
            continue;
        }

        const uint32 CopyCount = FMath::Min<uint32>(static_cast<uint32>(BytesRead), static_cast<uint32>(Length - Collected));
        FMemory::Memcpy(Buffer + Collected, Temp.GetData(), CopyCount);
        Collected += CopyCount;

        if (static_cast<uint32>(BytesRead) > CopyCount)
        {
            FScopeLock Guard(&ReceiveMutex);
            PendingReceived.Append(Temp.GetData() + CopyCount, BytesRead - CopyCount);
        }
    }

    return true;
}
