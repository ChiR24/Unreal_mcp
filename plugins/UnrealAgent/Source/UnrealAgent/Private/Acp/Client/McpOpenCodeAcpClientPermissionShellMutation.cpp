#include "Acp/Client/McpOpenCodeAcpClientPermissionShellMutation.h"

#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionReadOnlyCommands.h"

namespace UnrealAgent::OpenCodeAcp::PermissionShellMutation
{
bool ContainsDestructiveInterpreterOperation(const FString& LowerCommand)
{
    const TCHAR* DestructiveImports[] = {
        TEXT("extract_archive"),
        TEXT("remove"),
        TEXT("removedirs"),
        TEXT("rmdir"),
        TEXT("rmtree"),
        TEXT("unlink"),
        TEXT("unpack_archive")
    };
    if (ContainsAliasedPythonImport(
        LowerCommand,
        DestructiveImports,
        UE_ARRAY_COUNT(DestructiveImports)))
    {
        return true;
    }

    const TCHAR* DestructiveMarkers[] = {
        TEXT("shutil.rmtree("),
        TEXT("os.remove("),
        TEXT("os.unlink("),
        TEXT("os.rmdir("),
        TEXT("os.removedirs("),
        TEXT(".unlink("),
        TEXT(".rmdir("),
        TEXT("fs.rmsync("),
        TEXT("fs.rmdirsync("),
        TEXT("fs.unlinksync("),
        TEXT("deno.remove("),
        TEXT("fileutils.rm_rf(")
    };
    for (const TCHAR* Marker : DestructiveMarkers)
    {
        if (LowerCommand.Contains(Marker))
        {
            return true;
        }
    }
    return LowerCommand.Contains(TEXT("remove-item"))
        && (LowerCommand.Contains(TEXT("-recurse"))
            || LowerCommand.Contains(TEXT("-force")));
}

bool ContainsDynamicShellMutation(const FString& Command)
{
    const FString LowerCommand = Command.ToLower();
    bool bHasAssignment = false;
    for (int32 Index = 0; Index < LowerCommand.Len(); ++Index)
    {
        const bool bAtBoundary =
            Index == 0
            || FChar::IsWhitespace(LowerCommand[Index - 1])
            || LowerCommand[Index - 1] == TEXT(';')
            || LowerCommand[Index - 1] == TEXT('&')
            || LowerCommand[Index - 1] == TEXT('|');
        if (!bAtBoundary
            || !(FChar::IsAlpha(LowerCommand[Index])
                || LowerCommand[Index] == TEXT('_')))
        {
            continue;
        }
        int32 Cursor = Index + 1;
        while (Cursor < LowerCommand.Len()
            && (FChar::IsAlnum(LowerCommand[Cursor])
                || LowerCommand[Cursor] == TEXT('_')))
        {
            ++Cursor;
        }
        while (Cursor < LowerCommand.Len()
            && FChar::IsWhitespace(LowerCommand[Cursor]))
        {
            ++Cursor;
        }
        if (Cursor < LowerCommand.Len()
            && LowerCommand[Cursor] == TEXT('='))
        {
            bHasAssignment = true;
            break;
        }
    }
    if (!bHasAssignment)
    {
        return false;
    }
    for (int32 Index = 0; Index + 1 < LowerCommand.Len(); ++Index)
    {
        if (LowerCommand[Index] != TEXT('$'))
        {
            continue;
        }
        const TCHAR Next = LowerCommand[Index + 1];
        if (FChar::IsAlpha(Next)
            || Next == TEXT('_')
            || Next == TEXT('{'))
        {
            return true;
        }
    }
    return false;
}

bool ContainsCommandSubstitutionMutation(const FString& Command)
{
    return (Command.Contains(TEXT("$(")) || Command.Contains(TEXT("`")))
        && !PermissionReadOnlyCommands::IsExplicitReadOnlyCommandText(Command);
}
}

namespace UnrealAgent::OpenCodeAcp
{
FString NormalizeShellForSafety(FString Command)
{
    Command.ReplaceInline(TEXT("''"), TEXT(""));
    Command.ReplaceInline(TEXT("\"\""), TEXT(""));
    Command.ReplaceInline(TEXT("'+'"), TEXT(""));
    Command.ReplaceInline(TEXT("\"+\""), TEXT(""));
    Command.ReplaceInline(TEXT("^"), TEXT(""));
    FString Normalized;
    Normalized.Reserve(Command.Len());
    for (int32 Index = 0; Index < Command.Len(); ++Index)
    {
        const TCHAR Character = Command[Index];
        const bool bEmbeddedQuote =
            (Character == TEXT('\'') || Character == TEXT('"'))
            && Index > 0
            && Index + 1 < Command.Len()
            && (FChar::IsAlnum(Command[Index - 1])
                || Command[Index - 1] == TEXT('_'))
            && (FChar::IsAlnum(Command[Index + 1])
                || Command[Index + 1] == TEXT('_'));
        if (!bEmbeddedQuote)
        {
            Normalized.AppendChar(Character);
        }
    }
    return Normalized;
}
}
