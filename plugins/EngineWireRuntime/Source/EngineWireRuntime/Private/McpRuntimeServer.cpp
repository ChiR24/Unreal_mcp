#include "McpRuntimeServer.h"

#include "HAL/RunnableThread.h"
#include "IPAddress.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

DEFINE_LOG_CATEGORY(LogEngineWireRuntime);

namespace McpRuntimeServerDetail
{
constexpr int32 MaxHeader = 16 * 1024;
constexpr int32 MaxBody = 1024 * 1024;

int32 FindHeaderEnd(const TArray<uint8>& Data)
{
    for (int32 Index = 3; Index < Data.Num(); ++Index)
    {
        if (Data[Index - 3] == '\r' && Data[Index - 2] == '\n' && Data[Index - 1] == '\r' && Data[Index] == '\n')
        {
            return Index + 1;
        }
    }
    return INDEX_NONE;
}

// One HTTP request: its method, path, token header and body. False on a malformed, oversized or stalled request.
bool ReadRequest(FSocket& Client, FString& Method, FString& Path, FString& Token, FString& Body)
{
    TArray<uint8> Data;
    int32 HeaderEnd = INDEX_NONE;
    int32 Length = 0;
    constexpr int32 ChunkSize = 4096;
    uint8 Chunk[ChunkSize];
    while (HeaderEnd == INDEX_NONE || Data.Num() < HeaderEnd + Length)
    {
        int32 Read = 0;
        if (!Client.Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(10)) || !Client.Recv(Chunk, ChunkSize, Read) || Read <= 0)
        {
            return false;
        }
        Data.Append(Chunk, Read);
        if (HeaderEnd == INDEX_NONE && (HeaderEnd = FindHeaderEnd(Data)) != INDEX_NONE)
        {
            const FUTF8ToTCHAR HeadText(reinterpret_cast<const ANSICHAR*>(Data.GetData()), HeaderEnd);
            TArray<FString> Lines, First;
            FString(HeadText.Length(), HeadText.Get()).ParseIntoArray(Lines, TEXT("\r\n"));
            if (Lines.Num() > 0)
            {
                Lines[0].ParseIntoArrayWS(First);
            }
            if (First.Num() < 2)
            {
                return false;
            }
            Method = First[0];
            Path = First[1];
            for (const FString& Line : Lines)
            {
                FString Key, Value;
                if (Line.Split(TEXT(":"), &Key, &Value))
                {
                    Key.TrimStartAndEndInline();
                    Value.TrimStartAndEndInline();
                    if (Key.Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase) && (!LexTryParseString(Length, *Value) || Length < 0 || Length > MaxBody))
                    {
                        return false;
                    }
                    if (Key.Equals(TEXT("X-MCP-Capability-Token"), ESearchCase::IgnoreCase))
                    {
                        Token = Value;
                    }
                }
            }
        }
        if (HeaderEnd == INDEX_NONE && Data.Num() > MaxHeader)
        {
            return false;
        }
    }
    const FUTF8ToTCHAR BodyText(reinterpret_cast<const ANSICHAR*>(Data.GetData() + HeaderEnd), Length);
    Body = FString(BodyText.Length(), BodyText.Get());
    return true;
}

void Send(FSocket& Client, int32 Status, const TCHAR* Reason, const FString& Json)
{
    const FTCHARToUTF8 Body(*Json);
    const FTCHARToUTF8 Head(*FString::Printf(TEXT("HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\n"
                                                  "Connection: close\r\n\r\n"), Status, Reason, Body.Length()));
    int32 Sent = 0;
    Client.Send(reinterpret_cast<const uint8*>(Head.Get()), Head.Length(), Sent);
    if (Body.Length() > 0)
    {
        Client.Send(reinterpret_cast<const uint8*>(Body.Get()), Body.Length(), Sent);
    }
}

FString ErrorJson(int32 Code, const TCHAR* Message)
{
    return FString::Printf(TEXT("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":%d,\"message\":\"%s\"}}"), Code, Message);
}
}

