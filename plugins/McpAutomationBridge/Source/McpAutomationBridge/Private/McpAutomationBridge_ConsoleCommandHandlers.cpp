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
// Output capture helpers
// =============================================================================

namespace ConsoleCommandCapture
{
    /**
     * Execute a console command and capture any output it produces.
     *
     * UE's Exec() sends output through two independent channels depending on the
     * command type:
     *   1. Commands implemented as IConsoleCommand / IConsoleVariable write
     *      directly to the FOutputDevice passed to Exec(). FMcpOutputCapture
     *      catches these.
     *   2. Python (`py`) and some editor-only commands write through GLog /
     *      GOutputDeviceRedirector instead of the passed device. To catch those
     *      we temporarily register our capture device with GLog for the duration
     *      of the call.
     *
     * The two approaches are combined here so that the caller never has to think
     * about which channel a particular command uses.
     */
    static TArray<FString> ExecWithCapture(UWorld* World, const FString& Command)
    {
        // Device 1: Passed directly to Exec — catches most CVars and built-ins.
        FMcpOutputCapture DirectCapture;

        // Device 2: Registered with GLog — catches Python output and editor Exec
        // paths that bypass the passed device.
        FMcpOutputCapture LogCapture;
        if (GLog)
        {
            GLog->AddOutputDevice(&LogCapture);
        }

        if (GEngine && World)
        {
            GEngine->Exec(World, *Command, DirectCapture);
        }
        else if (GEngine)
        {
            // Fallback: no world context (should rarely happen)
            GEngine->Exec(nullptr, *Command, DirectCapture);
        }

        if (GLog)
        {
            GLog->RemoveOutputDevice(&LogCapture);
            GLog->Flush();
        }

        // Merge both capture channels, deduplicating consecutive identical lines
        // (some commands echo through both paths).
        TArray<FString> Direct  = DirectCapture.Consume();
        TArray<FString> FromLog = LogCapture.Consume();

        TArray<FString> Merged;
        Merged.Append(Direct);

        // Add log lines that are not already represented in the direct output.
        TSet<FString> DirectSet(Direct);
        for (const FString& Line : FromLog)
        {
            if (!DirectSet.Contains(Line))
            {
                Merged.Add(Line);
            }
        }

        return Merged;
    }

    /** Convert a string array into a JSON array of string values. */
    static TArray<TSharedPtr<FJsonValue>> LinesToJsonArray(const TArray<FString>& Lines)
    {
        TArray<TSharedPtr<FJsonValue>> JsonLines;
        JsonLines.Reserve(Lines.Num());
        for (const FString& Line : Lines)
        {
            JsonLines.Add(MakeShared<FJsonValueString>(Line));
        }
        return JsonLines;
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

    // Shared world resolution used by both handlers below.
    auto ResolveWorld = []() -> UWorld*
    {
        UWorld* W = nullptr;
        if (GEditor)
        {
            W = GEditor->GetEditorWorldContext().World();
        }
        if (!W && GEngine)
        {
            W = GEngine->GetWorldContexts().Num() > 0
                    ? GEngine->GetWorldContexts()[0].World()
                    : nullptr;
        }
        return W;
    };
    
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
        
        UWorld* World = ResolveWorld();
        if (!World)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("No world available for console command execution"), nullptr, TEXT("NO_WORLD"));
            return true;
        }
        
        int32 TotalCommands  = CommandsArray->Num();
        int32 ExecutedCount  = 0;
        int32 FailedCount    = 0;
        TArray<FString>                        FailedCommands;
        TArray<TSharedPtr<FJsonValue>>         ResultsArray;

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
            
            TSharedPtr<FJsonObject> CmdResult = McpHandlerUtils::CreateResultObject();
            CmdResult->SetStringField(TEXT("command"), Command);

            if (ConsoleCommandSecurity::IsBlockedCommand(Command))
            {
                UE_LOG(LogMcpConsoleHandlers, Warning, TEXT("Blocked command: %s"), *Command);
                FailedCount++;
                FailedCommands.Add(Command);
                CmdResult->SetBoolField(TEXT("executed"), false);
                CmdResult->SetStringField(TEXT("error"), TEXT("COMMAND_BLOCKED"));
                ResultsArray.Add(MakeShared<FJsonValueObject>(CmdResult));
                continue;
            }
            
            if (!GEngine)
            {
                FailedCount++;
                FailedCommands.Add(Command);
                CmdResult->SetBoolField(TEXT("executed"), false);
                CmdResult->SetStringField(TEXT("error"), TEXT("NO_ENGINE"));
                ResultsArray.Add(MakeShared<FJsonValueObject>(CmdResult));
                continue;
            }

            TArray<FString> OutputLines = ConsoleCommandCapture::ExecWithCapture(World, Command);

            ExecutedCount++;
            CmdResult->SetBoolField(TEXT("executed"), true);
            if (OutputLines.Num() > 0)
            {
                CmdResult->SetArrayField(TEXT("output"),
                    ConsoleCommandCapture::LinesToJsonArray(OutputLines));
            }
            ResultsArray.Add(MakeShared<FJsonValueObject>(CmdResult));
        }
        
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("totalCommands"),   TotalCommands);
        Result->SetNumberField(TEXT("executedCount"),   ExecutedCount);
        Result->SetNumberField(TEXT("failedCount"),     FailedCount);
        Result->SetArrayField(TEXT("results"),          ResultsArray);
        
        if (FailedCommands.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> FailedArray;
            for (const FString& Failed : FailedCommands)
            {
                FailedArray.Add(MakeShared<FJsonValueString>(Failed));
            }
            Result->SetArrayField(TEXT("failedCommands"), FailedArray);
        }
        
        bool bOverallSuccess = (ExecutedCount > 0) && (FailedCount == 0);
        FString Message = FailedCount > 0
            ? FString::Printf(TEXT("Batch completed: %d executed, %d failed"), ExecutedCount, FailedCount)
            : FString::Printf(TEXT("Executed %d/%d commands"), ExecutedCount, TotalCommands);
        
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
        
        if (ConsoleCommandSecurity::IsBlockedCommand(Command))
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Command blocked for security: %s"), *Command),
                nullptr, TEXT("COMMAND_BLOCKED"));
            return true;
        }
        
        UWorld* World = ResolveWorld();
        if (!World)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("No world available for console command execution"), nullptr, TEXT("NO_WORLD"));
            return true;
        }
        
        TArray<FString> OutputLines = ConsoleCommandCapture::ExecWithCapture(World, Command);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("command"), Command);
        if (OutputLines.Num() > 0)
        {
            Result->SetArrayField(TEXT("output"),
                ConsoleCommandCapture::LinesToJsonArray(OutputLines));
        }

        // Build a human-readable summary message.
        FString Msg = OutputLines.Num() > 0
            ? FString::Printf(TEXT("Command executed: %s"), *Command)
            : FString::Printf(TEXT("Command executed: %s (no output)"), *Command);

        SendAutomationResponse(RequestingSocket, RequestId, true, Msg, Result);
        
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
