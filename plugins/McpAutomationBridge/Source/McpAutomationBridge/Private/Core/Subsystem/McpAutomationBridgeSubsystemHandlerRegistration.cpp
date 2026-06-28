#include "McpAutomationBridgeSubsystem.h"

void UMcpAutomationBridgeSubsystem::InitializeHandlers()
{
    RegisterCoreAndAssetHandlers();
    RegisterEnvironmentMediaHandlers();
    RegisterSystemAndEditorHandlers();
    RegisterAssetRoutingHandlers();
    RegisterBlueprintAndDomainHandlers();
    RegisterAudioAnimationHandlers();
    RegisterWorldAndMiscHandlers();
    RegisterCustomToolingHandlers();
    LoadConfiguredHandlerAliases();
}
