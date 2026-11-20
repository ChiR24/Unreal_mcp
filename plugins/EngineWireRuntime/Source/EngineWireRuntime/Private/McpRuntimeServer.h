#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Templates/Atomic.h"

class FRunnableThread;
class FSocket;

DECLARE_LOG_CATEGORY_EXTERN(LogEngineWireRuntime, Log, All);

// The project's capability token (Saved/MCP/capability-token, the file the editor plugin writes), made and saved when
// there is none; empty when it can be neither read nor written, and then nothing listens.
FString McpRuntimeToken();

// Constant time over the longer of the two, so a wrong token says nothing about how much of it was right.
bool McpRuntimeTokenEquals(const FString& A, const FString& B);

// The JSON-RPC answer to one MCP request body (initialize, ping, tools/list, tools/call of the runtime reads); empty
// for a notification, which gets none. The reads run on the game thread, whichever thread calls this.
FString McpRuntimeAnswer(const FString& Body);

// A listener on 127.0.0.1 only, never another interface, for POST /mcp with the token in X-MCP-Capability-Token, one
// request per connection. Its own small loop rather than the HTTPServer module, whose bind address comes from the
// engine config ("any" binds every interface): loopback is guaranteed here, not configured.
class FMcpRuntimeServer final : public FRunnable
{
public:
    ~FMcpRuntimeServer() override;
    // False, with nothing listening, when no token can be had or the port is taken (Port 0 takes a free one).
    bool Start(int32 Port);
    void Shutdown();
    int32 GetPort() const { return BoundPort; }
    FString GetBoundAddress() const;

    virtual uint32 Run() override;
    virtual void Stop() override { bStopping = true; }

private:
    void Serve(FSocket& Client) const;

    FSocket* Listener = nullptr;
    FRunnableThread* Thread = nullptr;
    FString Token;
    int32 BoundPort = 0;
    TAtomic<bool> bStopping{false};
};
