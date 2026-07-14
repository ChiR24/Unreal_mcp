#include "McpAutomationBridgeSubsystem.h"
#include "Domains/Data/McpAutomationBridge_DataHandlers.h"

void UMcpAutomationBridgeSubsystem::InitializeHandlers()
{
    RegisterCoreAndAssetHandlers();
    RegisterEnvironmentMediaHandlers();
    RegisterSystemAndEditorHandlers();
    RegisterAssetRoutingHandlers();
    RegisterBlueprintAndDomainHandlers();
    RegisterAudioAnimationHandlers();
    RegisterWorldAndMiscHandlers();
    FMcpAutomationBridge_DataHandlers::RegisterHandlers(this);
    LoadConfiguredHandlerAliases();
}
