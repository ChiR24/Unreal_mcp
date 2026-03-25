// =============================================================================
// McpAutomationBridgeAIModule.h
// =============================================================================
// Module interface for the AI optional module.
//
// This module provides handlers for manage_ai tool actions when AI plugins
// (StateTree, SmartObjects, MassEntity, MassSpawner) are enabled.
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UMcpAutomationBridgeSubsystem;

/**
 * AI optional module for MCP Automation Bridge.
 * 
 * This module provides handlers for manage_ai tool actions when AI plugins
 * (StateTree, SmartObjects, MassEntity, MassSpawner) are enabled in the project.
 */
class FMcpAutomationBridgeAIModule : public IModuleInterface
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
    
    /** Register AI handlers with the subsystem */
    void RegisterAIHandlers(UMcpAutomationBridgeSubsystem* Subsystem);
    
    /** Try to register handlers (can be called after module load) */
    void TryRegisterHandlers();
    
    /** Flag to track if handlers were registered */
    bool bHandlersRegistered = false;
};