#include "Acp/Client/McpOpenCodeAcpClientPermissionSemantics.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"

namespace UnrealAgent::OpenCodeAcp::PermissionSemantics
{
namespace
{
bool IsInterpreterCommand(const FString& LowerValue)
{
    FString TokenizedValue = LowerValue;
    for (const TCHAR Boundary : FString(TEXT("\"'`/")))
    {
        TokenizedValue.ReplaceCharInline(Boundary, TEXT(' '));
    }
    TArray<FString> CommandTokens;
    TokenizedValue.ParseIntoArrayWS(CommandTokens);
    for (const FString& Token : CommandTokens)
    {
        if (Token.Equals(TEXT("python"))
            || Token.Equals(TEXT("python3"))
            || Token.StartsWith(TEXT("python3."))
            || Token.Equals(TEXT("node"))
            || Token.Equals(TEXT("nodejs"))
            || Token.Equals(TEXT("ruby"))
            || Token.Equals(TEXT("perl"))
            || Token.Equals(TEXT("php"))
            || Token.Equals(TEXT("lua"))
            || Token.Equals(TEXT("deno"))
            || Token.Equals(TEXT("bun"))
            || Token.Equals(TEXT("pwsh"))
            || Token.Equals(TEXT("powershell"))
            || Token.Equals(TEXT("powershell.exe"))
            || Token.Equals(TEXT("bash"))
            || Token.Equals(TEXT("dash"))
            || Token.Equals(TEXT("zsh"))
            || Token.Equals(TEXT("ksh"))
            || Token.Equals(TEXT("fish"))
            || Token.Equals(TEXT("sh"))
            || Token.Equals(TEXT("cmd"))
            || Token.Equals(TEXT("cmd.exe"))
            || Token.Equals(TEXT("wscript"))
            || Token.Equals(TEXT("wscript.exe"))
            || Token.Equals(TEXT("cscript"))
            || Token.Equals(TEXT("cscript.exe")))
        {
            return true;
        }
    }
    return false;
}

bool HasInterpreterMutationPayload(const FString& LowerValue)
{
    const TCHAR* MutationMarkers[] = {
        TEXT("shutil.rmtree("), TEXT("os.chmod("), TEXT("os.chown("),
        TEXT("os.link("), TEXT("os.mkdir("), TEXT("os.makedirs("),
        TEXT("os.remove("), TEXT("os.rename("), TEXT("os.replace("),
        TEXT("os.rmdir("), TEXT("os.symlink("), TEXT("os.system("),
        TEXT("os.truncate("), TEXT("os.unlink("), TEXT(".append("),
        TEXT(".save("), TEXT(".touch("), TEXT(".truncate("), TEXT(".write("),
        TEXT(".write_bytes("), TEXT(".write_text("), TEXT("appendfile("),
        TEXT("appendfilesync("), TEXT("chmod("), TEXT("chmodsync("),
        TEXT("chown("), TEXT("chownsync("), TEXT("copyfile("),
        TEXT("copyfilesync("), TEXT("createwritestream("), TEXT("cp("),
        TEXT("cpsync("), TEXT("file.write("), TEXT("file_put_contents("),
        TEXT("io.write("), TEXT("link("), TEXT("linksync("), TEXT("mkdir("),
        TEXT("mkdirsync("), TEXT("rename("), TEXT("renamesync("),
        TEXT("symlink("), TEXT("symlinksync("), TEXT("truncate("),
        TEXT("truncatesync("), TEXT("writefile("), TEXT("writefilesync("),
        TEXT("new-item"), TEXT("set-content"), TEXT("add-content"),
        TEXT("out-file")
    };
    for (const TCHAR* Marker : MutationMarkers)
    {
        if (LowerValue.Contains(Marker))
        {
            return true;
        }
    }
    if (!LowerValue.Contains(TEXT("open(")))
    {
        return false;
    }
    const TCHAR* WriteModeMarkers[] = {
        TEXT(",'w"), TEXT(", 'w"), TEXT(",\"w"), TEXT(", \"w"),
        TEXT(",'a"), TEXT(", 'a"), TEXT(",\"a"), TEXT(", \"a"),
        TEXT(",'x"), TEXT(", 'x"), TEXT(",\"x"), TEXT(", \"x"),
        TEXT(",'+"), TEXT(", '+"), TEXT(",\"+"), TEXT(", \"+"),
        TEXT("mode='w"), TEXT("mode = 'w"), TEXT("mode=\"w"),
        TEXT("mode = \"w"), TEXT("mode='a"), TEXT("mode = 'a"),
        TEXT("mode=\"a"), TEXT("mode = \"a"), TEXT("mode='x"),
        TEXT("mode = 'x"), TEXT("mode=\"x"), TEXT("mode = \"x"),
        TEXT("mode='+"), TEXT("mode = '+"), TEXT("mode=\"+"),
        TEXT("mode = \"+")
    };
    for (const TCHAR* Marker : WriteModeMarkers)
    {
        if (LowerValue.Contains(Marker))
        {
            return true;
        }
    }
    return false;
}

bool HasAliasedPythonMutationImport(const FString& LowerValue)
{
    const TCHAR* MutationImports[] = {
        TEXT("chmod"), TEXT("chown"), TEXT("copy"), TEXT("copy2"),
        TEXT("copyfile"), TEXT("copytree"), TEXT("extract_archive"),
        TEXT("link"), TEXT("makedirs"), TEXT("mkdir"), TEXT("move"),
        TEXT("remove"), TEXT("removedirs"), TEXT("rename"),
        TEXT("replace"), TEXT("rmdir"), TEXT("rmtree"),
        TEXT("symlink"), TEXT("system"), TEXT("truncate"),
        TEXT("unlink"), TEXT("unpack_archive")
    };
    return ContainsAliasedPythonImport(
        LowerValue,
        MutationImports,
        UE_ARRAY_COUNT(MutationImports));
}
}

bool HasInterpreterSemanticMutation(
    const FString& LowerValue,
    const FString& CompactValue)
{
    if (!IsInterpreterCommand(LowerValue))
    {
        return false;
    }
    const bool bDynamicPayload = LowerValue.Contains(TEXT("base64"))
        || LowerValue.Contains(TEXT("b64decode"))
        || LowerValue.Contains(TEXT("fromhex"))
        || LowerValue.Contains(TEXT("exec("))
        || LowerValue.Contains(TEXT("eval("))
        || LowerValue.Contains(TEXT("compile("))
        || LowerValue.Contains(TEXT("__import__"))
        || LowerValue.Contains(TEXT("getattr("))
        || LowerValue.Contains(TEXT("invoke-expression"));
    const bool bReferencesUnrealApi =
        CompactValue.Contains(TEXT("importunreal"))
        || CompactValue.Contains(TEXT("fromunreal"))
        || LowerValue.Contains(TEXT("unreal."));
    return bReferencesUnrealApi
        || bDynamicPayload
        || HasAliasedPythonMutationImport(LowerValue)
        || HasInterpreterMutationPayload(LowerValue);
}

bool HasShellPathExpansionMutation(const FString& LowerValue)
{
    const int32 BraceOpen = LowerValue.Find(TEXT("{"));
    const int32 BraceClose = BraceOpen == INDEX_NONE
        ? INDEX_NONE
        : LowerValue.Find(
            TEXT("}"),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            BraceOpen + 1);
    const int32 BraceComma = BraceOpen == INDEX_NONE
        ? INDEX_NONE
        : LowerValue.Find(
            TEXT(","),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            BraceOpen + 1);
    const bool bHasBraceExpansion =
        BraceOpen != INDEX_NONE
        && BraceClose != INDEX_NONE
        && BraceComma != INDEX_NONE
        && BraceComma < BraceClose;
    const int32 BracketOpen = LowerValue.Find(TEXT("["));
    const bool bHasBracketExpansion =
        BracketOpen != INDEX_NONE
        && LowerValue.Find(
            TEXT("]"),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            BracketOpen + 1) != INDEX_NONE;
    if (!bHasBraceExpansion && !bHasBracketExpansion)
    {
        return false;
    }

    const TCHAR* MutationMarkers[] = {
        TEXT("cp "), TEXT("install "), TEXT("mkdir "), TEXT("mv "),
        TEXT("rm "), TEXT("rmdir "), TEXT("tee "), TEXT("touch "),
        TEXT("truncate "), TEXT(">"), TEXT("| sponge"), TEXT("| tee")
    };
    for (const TCHAR* Marker : MutationMarkers)
    {
        if (LowerValue.StartsWith(Marker)
            || LowerValue.Contains(FString(TEXT(" ")) + Marker))
        {
            return true;
        }
    }
    return false;
}
}
