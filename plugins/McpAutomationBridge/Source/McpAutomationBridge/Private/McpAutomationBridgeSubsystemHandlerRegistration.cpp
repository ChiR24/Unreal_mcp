#include "McpAutomationBridgeSubsystem.h"

void UMcpAutomationBridgeSubsystem::RegisterHandler(
    const FString& Action,
    FAutomationHandler Handler)
{
    if (Handler)
    {
        AutomationHandlers.Add(Action, Handler);
    }
}

void UMcpAutomationBridgeSubsystem::InitializeHandlers()
{
    RegisterCoreAndAssetHandlers();
    RegisterEnvironmentMediaHandlers();
    RegisterSystemAndEditorHandlers();
    RegisterAssetRoutingHandlers();
    RegisterBlueprintAndDomainHandlers();
    RegisterAudioAnimationHandlers();
    RegisterWorldAndMiscHandlers();
}
