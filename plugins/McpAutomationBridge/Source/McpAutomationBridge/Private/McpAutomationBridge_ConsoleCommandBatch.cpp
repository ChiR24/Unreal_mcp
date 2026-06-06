#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_ConsoleCommandHandlersPrivate.h"

#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#endif

namespace McpConsoleCommandHandlers
{

bool HandleBatchConsoleCommands(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!Payload.IsValid())
    {
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("Payload missing for batch_console_commands"), nullptr, TEXT("INVALID_PAYLOAD"));
        return true;
    }

    // Get commands array
    const TArray<TSharedPtr<FJsonValue>>* CommandsArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("commands"), CommandsArray) || !CommandsArray)
    {
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("'commands' array is required"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (CommandsArray->Num() == 0)
    {
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true,
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
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false,
            TEXT("No world available for console command execution"), nullptr, TEXT("NO_WORLD"));
        return true;
    }

    int32 TotalCommands = CommandsArray->Num();
    int32 ExecutedCount = 0;
    int32 FailedCount = 0;
    TArray<FString> FailedCommands;

    for (const TSharedPtr<FJsonValue>& CommandValue : *CommandsArray)
    {
        FString Command;
        if (CommandValue.IsValid() && CommandValue->Type == EJson::String)
        {
            Command = CommandValue->AsString().TrimStartAndEnd();
        }
        else if (CommandValue.IsValid() && CommandValue->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> CommandObject = CommandValue->AsObject();
            if (CommandObject.IsValid())
            {
                CommandObject->TryGetStringField(TEXT("command"), Command);
                if (Command.IsEmpty())
                {
                    CommandObject->TryGetStringField(TEXT("cmd"), Command);
                }
                Command.TrimStartAndEndInline();
            }
        }
        else
        {
            FailedCount++;
            continue;
        }

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

        // Try to execute via editor first; Exec returns true if command was handled.
        if (GEditor)
        {
            bSuccess = GEditor->Exec(World, *Command);
        }

        if (!bSuccess && GEngine)
        {
            bSuccess = GEngine->Exec(World, *Command);
        }

        if (bSuccess)
        {
            ExecutedCount++;
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

    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, bOverallSuccess, Message, Result,
        bOverallSuccess ? FString() : TEXT("PARTIAL_FAILURE"));

    return true;
}

}
