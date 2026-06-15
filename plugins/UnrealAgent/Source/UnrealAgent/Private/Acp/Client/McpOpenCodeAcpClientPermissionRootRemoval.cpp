#include "Acp/Client/McpOpenCodeAcpClientPermissionCommandTokens.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"

#include "Misc/Paths.h"

namespace UnrealAgent::OpenCodeAcp
{
namespace
{
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

    bool IsProjectRootTarget(
        FString Token,
        const FString& WorkingDirectory)
    {
        Token = Token.TrimQuotes();
        Token.ReplaceInline(TEXT("\\"), TEXT("/"));
        while (Token.Len() > 1 && Token.EndsWith(TEXT("/")))
        {
            Token.LeftChopInline(1);
        }
        const FString LowerToken = Token.ToLower();
        if (LowerToken == TEXT(".")
            || LowerToken == TEXT("*")
            || LowerToken == TEXT("..")
            || LowerToken == TEXT("../*")
            || LowerToken == TEXT("/")
            || LowerToken == TEXT("$pwd")
            || LowerToken == TEXT("$pwd/*")
            || LowerToken == TEXT("${pwd}")
            || LowerToken == TEXT("${pwd}/*"))
        {
            return true;
        }
        if (Token.Contains(TEXT("$"))
            || Token.Contains(TEXT("*"))
            || Token.Contains(TEXT("?"))
            || Token.Contains(TEXT("[")))
        {
            return false;
        }
        const FString FullTarget = FPaths::IsRelative(Token)
            ? FPaths::ConvertRelativePathToFull(
                FPaths::Combine(WorkingDirectory, Token))
            : FPaths::ConvertRelativePathToFull(Token);
        return FPaths::IsSamePath(
            FullTarget,
            FPaths::ConvertRelativePathToFull(WorkingDirectory));
    }
}

bool ContainsDestructiveRecursiveRemove(
    const FString& Command,
    const FString& WorkingDirectory,
    const int32 Depth)
{
    if (Depth > 4)
    {
        return true;
    }
    TArray<FString> Tokens;
    PermissionCommandTokens::TokenizeQuotedCommand(Command, Tokens);
    for (int32 ExecutableIndex = 0; ExecutableIndex < Tokens.Num(); ++ExecutableIndex)
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
                    if (ContainsDestructiveRecursiveRemove(
                            Tokens[OptionIndex + 1],
                            WorkingDirectory,
                            Depth + 1))
                    {
                        return true;
                    }
                    break;
                }
            }
        }
        if (Executable != TEXT("rm"))
        {
            continue;
        }
        for (int32 Index = ExecutableIndex + 1; Index < Tokens.Num(); ++Index)
        {
            const FString& Token = Tokens[Index];
            if (!Token.StartsWith(TEXT("-"))
                && IsProjectRootTarget(Token, WorkingDirectory))
            {
                return true;
            }
        }
    }
    return false;
}
}
