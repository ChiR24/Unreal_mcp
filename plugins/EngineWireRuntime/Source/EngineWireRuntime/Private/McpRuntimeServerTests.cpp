#include "McpRuntimeReads.h"
#include "McpRuntimeServer.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "IPAddress.h"
#include "Misc/AutomationTest.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace McpRuntimeServerTestsDetail
{
// The whole reply a POST of Body to the server gives, over a real loopback socket; empty when none came.
FString Post(int32 Port, const FString& Token, const FString& Body)
{
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    const TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
    Address->SetLoopbackAddress();
    Address->SetPort(Port);
    FSocket* Client = Sockets->CreateSocket(NAME_Stream, TEXT("EngineWireRuntimeTest"), Address->GetProtocolType());
    FString Reply;
    if (Client && Client->Connect(*Address))
    {
        const FTCHARToUTF8 Payload(*Body);
        const FTCHARToUTF8 Head(*FString::Printf(TEXT("POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n%s"
            "Content-Length: %d\r\n\r\n"), Token.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("X-MCP-Capability-Token: %s\r\n"), *Token),
            Payload.Length()));
        int32 Sent = 0;
        Client->Send(reinterpret_cast<const uint8*>(Head.Get()), Head.Length(), Sent);
        Client->Send(reinterpret_cast<const uint8*>(Payload.Get()), Payload.Length(), Sent);
        TArray<uint8> Data;
        constexpr int32 ChunkSize = 4096;
        uint8 Chunk[ChunkSize];
        int32 Read = 0;
        while (Client->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(5)) && Client->Recv(Chunk, ChunkSize, Read) && Read > 0)
        {
            Data.Append(Chunk, Read);
        }
        const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Data.GetData()), Data.Num());
        Reply = FString(Text.Length(), Text.Get());
    }
    if (Client)
    {
        Client->Close();
        Sockets->DestroySocket(Client);
    }
    return Reply;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpRuntimeServerTest, "EngineWire.Runtime.Server",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpRuntimeServerTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpRuntimeServerTestsDetail;
    TestTrue(TEXT("initialize answers its protocol version"),
             McpRuntimeAnswer(TEXT(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18"}})")).Contains(TEXT("\"2025-06-18\"")));
    TestTrue(TEXT("tools/list offers the one unreal tool"),
             McpRuntimeAnswer(TEXT(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})")).Contains(TEXT("\"name\":\"unreal\"")));
    TestTrue(TEXT("a notification gets no answer"), McpRuntimeAnswer(TEXT(R"({"jsonrpc":"2.0","method":"notifications/initialized"})")).IsEmpty());
    TestTrue(TEXT("an unknown capability is an error result"),
             McpRuntimeAnswer(TEXT(R"({"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"unreal","arguments":{"operation":"execute","capability":"runtime.nope"}}})")).Contains(TEXT("\"isError\":true")));
    FString Error;
    const TSharedPtr<FJsonObject> Listed = McpRuntimeRead(TEXT("runtime.list_actors"), nullptr, Error);
    TestTrue(TEXT("list_actors reads the world it finds"), Listed.IsValid() && Listed->HasField(TEXT("totalCount")));
    TestTrue(TEXT("frame_cost reads the frame"), McpRuntimeRead(TEXT("runtime.frame_cost"), nullptr, Error).IsValid());
    const TSharedPtr<FJsonObject> NoKey = MakeShared<FJsonObject>();
    NoKey->SetStringField(TEXT("key"), TEXT("NoSuchKey_MCP"));
    TestTrue(TEXT("an unknown key is refused"), !McpRuntimeRead(TEXT("runtime.press_key"), NoKey, Error).IsValid() && Error.StartsWith(TEXT("INVALID_KEY")));

    FMcpRuntimeServer Server;
    if (!TestTrue(TEXT("the server starts on a free port"), Server.Start(0)))
    {
        return false;
    }
    const FString Bound = Server.GetBoundAddress();
    TestTrue(FString::Printf(TEXT("it listens on loopback only (%s)"), *Bound), Bound == TEXT("127.0.0.1") || Bound == TEXT("::1"));
    const FString Init = TEXT(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
    TestTrue(TEXT("a call without the token is refused"), Post(Server.GetPort(), FString(), Init).StartsWith(TEXT("HTTP/1.1 401")));
    TestTrue(TEXT("a wrong token is refused"), Post(Server.GetPort(), TEXT("not-the-token"), Init).StartsWith(TEXT("HTTP/1.1 401")));
    const FString Answered = Post(Server.GetPort(), McpRuntimeToken(), Init);
    TestTrue(TEXT("the token is taken"), Answered.StartsWith(TEXT("HTTP/1.1 200")) && Answered.Contains(TEXT("protocolVersion")));
    Server.Shutdown();
    return true;
}
#endif