FString McpRuntimeToken()
{
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP"), TEXT("capability-token"));
    FString Token;
    if (FFileHelper::LoadFileToString(Token, *Path) && !Token.TrimStartAndEnd().IsEmpty())
    {
        return Token.TrimStartAndEnd();
    }
    // ponytail: two random GUIDs (244 random bits); the editor plugin's own token, when it wrote one, is used instead.
    Token = (FGuid::NewGuid().ToString(EGuidFormats::Digits) + FGuid::NewGuid().ToString(EGuidFormats::Digits)).ToLower();
    return FFileHelper::SaveStringToFile(Token, *Path) ? Token : FString();
}

bool McpRuntimeTokenEquals(const FString& A, const FString& B)
{
    const FTCHARToUTF8 Ua(*A), Ub(*B);
    const int32 Longest = FMath::Max(Ua.Length(), Ub.Length());
    uint32 Diff = static_cast<uint32>(Ua.Length() ^ Ub.Length());
    for (int32 Index = 0; Index < Longest; ++Index)
    {
        const uint8 Ca = Index < Ua.Length() ? static_cast<uint8>(Ua.Get()[Index]) : 0;
        const uint8 Cb = Index < Ub.Length() ? static_cast<uint8>(Ub.Get()[Index]) : 0;
        Diff |= static_cast<uint32>(Ca ^ Cb);
    }
    return Diff == 0 && !A.IsEmpty();
}

FMcpRuntimeServer::~FMcpRuntimeServer()
{
    Shutdown();
}

bool FMcpRuntimeServer::Start(int32 Port)
{
    Token = McpRuntimeToken();
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (Token.IsEmpty() || !Sockets || Listener)
    {
        return false;
    }
    const TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
    Address->SetLoopbackAddress();
    Address->SetPort(Port);
    Listener = Sockets->CreateSocket(NAME_Stream, TEXT("EngineWireRuntime"), Address->GetProtocolType());
    if (!Listener || !Listener->Bind(*Address) || !Listener->Listen(8))
    {
        Shutdown();
        return false;
    }
    BoundPort = Listener->GetPortNo();
    Thread = FRunnableThread::Create(this, TEXT("EngineWireRuntime"));
    return Thread != nullptr;
}

void FMcpRuntimeServer::Shutdown()
{
    bStopping = true;
    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }
    if (Listener)
    {
        Listener->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Listener);
        Listener = nullptr;
    }
}

FString FMcpRuntimeServer::GetBoundAddress() const
{
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    const TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
    if (Listener)
    {
        Listener->GetAddress(*Address);
    }
    return Address->ToString(false);
}

uint32 FMcpRuntimeServer::Run()
{
    while (!bStopping)
    {
        bool bPending = false;
        if (!Listener->WaitForPendingConnection(bPending, FTimespan::FromMilliseconds(200)) || !bPending)
        {
            continue;
        }
        if (FSocket* Client = Listener->Accept(TEXT("EngineWireRuntimeClient")))
        {
            Serve(*Client);
            Client->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Client);
        }
    }
    return 0;
}

void FMcpRuntimeServer::Serve(FSocket& Client) const
{
    using namespace McpRuntimeServerDetail;
    FString Method, Path, Sent, Body;
    if (!ReadRequest(Client, Method, Path, Sent, Body))
    {
        Send(Client, 400, TEXT("Bad Request"), ErrorJson(-32700, TEXT("Malformed or oversized request")));
    }
    else if (Path != TEXT("/mcp"))
    {
        Send(Client, 404, TEXT("Not Found"), ErrorJson(-32601, TEXT("Only /mcp is served")));
    }
    else if (Method != TEXT("POST"))
    {
        Send(Client, 405, TEXT("Method Not Allowed"), ErrorJson(-32601, TEXT("POST a JSON-RPC message to /mcp")));
    }
    else if (!McpRuntimeTokenEquals(Sent, Token))
    {
        Send(Client, 401, TEXT("Unauthorized"), ErrorJson(-32600, TEXT("Invalid capability token")));
    }
    else
    {
        const FString Answer = McpRuntimeAnswer(Body);
        Send(Client, Answer.IsEmpty() ? 202 : 200, Answer.IsEmpty() ? TEXT("Accepted") : TEXT("OK"), Answer);
    }
}
