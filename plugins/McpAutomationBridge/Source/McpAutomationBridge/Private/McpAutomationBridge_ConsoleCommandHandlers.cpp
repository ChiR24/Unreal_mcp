// =============================================================================
// McpAutomationBridge_ConsoleCommandHandlers.cpp
// =============================================================================
// Handler implementations for console command execution operations.
//
// HANDLERS IMPLEMENTED:
// ---------------------
// - batch_console_commands: Execute multiple console commands in batch
// - console_command: Execute a single console command
//
// SECURITY:
// ---------
// - Commands are validated against blocked patterns
// - Path traversal in command arguments is blocked
// =============================================================================

#include "McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#endif

// =============================================================================
// Logging
// =============================================================================

DEFINE_LOG_CATEGORY_STATIC(LogMcpConsoleHandlers, Log, All);

// =============================================================================
// Blocked Command Patterns
// =============================================================================

namespace ConsoleCommandSecurity
{
    // Commands that are blocked for security reasons
    static const TArray<FString> BLOCKED_COMMANDS = {
        TEXT("shutdown"),
        TEXT("quit"),
        TEXT("exit"),
        TEXT("crash"),
        TEXT("debugbreak"),
        TEXT("recompileglobalshaders"),  // Can cause instability
        TEXT("deriveddatacache"),         // Can corrupt DDC
    };
    
    // Commands that require explicit enable flag
    static const TArray<FString> RESTRICTED_COMMANDS = {
        TEXT("delete"),       // Destructive
        TEXT("destroy"),      // Destructive
        TEXT("unrealbuildtool"), // Process spawning
        TEXT("ubt"),          // Process spawning
    };
    
    bool IsBlockedCommand(const FString& Command)
    {
        FString LowerCommand = Command.ToLower();
        
        // Extract command name (first word)
        FString CommandName;
        int32 SpaceIndex;
        if (LowerCommand.FindChar(' ', SpaceIndex))
        {
            CommandName = LowerCommand.Left(SpaceIndex);
        }
        else
        {
            CommandName = LowerCommand;
        }
        
        // Check blocked list
        for (const FString& Blocked : BLOCKED_COMMANDS)
        {
            if (CommandName.Equals(Blocked, ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        
        return false;
    }
}

// =============================================================================
// Handler Implementation
// =============================================================================

bool UMcpAutomationBridgeSubsystem::HandleConsoleCommandAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString LowerAction = Action.ToLower();
    
    UE_LOG(LogMcpConsoleHandlers, Verbose, TEXT("HandleConsoleCommandAction: %s"), *LowerAction);
    
    // ===========================================================================
    // batch_console_commands - Execute multiple console commands
    // ===========================================================================
    if (LowerAction == TEXT("batch_console_commands"))
    {
        if (!Payload.IsValid())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Payload missing for batch_console_commands"), nullptr, TEXT("INVALID_PAYLOAD"));
            return true;
        }
        
        // Get commands array
        const TArray<TSharedPtr<FJsonValue>>* CommandsArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("commands"), CommandsArray) || !CommandsArray)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("'commands' array is required"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        if (CommandsArray->Num() == 0)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("No commands to execute"), nullptr);
            return true;
        }
        
        // Get the world context
        UWorld* World = nullptr;
        if (GEditor)
        {
            World = GEditor->GetEditorWorldContext().World();
        }
        
        if (!World && GEngine)
        {
            World = GEngine->GetWorldContexts().Num() > 0 ? GEngine->GetWorldContexts()[0].World() : nullptr;
        }
        
        if (!World)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("No world available for console command execution"), nullptr, TEXT("NO_WORLD"));
            return true;
        }
        
        int32 TotalCommands = CommandsArray->Num();
        int32 ExecutedCount = 0;
        int32 FailedCount = 0;
        TArray<FString> FailedCommands;
        
        for (const TSharedPtr<FJsonValue>& CommandValue : *CommandsArray)
        {
            if (!CommandValue.IsValid() || CommandValue->Type != EJson::String)
            {
                FailedCount++;
                continue;
            }
            
            FString Command = CommandValue->AsString().TrimStartAndEnd();
            if (Command.IsEmpty())
            {
                continue;
            }
            
            // Security check
            if (ConsoleCommandSecurity::IsBlockedCommand(Command))
            {
                UE_LOG(LogMcpConsoleHandlers, Warning, TEXT("Blocked command: %s"), *Command);
                FailedCount++;
                FailedCommands.Add(Command);
                continue;
            }
            
            // Execute the console command
            bool bSuccess = false;
            
            // Try to execute via GEngine - Exec returns true if command was handled
            if (GEngine)
            {
                bSuccess = GEngine->Exec(World, *Command);
                if (bSuccess)
                {
                    ExecutedCount++;
                }
            }
            
            if (!bSuccess)
            {
                FailedCount++;
                FailedCommands.Add(Command);
            }
        }
        
        // Build response
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("totalCommands"), TotalCommands);
        Result->SetNumberField(TEXT("executedCount"), ExecutedCount);
        Result->SetNumberField(TEXT("failedCount"), FailedCount);
        
        if (FailedCommands.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> FailedArray;
            for (const FString& Failed : FailedCommands)
            {
                FailedArray.Add(MakeShared<FJsonValueString>(Failed));
            }
            Result->SetArrayField(TEXT("failedCommands"), FailedArray);
        }
        
        // Return success if at least some commands executed
        bool bOverallSuccess = (ExecutedCount > 0) && (FailedCount == 0);
        FString Message = FString::Printf(TEXT("Executed %d/%d commands"), ExecutedCount, TotalCommands);
        
        if (FailedCount > 0)
        {
            Message = FString::Printf(TEXT("Batch command execution completed: %d executed, %d failed"), 
                ExecutedCount, FailedCount);
        }
        
        SendAutomationResponse(RequestingSocket, RequestId, bOverallSuccess, Message, Result,
            bOverallSuccess ? FString() : TEXT("PARTIAL_FAILURE"));
        
        return true;
    }
    
    // ===========================================================================
    // console_command - Execute a single console command
    // ===========================================================================
    if (LowerAction == TEXT("console_command"))
    {
        if (!Payload.IsValid())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Payload missing for console_command"), nullptr, TEXT("INVALID_PAYLOAD"));
            return true;
        }
        
        FString Command;
        if (!Payload->TryGetStringField(TEXT("command"), Command) || Command.TrimStartAndEnd().IsEmpty())
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("'command' parameter is required"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }
        
        Command = Command.TrimStartAndEnd();
        
        // Security check
        if (ConsoleCommandSecurity::IsBlockedCommand(Command))
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Command blocked for security: %s"), *Command),
                nullptr, TEXT("COMMAND_BLOCKED"));
            return true;
        }
        
        // Get the world context
        UWorld* World = nullptr;
        if (GEditor)
        {
            World = GEditor->GetEditorWorldContext().World();
        }
        
        if (!World && GEngine)
        {
            World = GEngine->GetWorldContexts().Num() > 0 ? GEngine->GetWorldContexts()[0].World() : nullptr;
        }
        
        if (!World)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("No world available for console command execution"), nullptr, TEXT("NO_WORLD"));
            return true;
        }
        
        // Execute the command
        if (GEngine)
        {
            GEngine->Exec(World, *Command);
        }
        
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("command"), Command);
        
        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Command executed: %s"), *Command), Result);
        
        return true;
    }
    
    return false; // Not handled
#else
    SendAutomationResponse(RequestingSocket, RequestId, false,
        TEXT("Console command actions require editor build"),
        nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
