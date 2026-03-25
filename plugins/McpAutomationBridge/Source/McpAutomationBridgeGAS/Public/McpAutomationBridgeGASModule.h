// =============================================================================
// McpAutomationBridgeGASModule.h
// =============================================================================
// Module interface for the GAS (Gameplay Ability System) optional module.
//
// This module registers GAS handlers with the base McpAutomationBridge module
// when the GameplayAbilities plugin is enabled.
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UMcpAutomationBridgeSubsystem;

/**
 * GAS (Gameplay Ability System) optional module for MCP Automation Bridge.
 * 
 * This module provides handlers for manage_gas tool actions when the 
 * GameplayAbilities plugin is enabled in the project.
 */
class FMcpAutomationBridgeGASModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
    /** Check if this module is currently loaded */
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded(ModuleName);
    }
    
    /** Get the module name */
    static inline FName GetModuleName() { return ModuleName; }

private:
    static const FName ModuleName;
    
    /** Register GAS handlers with the subsystem */
    void RegisterGASHandlers(UMcpAutomationBridgeSubsystem* Subsystem);
    
    /** Flag to track if handlers were registered */
    bool bHandlersRegistered = false;
};
