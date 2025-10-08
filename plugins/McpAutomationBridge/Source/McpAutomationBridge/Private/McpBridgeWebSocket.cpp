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
{
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

bool FMcpBridgeWebSocket::Init()
{
    return true;
}

uint32 FMcpBridgeWebSocket::Run()
{
    if (!PerformHandshake())
    {
        return 0;
    }

    bConnected = true;
    DispatchOnGameThread([WeakThis = SelfWeakPtr]
    {
        if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
        {
            Pinned->ConnectedDelegate.Broadcast();
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
            Pinned->ClosedDelegate.Broadcast(StatusCode, Reason, bWasClean);
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
    DispatchOnGameThread([WeakThis = SelfWeakPtr, Message]
    {
        if (TSharedPtr<FMcpBridgeWebSocket> Pinned = WeakThis.Pin())
        {
            Pinned->MessageDelegate.Broadcast(Message);
        }
    });
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

    if (OpCode == OpCodePing)
    {
        if (!bFinalFrame)
        {
            TearDown(TEXT("Ping frames must not be fragmented."), false, 4002);
            return false;
        }
        SendControlFrame(OpCodePong, Payload);
        return true;
    }

    if (OpCode == OpCodePong)
    {
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

    if ((OpCode & 0x08) != 0)
    {
        if (!bFinalFrame)
        {
            TearDown(TEXT("Control frames must not be fragmented."), false, 4002);
            return false;
        }
        return true;
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
