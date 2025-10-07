#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpAutomationBridge, Log, All);

class FMcpAutomationBridgeModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        UE_LOG(LogMcpAutomationBridge, Log, TEXT("MCP Automation Bridge module initialized."));
    }

    virtual void ShutdownModule() override
    {
        UE_LOG(LogMcpAutomationBridge, Log, TEXT("MCP Automation Bridge module shut down."));
    }
};

IMPLEMENT_MODULE(FMcpAutomationBridgeModule, McpAutomationBridge)
