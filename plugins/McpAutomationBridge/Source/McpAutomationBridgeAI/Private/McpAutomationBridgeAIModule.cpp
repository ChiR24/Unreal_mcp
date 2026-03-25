// =============================================================================
// McpAutomationBridgeAIModule.cpp
// =============================================================================
// Module implementation for the AI optional module.
//
// This module registers AI handlers with the base McpAutomationBridge module
// when AI plugins (StateTree, SmartObjects, MassEntity, MassSpawner) are
// enabled in the project.
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#include "McpAutomationBridgeAIModule.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Interfaces/IPluginManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpAIModule, Log, All);

const FName FMcpAutomationBridgeAIModule::ModuleName = TEXT("McpAutomationBridgeAI");

void FMcpAutomationBridgeAIModule::StartupModule()
{
    UE_LOG(LogMcpAIModule, Log, TEXT("McpAutomationBridgeAI module starting up"));
    
#if MCP_STATETREE_MODULE_AVAILABLE || MCP_SMARTOBJECTS_MODULE_AVAILABLE || MCP_MASSENTITY_MODULE_AVAILABLE || MCP_MASSSPAWNER_MODULE_AVAILABLE
    // Check if any AI module is actually loaded at runtime
    bool bAnyAIModuleLoaded = 
#if MCP_STATETREE_MODULE_AVAILABLE
        FModuleManager::Get().IsModuleLoaded(TEXT("StateTreeModule")) ||
#endif
#if MCP_SMARTOBJECTS_MODULE_AVAILABLE
        FModuleManager::Get().IsModuleLoaded(TEXT("SmartObjectsModule")) ||
#endif
#if MCP_MASSENTITY_MODULE_AVAILABLE
        FModuleManager::Get().IsModuleLoaded(TEXT("MassEntity")) ||
#endif
#if MCP_MASSSPAWNER_MODULE_AVAILABLE
        FModuleManager::Get().IsModuleLoaded(TEXT("MassSpawner")) ||
#endif
        false;

    if (!bAnyAIModuleLoaded)
    {
        // Check if any AI plugin exists but isn't loaded yet
        bool bAnyPluginEnabled = false;
        
#if MCP_STATETREE_MODULE_AVAILABLE
        TSharedPtr<IPlugin> StateTreePlugin = IPluginManager::Get().FindPlugin(TEXT("StateTree"));
        if (StateTreePlugin.IsValid() && StateTreePlugin->IsEnabled())
        {
            bAnyPluginEnabled = true;
            FModuleManager::Get().LoadModule(TEXT("StateTreeModule"));
        }
#endif

#if MCP_SMARTOBJECTS_MODULE_AVAILABLE
        TSharedPtr<IPlugin> SmartObjectsPlugin = IPluginManager::Get().FindPlugin(TEXT("SmartObjects"));
        if (SmartObjectsPlugin.IsValid() && SmartObjectsPlugin->IsEnabled())
        {
            bAnyPluginEnabled = true;
            FModuleManager::Get().LoadModule(TEXT("SmartObjectsModule"));
        }
#endif

#if MCP_MASSENTITY_MODULE_AVAILABLE
        TSharedPtr<IPlugin> MassGameplayPlugin = IPluginManager::Get().FindPlugin(TEXT("MassGameplay"));
        if (MassGameplayPlugin.IsValid() && MassGameplayPlugin->IsEnabled())
        {
            bAnyPluginEnabled = true;
            FModuleManager::Get().LoadModule(TEXT("MassEntity"));
#if MCP_MASSSPAWNER_MODULE_AVAILABLE
            FModuleManager::Get().LoadModule(TEXT("MassSpawner"));
#endif
        }
#endif

        if (!bAnyPluginEnabled)
        {
            UE_LOG(LogMcpAIModule, Warning, 
                TEXT("No AI plugins are enabled - AI handlers will not be registered"));
            return;
        }
    }
    
    // Get the subsystem and register handlers
    if (GEditor)
    {
        if (UMcpAutomationBridgeSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMcpAutomationBridgeSubsystem>())
        {
            RegisterAIHandlers(Subsystem);
            bHandlersRegistered = true;
            UE_LOG(LogMcpAIModule, Log, TEXT("AI handlers registered successfully"));
        }
        else
        {
            UE_LOG(LogMcpAIModule, Warning, 
                TEXT("McpAutomationBridgeSubsystem not available - will retry on subsystem init"));
        }
    }
#else
    UE_LOG(LogMcpAIModule, Warning, 
        TEXT("AI module was not built with AI plugin support - handlers disabled"));
#endif
}

void FMcpAutomationBridgeAIModule::ShutdownModule()
{
    if (bHandlersRegistered)
    {
        // Handlers will be cleaned up by subsystem deinitialization
        bHandlersRegistered = false;
        UE_LOG(LogMcpAIModule, Log, TEXT("AI module shut down"));
    }
}

void FMcpAutomationBridgeAIModule::RegisterAIHandlers(UMcpAutomationBridgeSubsystem* Subsystem)
{
    if (!Subsystem)
    {
        return;
    }
    
    // Register the AI handler - this will override the stub in the base module
    Subsystem->RegisterHandler(TEXT("manage_ai"),
        [Subsystem](const FString& RequestId, const FString& Action,
                    const TSharedPtr<FJsonObject>& Payload,
                    TSharedPtr<FMcpBridgeWebSocket> Socket) {
            return Subsystem->HandleManageAIAction(RequestId, Action, Payload, Socket);
        });
    
    UE_LOG(LogMcpAIModule, Log, TEXT("Registered manage_ai handler"));
}

IMPLEMENT_MODULE(FMcpAutomationBridgeAIModule, McpAutomationBridgeAI)