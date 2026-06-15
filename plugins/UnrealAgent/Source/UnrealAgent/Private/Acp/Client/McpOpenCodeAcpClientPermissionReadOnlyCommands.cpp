#include "Acp/Client/McpOpenCodeAcpClientPermissionReadOnlyCommands.h"

#include "Acp/Client/McpOpenCodeAcpClientPermissionCommandTokens.h"
#include "Misc/Paths.h"

namespace UnrealAgent::OpenCodeAcp::PermissionReadOnlyCommands
{
namespace
{
bool ContainsReadOnlyCommandEscape(const FString& Value)
{
    const FString LowerValue = Value.ToLower();
    const TCHAR* EscapeMarkers[] = {
        TEXT("|"), TEXT("&"), TEXT(";"), TEXT(">"), TEXT("<"), TEXT("`"),
        TEXT("$("), TEXT("&&"), TEXT("||"), TEXT("-delete"), TEXT("-exec"),
        TEXT("-execdir"), TEXT("-ok"), TEXT("-okdir")
    };
    for (const TCHAR* Marker : EscapeMarkers)
    {
        if (LowerValue.Contains(Marker))
        {
            return true;
        }
    }
    return false;
}

bool IsShellExecutable(const FString& Executable)
{
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
}

bool IsExplicitReadOnlyCommandText(const FString& Value)
{
    FString Normalized = Value.TrimStartAndEnd();
    Normalized.ReplaceInline(TEXT("\t"), TEXT(" "));
    if (Normalized.IsEmpty()
        || Normalized.Contains(TEXT("\n"))
        || ContainsReadOnlyCommandEscape(Normalized))
    {
        return false;
    }

    FString Executable;
    if (!Normalized.Split(TEXT(" "), &Executable, nullptr))
    {
        Executable = Normalized;
    }
    Executable = FPaths::GetCleanFilename(Executable).ToLower();
    const TSet<FString> ReadOnlyExecutables = {
        TEXT("rg"), TEXT("grep"), TEXT("fd"), TEXT("ls"), TEXT("tree"),
        TEXT("pwd"), TEXT("realpath"), TEXT("readlink"), TEXT("stat"),
        TEXT("file"), TEXT("head"), TEXT("tail"), TEXT("cat")
    };
    if (!ReadOnlyExecutables.Contains(Executable))
    {
        return false;
    }

    TArray<FString> Tokens;
    Normalized.ParseIntoArrayWS(Tokens);
    for (int32 Index = 1; Index < Tokens.Num(); ++Index)
    {
        const FString Option = Tokens[Index].TrimQuotes().ToLower();
        if (Executable == TEXT("rg")
            && (Option == TEXT("--pre")
                || Option.StartsWith(TEXT("--pre="))
                || Option == TEXT("--hostname-bin")
                || Option.StartsWith(TEXT("--hostname-bin="))))
        {
            return false;
        }
        if (Executable == TEXT("fd")
            && (Option == TEXT("-x")
                || Option == TEXT("-X")
                || Option == TEXT("--exec")
                || Option.StartsWith(TEXT("--exec="))
                || Option == TEXT("--exec-batch")
                || Option.StartsWith(TEXT("--exec-batch="))))
        {
            return false;
        }
        if (Executable == TEXT("tree")
            && (Option == TEXT("-o")
                || Option == TEXT("--output")
                || Option.StartsWith(TEXT("--output="))))
        {
            return false;
        }
    }
    return true;
}

bool ContainsExecutionCapableReadOption(const FString& Command)
{
    TArray<FString> Tokens;
    PermissionCommandTokens::TokenizeQuotedCommand(Command, Tokens);
    for (int32 ExecutableIndex = 0;
        ExecutableIndex < Tokens.Num();
        ++ExecutableIndex)
    {
        const FString Executable =
            FPaths::GetCleanFilename(Tokens[ExecutableIndex]).ToLower();
        if (IsShellExecutable(Executable))
        {
            for (int32 OptionIndex = ExecutableIndex + 1;
                OptionIndex + 1 < Tokens.Num();
                ++OptionIndex)
            {
                const FString Option = Tokens[OptionIndex].ToLower();
                if ((Option.StartsWith(TEXT("-")) && Option.EndsWith(TEXT("c")))
                    || Option == TEXT("/c"))
                {
                    if (ContainsExecutionCapableReadOption(
                            Tokens[OptionIndex + 1]))
                    {
                        return true;
                    }
                    break;
                }
            }
        }
        for (int32 OptionIndex = ExecutableIndex + 1;
            OptionIndex < Tokens.Num();
            ++OptionIndex)
        {
            const FString Option = Tokens[OptionIndex].ToLower();
            if (Executable == TEXT("rg")
                && (Option == TEXT("--pre")
                    || Option.StartsWith(TEXT("--pre="))
                    || Option == TEXT("--hostname-bin")
                    || Option.StartsWith(TEXT("--hostname-bin="))))
            {
                return true;
            }
            if (Executable == TEXT("fd")
                && (Option == TEXT("-x")
                    || Option == TEXT("-X")
                    || Option == TEXT("--exec")
                    || Option.StartsWith(TEXT("--exec="))
                    || Option == TEXT("--exec-batch")
                    || Option.StartsWith(TEXT("--exec-batch="))))
            {
                return true;
            }
            if (Executable == TEXT("tree")
                && (Option == TEXT("-o")
                    || Option == TEXT("--output")
                    || Option.StartsWith(TEXT("--output="))))
            {
                return true;
            }
        }
    }
    return false;
}
}
