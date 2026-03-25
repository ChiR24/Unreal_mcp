// =============================================================================
// McpAutomationBridgeGASModule.cpp
// =============================================================================
// Module implementation for the GAS (Gameplay Ability System) optional module.
//
// This module registers GAS handlers with the base McpAutomationBridge module
// when the GameplayAbilities plugin is enabled in the project.
//
// IMPORTANT: This module uses LoadingPhase "None" and is loaded manually by
// the base McpAutomationBridgeSubsystem when GameplayAbilities is available.
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpAutomationBridgeGASModule.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpGASModule, Log, All);

const FName FMcpAutomationBridgeGASModule::ModuleName = TEXT("McpAutomationBridgeGAS");

// Console command to manually load GAS module (for debugging)
static FAutoConsoleCommand LoadGASModuleCmd(
    TEXT("Mcp.LoadGASModule"),
    TEXT("Manually load the McpAutomationBridgeGAS module"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        FModuleManager::Get().LoadModule(TEXT("McpAutomationBridgeGAS"));
    })
);

void FMcpAutomationBridgeGASModule::StartupModule()
{
    UE_LOG(LogMcpGASModule, Log, TEXT("McpAutomationBridgeGAS module starting up"));
    
#if MCP_GAS_MODULE_AVAILABLE && MCP_HAS_GAS
    // Check if GameplayAbilities is actually loaded at runtime
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("GameplayAbilities")))
    {
        // Check if the plugin exists but isn't loaded yet
        TSharedPtr<IPlugin> GASPlugin = IPluginManager::Get().FindPlugin(TEXT("GameplayAbilities"));
        if (!GASPlugin.IsValid() || !GASPlugin->IsEnabled())
        {
            UE_LOG(LogMcpGASModule, Warning, 
                TEXT("GameplayAbilities plugin is not enabled - GAS handlers will not be registered"));
            return;
        }
        
        // Try to load the module
        FModuleManager::Get().LoadModule(TEXT("GameplayAbilities"));
    }
    
    // Register handlers - subsystem should be available since we're loaded after it
    TryRegisterHandlers();
#else
    UE_LOG(LogMcpGASModule, Warning, 
        TEXT("GAS module was not built with GameplayAbilities support - handlers disabled"));
#endif
}

void FMcpAutomationBridgeGASModule::ShutdownModule()
{
    if (bHandlersRegistered)
    {
        // Handlers will be cleaned up by subsystem deinitialization
        bHandlersRegistered = false;
        UE_LOG(LogMcpGASModule, Log, TEXT("GAS module shut down"));
    }
}

void FMcpAutomationBridgeGASModule::TryRegisterHandlers()
{
#if MCP_GAS_MODULE_AVAILABLE && MCP_HAS_GAS
    if (bHandlersRegistered)
    {
        return; // Already registered
    }
    
    if (!GEditor)
    {
        UE_LOG(LogMcpGASModule, Warning, TEXT("GEditor not available - cannot register handlers"));
        return;
    }
    
    UMcpAutomationBridgeSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMcpAutomationBridgeSubsystem>();
    if (!Subsystem)
    {
        UE_LOG(LogMcpGASModule, Warning, 
            TEXT("McpAutomationBridgeSubsystem not available - cannot register handlers"));
        return;
    }
    
    RegisterGASHandlers(Subsystem);
    bHandlersRegistered = true;
    UE_LOG(LogMcpGASModule, Log, TEXT("GAS handlers registered successfully"));
#endif
}

void FMcpAutomationBridgeGASModule::RegisterGASHandlers(UMcpAutomationBridgeSubsystem* Subsystem)
{
    if (!Subsystem)
    {
        return;
    }
    
#if MCP_GAS_MODULE_AVAILABLE && MCP_HAS_GAS
    // Register the GAS handler - this will override the stub in the base module
    Subsystem->RegisterHandler(TEXT("manage_gas"),
        [Subsystem](const FString& RequestId, const FString& Action,
                    const TSharedPtr<FJsonObject>& Payload,
                    TSharedPtr<FMcpBridgeWebSocket> Socket) {
            return Subsystem->HandleManageGASAction(RequestId, Action, Payload, Socket);
        });
    
    UE_LOG(LogMcpGASModule, Log, TEXT("Registered manage_gas handler"));
#endif
}

IMPLEMENT_MODULE(FMcpAutomationBridgeGASModule, McpAutomationBridgeGAS)
