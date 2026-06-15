#include "Acp/Client/McpOpenCodeAcpClientPermissionGitCommands.h"

#include "Acp/Client/McpOpenCodeAcpClientPermissionCommandTokens.h"
#include "Misc/Paths.h"

namespace UnrealAgent::OpenCodeAcp::PermissionGitCommands
{
namespace
{
bool IsGitExecutable(const FString& Token)
{
    return FPaths::GetCleanFilename(Token).Equals(
        TEXT("git"),
        ESearchCase::IgnoreCase);
}

bool IsShellExecutable(const FString& Token)
{
    const FString Executable = FPaths::GetCleanFilename(Token).ToLower();
    return Executable == TEXT("bash")
        || Executable == TEXT("dash")
        || Executable == TEXT("zsh")
        || Executable == TEXT("ksh")
        || Executable == TEXT("fish")
        || Executable == TEXT("sh")
        || Executable == TEXT("cmd")
        || Executable == TEXT("cmd.exe")
        || Executable == TEXT("pwsh")
        || Executable == TEXT("powershell")
        || Executable == TEXT("powershell.exe");
}

bool IsShellPayloadOption(const FString& Token)
{
    const FString LowerToken = Token.ToLower();
    return LowerToken == TEXT("/c")
        || (LowerToken.StartsWith(TEXT("-"))
            && LowerToken.EndsWith(TEXT("c")));
}

bool IsOptionWithSeparateValue(const FString& Option)
{
    return Option.Equals(TEXT("-c"), ESearchCase::IgnoreCase)
        || Option.Equals(TEXT("-C"), ESearchCase::CaseSensitive)
        || Option.Equals(TEXT("--git-dir"), ESearchCase::IgnoreCase)
        || Option.Equals(TEXT("--work-tree"), ESearchCase::IgnoreCase)
        || Option.Equals(TEXT("--namespace"), ESearchCase::IgnoreCase)
        || Option.Equals(TEXT("--exec-path"), ESearchCase::IgnoreCase);
}

bool IsAliasConfigWithValue(
    const FString& ConfigValue,
    const FString& ExpectedValuePrefix)
{
    const FString LowerValue = ConfigValue.ToLower();
    const int32 EqualsIndex = LowerValue.Find(TEXT("="));
    return EqualsIndex != INDEX_NONE
        && LowerValue.Left(EqualsIndex).StartsWith(TEXT("alias."))
        && LowerValue.Mid(EqualsIndex + 1).StartsWith(ExpectedValuePrefix);
}

bool IsInlineConfigOption(
    const FString& Option,
    FString& OutConfigValue)
{
    if (!Option.StartsWith(TEXT("-c="), ESearchCase::IgnoreCase))
    {
        return false;
    }
    OutConfigValue = Option.Mid(3);
    return true;
}

bool IsAliasConfigEnvironmentOption(const FString& Option)
{
    return Option.StartsWith(
        TEXT("--config-env=alias."),
        ESearchCase::IgnoreCase);
}
}

bool ContainsGitApplyCommand(const FString& Command)
{
    TArray<FString> Tokens;
    PermissionCommandTokens::TokenizeQuotedCommand(Command, Tokens);
    for (int32 ExecutableIndex = 0;
        ExecutableIndex < Tokens.Num();
        ++ExecutableIndex)
    {
        if (!IsGitExecutable(Tokens[ExecutableIndex]))
        {
            continue;
        }
        for (int32 Index = ExecutableIndex + 1; Index < Tokens.Num(); ++Index)
        {
            const FString& OptionOrCommand = Tokens[Index];
            if (OptionOrCommand.Equals(TEXT("apply"), ESearchCase::IgnoreCase))
            {
                return true;
            }
            if (OptionOrCommand.Equals(TEXT("-c"), ESearchCase::IgnoreCase))
            {
                if (Index + 1 < Tokens.Num()
                    && IsAliasConfigWithValue(
                        Tokens[Index + 1],
                        TEXT("apply")))
                {
                    return true;
                }
                ++Index;
                continue;
            }
            FString ConfigValue;
            if (IsInlineConfigOption(OptionOrCommand, ConfigValue))
            {
                if (IsAliasConfigWithValue(ConfigValue, TEXT("apply")))
                {
                    return true;
                }
                continue;
            }
            if (IsAliasConfigEnvironmentOption(OptionOrCommand))
            {
                return true;
            }
            if (IsOptionWithSeparateValue(OptionOrCommand))
            {
                ++Index;
                continue;
            }
            if (!OptionOrCommand.StartsWith(TEXT("-")))
            {
                break;
            }
        }
    }
    return false;
}

bool ContainsDestructiveGitCommandInternal(
    const FString& Command,
    const int32 Depth)
{
    if (Depth > 4)
    {
        return true;
    }
    TArray<FString> Tokens;
    PermissionCommandTokens::TokenizeQuotedCommand(Command, Tokens);
    for (int32 ExecutableIndex = 0;
        ExecutableIndex < Tokens.Num();
        ++ExecutableIndex)
    {
        if (IsShellExecutable(Tokens[ExecutableIndex]))
        {
            for (int32 Index = ExecutableIndex + 1;
                Index + 1 < Tokens.Num();
                ++Index)
            {
                if (IsShellPayloadOption(Tokens[Index])
                    && ContainsDestructiveGitCommandInternal(
                        Tokens[Index + 1],
                        Depth + 1))
                {
                    return true;
                }
            }
        }
        if (!IsGitExecutable(Tokens[ExecutableIndex]))
        {
            continue;
        }
        int32 Index = ExecutableIndex + 1;
        while (Index < Tokens.Num() && Tokens[Index].StartsWith(TEXT("-")))
        {
            const FString& Option = Tokens[Index];
            if (Option.Equals(TEXT("-c"), ESearchCase::IgnoreCase))
            {
                if (Index + 1 < Tokens.Num()
                    && IsAliasConfigWithValue(Tokens[Index + 1], TEXT("!")))
                {
                    return true;
                }
                Index += 2;
                continue;
            }
            FString ConfigValue;
            if (IsInlineConfigOption(Option, ConfigValue))
            {
                if (IsAliasConfigWithValue(ConfigValue, TEXT("!")))
                {
                    return true;
                }
                ++Index;
                continue;
            }
            if (IsAliasConfigEnvironmentOption(Option))
            {
                return true;
            }
            Index += IsOptionWithSeparateValue(Option)
                && !Option.Contains(TEXT("="))
                ? 2
                : 1;
        }
        if (Index >= Tokens.Num())
        {
            continue;
        }
        const FString Subcommand = Tokens[Index++].ToLower();
        TArray<FString> Arguments;
        for (; Index < Tokens.Num(); ++Index)
        {
            Arguments.Add(Tokens[Index].ToLower());
        }
        if (Subcommand == TEXT("apply"))
        {
            return true;
        }
        if (Subcommand == TEXT("reset")
            || Subcommand == TEXT("restore")
            || Subcommand == TEXT("checkout"))
        {
            return true;
        }
        if (Subcommand == TEXT("clean"))
        {
            const bool bDryRun = Arguments.ContainsByPredicate(
                [](const FString& Argument)
                {
                    return Argument == TEXT("--dry-run")
                        || (Argument.StartsWith(TEXT("-"))
                            && !Argument.StartsWith(TEXT("--"))
                            && Argument.Contains(TEXT("n")));
                });
            if (!bDryRun)
            {
                return true;
            }
        }
    }
    return false;
}
bool ContainsDestructiveGitCommand(const FString& Command)
{
    return ContainsDestructiveGitCommandInternal(Command, 0);
}
}
