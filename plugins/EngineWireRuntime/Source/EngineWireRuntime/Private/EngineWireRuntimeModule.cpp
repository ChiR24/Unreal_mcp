#include "McpRuntimeServer.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

// Starts the loopback server only when the game is launched with -EngineWireRuntime (port 3100, or
// -EngineWireRuntimePort=N); without it the module does nothing.
class FEngineWireRuntimeModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("EngineWireRuntime")))
        {
            return;
        }
        int32 Port = 3100;
        FParse::Value(FCommandLine::Get(), TEXT("EngineWireRuntimePort="), Port);
        Server = MakeUnique<FMcpRuntimeServer>();
        if (!Server->Start(Port))
        {
            UE_LOG(LogEngineWireRuntime, Warning, TEXT("EngineWire Runtime did not start: port %d is taken, or Saved/MCP/capability-token "
                                                       "can be neither read nor written."), Port);
            Server.Reset();
            return;
        }
        UE_LOG(LogEngineWireRuntime, Log, TEXT("EngineWire Runtime listening on 127.0.0.1:%d/mcp (token: Saved/MCP/capability-token)."),
               Server->GetPort());
    }

    virtual void ShutdownModule() override
    {
        Server.Reset();
    }

private:
    TUniquePtr<FMcpRuntimeServer> Server;
};

IMPLEMENT_MODULE(FEngineWireRuntimeModule, EngineWireRuntime)
