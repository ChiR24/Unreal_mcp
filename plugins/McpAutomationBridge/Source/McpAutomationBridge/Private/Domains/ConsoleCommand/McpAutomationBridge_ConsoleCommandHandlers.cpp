#include "Core/Compatibility/McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandHandlersPrivate.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#endif

DEFINE_LOG_CATEGORY(LogMcpConsoleHandlers);

namespace ConsoleCommandSecurity
{
    // One canonical console-command policy, generated from the TypeScript typed
    // rule data by scripts/generate-console-command-policy.ts (Task 22). The
    // handwritten block lists were removed; this namespace now consumes the
    // generated arrays so both transports share one fail-closed policy.
    #include "Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandPolicy.generated.h"

    static bool ContainsUnsafeSeparator(const FString& Command)
    {
        for (const TCHAR* const Separator : McpGeneratedConsoleCommandPolicy::UNSAFE_SEPARATORS)
        {
            if (Command.Contains(Separator))
            {
                return true;
            }
        }
        return false;
    }

    static bool IsListedCommandName(const FString& CommandName, const TCHAR* const* Names, int32 Count)
    {
        for (int32 Index = 0; Index < Count; ++Index)
        {
            if (CommandName.Equals(Names[Index], ESearchCase::IgnoreCase))
            {
                return true;
            }
        }
        return false;
    }

    bool IsBlockedCommand(const FString& Command)
    {
        FString LowerCommand = Command.TrimStartAndEnd().ToLower();
        if (LowerCommand.IsEmpty())
        {
            return false;
        }

        if (ContainsUnsafeSeparator(LowerCommand))
        {
            return true;
        }

        TArray<FString> CommandParts;
        LowerCommand.ParseIntoArrayWS(CommandParts);
        if (CommandParts.Num() == 0)
        {
            return false;
        }
        const FString& CommandName = CommandParts[0];

        // Check blocked/restricted/forbidden-name lists (py/python folded into BLOCKED).
        if (IsListedCommandName(CommandName, McpGeneratedConsoleCommandPolicy::BLOCKED_COMMANDS, UE_ARRAY_COUNT(McpGeneratedConsoleCommandPolicy::BLOCKED_COMMANDS)) ||
            IsListedCommandName(CommandName, McpGeneratedConsoleCommandPolicy::RESTRICTED_COMMANDS, UE_ARRAY_COUNT(McpGeneratedConsoleCommandPolicy::RESTRICTED_COMMANDS)) ||
            IsListedCommandName(CommandName, McpGeneratedConsoleCommandPolicy::FORBIDDEN_COMMAND_NAMES, UE_ARRAY_COUNT(McpGeneratedConsoleCommandPolicy::FORBIDDEN_COMMAND_NAMES)))
        {
            return true;
        }

        for (const TCHAR* Token : McpGeneratedConsoleCommandPolicy::FORBIDDEN_TOKENS)
        {
            if (LowerCommand.Contains(Token))
            {
                return true;
            }
        }

        return false;
    }
}

bool UMcpAutomationBridgeSubsystem::HandleConsoleCommandAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    FString LowerAction = Action.ToLower();

    UE_LOG(LogMcpConsoleHandlers, Verbose, TEXT("HandleConsoleCommandAction: %s"), *LowerAction);

    if (LowerAction == TEXT("batch_console_commands"))
    {
        return McpConsoleCommandHandlers::HandleBatchConsoleCommands(
            this, RequestId, Payload, RequestingSocket);
    }

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

        // Execute the command through the editor first, then engine fallback.
        bool bSuccess = false;
        if (GEditor)
        {
            bSuccess = GEditor->Exec(World, *Command);
        }

        if (!bSuccess && GEngine)
        {
            bSuccess = GEngine->Exec(World, *Command);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("command"), Command);
        Result->SetBoolField(TEXT("success"), bSuccess);

        SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
            bSuccess ? FString::Printf(TEXT("Command executed: %s"), *Command)
                     : FString::Printf(TEXT("Command not executed: %s"), *Command),
            Result, bSuccess ? FString() : TEXT("EXEC_FAILED"));

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
