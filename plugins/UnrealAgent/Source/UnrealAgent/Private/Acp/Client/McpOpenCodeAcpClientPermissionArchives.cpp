#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"

#include "Misc/Paths.h"

namespace UnrealAgent::OpenCodeAcp
{
bool ContainsTarExtractionCommand(const FString& LowerCommand)
{
    TArray<FString> Tokens;
    LowerCommand.ParseIntoArrayWS(Tokens);
    for (int32 ExecutableIndex = 0; ExecutableIndex + 1 < Tokens.Num(); ++ExecutableIndex)
    {
        const FString Executable =
            FPaths::GetCleanFilename(Tokens[ExecutableIndex].TrimQuotes()).ToLower();
        if (!Executable.Equals(TEXT("tar"))
            && !Executable.Equals(TEXT("bsdtar"))
            && !Executable.Equals(TEXT("gtar")))
        {
            continue;
        }

        const FString FirstOption = Tokens[ExecutableIndex + 1].TrimQuotes().ToLower();
        if (!FirstOption.StartsWith(TEXT("-")) && FirstOption.StartsWith(TEXT("x")))
        {
            return true;
        }
        for (int32 OptionIndex = ExecutableIndex + 1; OptionIndex < Tokens.Num(); ++OptionIndex)
        {
            const FString Option = Tokens[OptionIndex].TrimQuotes().ToLower();
            if (Option.Equals(TEXT("--extract"))
                || (Option.StartsWith(TEXT("-"))
                    && !Option.StartsWith(TEXT("--"))
                    && Option.Contains(TEXT("x"))))
            {
                return true;
            }
        }
    }
    return false;
}

bool ContainsArchiveExtractionOperation(const FString& LowerCommand)
{
    return ContainsTarExtractionCommand(LowerCommand)
        || LowerCommand.StartsWith(TEXT("unzip "))
        || LowerCommand.Contains(TEXT(" unzip "))
        || LowerCommand.StartsWith(TEXT("7z x "))
        || LowerCommand.Contains(TEXT(" 7z x "))
        || (LowerCommand.Contains(TEXT("python"))
            && LowerCommand.Contains(TEXT("-m tarfile"))
            && (LowerCommand.Contains(TEXT(" -e "))
                || LowerCommand.Contains(TEXT(" --extract "))))
        || ((LowerCommand.Contains(TEXT("tarfile"))
                || LowerCommand.Contains(TEXT("zipfile")))
            && LowerCommand.Contains(TEXT(".extractall(")))
        || LowerCommand.Contains(TEXT("shutil.unpack_archive("))
        || LowerCommand.Contains(TEXT("patoolib.extract_archive("))
        || ((LowerCommand.Contains(TEXT("rarfile"))
                || LowerCommand.Contains(TEXT("py7zr")))
            && LowerCommand.Contains(TEXT(".extractall(")));
}
}
