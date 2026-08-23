#include "Core/Compatibility/McpVersionCompatibility.h"  // MUST BE FIRST - Version compatibility macros
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandHandlersPrivate.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "CoreGlobals.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/StringOutputDevice.h"
#endif

DEFINE_LOG_CATEGORY(LogMcpConsoleHandlers);

// Outer-result cap for captured console output (receipt DATA is further truncated to 2048 by redaction).
constexpr int32 kMaxConsoleOutputChars = 4096;

namespace ConsoleCommandSecurity
{
    // One canonical console-command policy, generated from the TypeScript typed
    // rule data by scripts/generate-console-command-policy.ts (Task 22). The
    // handwritten block lists were removed; this namespace now consumes the
    // generated arrays so both transports share one fail-closed policy.
    #include "Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandPolicy.generated.h"

    // B7 fail-closed guard: if the policy generator emits empty arrays (broken
    // build, stale header, or hand-edit), every check below silently allows
    // everything. These asserts make that impossible at compile time.
    static_assert(UE_ARRAY_COUNT(McpGeneratedConsoleCommandPolicy::BLOCKED_COMMANDS) > 0,
        "Generated console-command policy must define at least one BLOCKED_COMMAND");
    static_assert(UE_ARRAY_COUNT(McpGeneratedConsoleCommandPolicy::RESTRICTED_COMMANDS) > 0,
        "Generated console-command policy must define at least one RESTRICTED_COMMAND");
    static_assert(UE_ARRAY_COUNT(McpGeneratedConsoleCommandPolicy::FORBIDDEN_TOKENS) > 0,
        "Generated console-command policy must define at least one FORBIDDEN_TOKEN");

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

    // B1: Mirror UE's FParse::Command name matching. The engine accepts a
    // command-name match when the name is a case-insensitive prefix AND the
    // next character is non-alphanumeric (or end of string). The old
    // ParseIntoArrayWS + exact-Equals tokenizer only split on whitespace,
    // so appending a non-whitespace non-alnum char (e.g. "quit.") bypassed
    // the block while UE's exec still matched and ran "quit".
    static bool CommandNameMatches(const FString& LowerCommand, const TCHAR* Name)
    {
        const int32 NameLen = FCString::Strlen(Name);
        if (NameLen == 0 || LowerCommand.Len() < NameLen)
        {
            return false;
        }
        for (int32 i = 0; i < NameLen; ++i)
        {
            const TCHAR CmdCh = LowerCommand[i];
            const TCHAR NameCh = Name[i];
            const TCHAR CmdLower = (CmdCh >= 'A' && CmdCh <= 'Z') ? (CmdCh + ('a' - 'A')) : CmdCh;
            const TCHAR NameLower = (NameCh >= 'A' && NameCh <= 'Z') ? (NameCh + ('a' - 'A')) : NameCh;
            if (CmdLower != NameLower)
            {
                return false;
            }
        }
        if (LowerCommand.Len() > NameLen)
        {
            const TCHAR NextChar = LowerCommand[NameLen];
            const bool bIsAlnum = (NextChar >= '0' && NextChar <= '9') ||
                (NextChar >= 'A' && NextChar <= 'Z') || (NextChar >= 'a' && NextChar <= 'z');
            if (bIsAlnum)
            {
                return false;
            }
        }
        return true;
    }

    static bool IsListedCommandName(const FString& Command, const TCHAR* const* Names, int32 Count)
    {
        for (int32 Index = 0; Index < Count; ++Index)
        {
            if (CommandNameMatches(Command, Names[Index]))
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

        // B1: Prefix-match against the full lowercased command string instead
        // of splitting on whitespace. CommandNameMatches mirrors UE's
        // FParse::Command so "quit." and "quit=x" are blocked just like "quit".
        if (IsListedCommandName(LowerCommand, McpGeneratedConsoleCommandPolicy::BLOCKED_COMMANDS, UE_ARRAY_COUNT(McpGeneratedConsoleCommandPolicy::BLOCKED_COMMANDS)) ||
            IsListedCommandName(LowerCommand, McpGeneratedConsoleCommandPolicy::RESTRICTED_COMMANDS, UE_ARRAY_COUNT(McpGeneratedConsoleCommandPolicy::RESTRICTED_COMMANDS)) ||
            IsListedCommandName(LowerCommand, McpGeneratedConsoleCommandPolicy::FORBIDDEN_COMMAND_NAMES, UE_ARRAY_COUNT(McpGeneratedConsoleCommandPolicy::FORBIDDEN_COMMAND_NAMES)))
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
    if (!IsInGameThread())
    {
        SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("Console command execution requires the game thread"),
            nullptr, TEXT("WRONG_THREAD"));
        return true;
    }

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
                TEXT("Command blocked for security"),
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

        // Execute the command through the editor first, then engine fallback,
        // capturing bounded output into an FStringOutputDevice.
        FStringOutputDevice OutputCapture;
        OutputCapture.SetAutoEmitLineTerminator(false);

        bool bHandled = false;
        if (GEditor)
        {
            bHandled = GEditor->Exec(World, *Command, OutputCapture);
        }

        if (!bHandled && GEngine)
        {
            bHandled = GEngine->Exec(World, *Command, OutputCapture);
        }

        FString BoundedOutput = OutputCapture.TrimStartAndEnd();
        if (BoundedOutput.Len() > kMaxConsoleOutputChars)
        {
            int32 CutPos = kMaxConsoleOutputChars;
            // Avoid splitting a UTF-16 surrogate pair (high surrogates: 0xD800-0xDBFF).
            if (CutPos > 0 && CutPos < BoundedOutput.Len() &&
                static_cast<uint16>(BoundedOutput[CutPos - 1]) >= 0xD800 &&
                static_cast<uint16>(BoundedOutput[CutPos - 1]) <= 0xDBFF)
            {
                --CutPos;
            }
            BoundedOutput = BoundedOutput.Left(CutPos);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("command"), Command);
        Result->SetBoolField(TEXT("success"), bHandled);
        Result->SetStringField(TEXT("output"), BoundedOutput);

        SendAutomationResponse(RequestingSocket, RequestId, bHandled,
            bHandled ? FString::Printf(TEXT("Command executed: %s"), *Command)
                     : FString::Printf(TEXT("Command not executed: %s"), *Command),
            Result, bHandled ? FString() : TEXT("EXEC_FAILED"));

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
