#include "Acp/Client/McpOpenCodeAcpClient.h"
#include "Acp/Client/McpOpenCodeAcpClientPrivate.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

using namespace UnrealAgent::OpenCodeAcp;

namespace
{
    FString ReadCommandDescription(const FString& FilePath)
    {
        FString FileText;
        if (!FFileHelper::LoadFileToString(FileText, *FilePath))
        {
            return FString();
        }

        TArray<FString> Lines;
        FileText.ParseIntoArrayLines(Lines, false);
        for (const FString& Line : Lines)
        {
            FString Key;
            FString Value;
            if (Line.Split(TEXT(":"), &Key, &Value) && Key.TrimStartAndEnd() == TEXT("description"))
            {
                return Value.TrimStartAndEnd();
            }
        }
        return FString();
    }
}

void FOpenCodeAcpClient::AddAvailableCommand(const FString& Name, const FString& Description, const FString& InputHint)
{
    const FString CleanName = Name.TrimStartAndEnd().TrimChar(TEXT('/'));
    if (CleanName.IsEmpty())
    {
        return;
    }

    FOpenCodeAcpCommandOption* Existing = AvailableCommands.FindByPredicate([&CleanName](const FOpenCodeAcpCommandOption& Option)
    {
        return Option.Name == CleanName;
    });
    if (Existing != nullptr)
    {
        Existing->Description = Description;
        Existing->InputHint = InputHint;
        return;
    }

    FOpenCodeAcpCommandOption Command;
    Command.Name = CleanName;
    Command.Description = Description;
    Command.InputHint = InputHint;
    AvailableCommands.Add(Command);
}

void FOpenCodeAcpClient::LoadStudioKitCommandSummariesFromDisk()
{
    const FString CommandsDirectory = FPaths::Combine(WorkingDirectory, TEXT(".opencode"), TEXT("commands"));
    TArray<FString> CommandFiles;
    IFileManager::Get().FindFiles(CommandFiles, *FPaths::Combine(CommandsDirectory, TEXT("*.md")), true, false);
    CommandFiles.Sort();

    for (const FString& CommandFile : CommandFiles)
    {
        const FString CommandPath = FPaths::Combine(CommandsDirectory, CommandFile);
        const FString BaseName = FPaths::GetBaseFilename(CommandFile);
        AddAvailableCommand(BaseName, ReadCommandDescription(CommandPath));
    }
}

void FOpenCodeAcpClient::HandleAvailableCommandsUpdate(const TSharedPtr<FJsonObject>& Update)
{
    const TArray<TSharedPtr<FJsonValue>>* Commands = nullptr;
    if (!Update->TryGetArrayField(TEXT("availableCommands"), Commands) || Commands == nullptr)
    {
        return;
    }

    // ACP semantic: each `available_commands_update` is the *complete* set of commands
    // the agent currently exposes. Replace the local cache rather than merging so that
    // commands removed by the agent stop appearing in the panel's slash suggestions.
    AvailableCommands.Reset();
    for (const TSharedPtr<FJsonValue>& CommandValue : *Commands)
    {
        const TSharedPtr<FJsonObject> CommandObject = CommandValue.IsValid()
            ? CommandValue->AsObject()
            : nullptr;
        if (!CommandObject.IsValid())
        {
            continue;
        }

        FString InputHint;
        const TSharedPtr<FJsonObject>* InputObject = nullptr;
        if (CommandObject->TryGetObjectField(TEXT("input"), InputObject) && InputObject != nullptr && InputObject->IsValid())
        {
            (*InputObject)->TryGetStringField(TEXT("hint"), InputHint);
        }

        AddAvailableCommand(
            GetStringFieldOrEmpty(CommandObject, TEXT("name")),
            GetStringFieldOrEmpty(CommandObject, TEXT("description")),
            InputHint);
    }
}
