#pragma once

#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "UObject/NameTypes.h"

namespace McpSubobjectPath
{
inline bool IsObjectName(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() >= NAME_SIZE) return false;
    for (const TCHAR Char : Name)
    {
        if (Char <= TEXT(' ') || Char == 0x7f || Char == TEXT('\\') ||
            Char == TEXT('<') || Char == TEXT('>') || Char == TEXT('?') || Char == TEXT('*') ||
            FCString::Strchr(INVALID_OBJECTNAME_CHARACTERS, Char)) return false;
    }
    return true;
}

// Lexical only: no mount lookup, object resolution or editor state on socket threads.
// Keep the full locator as well as its package; never authorize an unchecked suffix.
inline bool TryCanonicalize(const FString& Input, FString& OutObject, FString& OutPackage)
{
    OutObject.Reset();
    OutPackage.Reset();
    if (Input.Len() != FCString::Strlen(*Input)) return false;
    for (const TCHAR Char : Input)
    {
        if (Char < TEXT(' ') || Char == 0x7f) return false;
    }
    FString Path = Input.TrimStartAndEnd();
    Path.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!Path.StartsWith(TEXT("/")) || Path.Contains(TEXT("//")) || Path.Contains(TEXT(".."))) return false;
    McpAssetPathCanonical::MapContentRootInline(Path);
    const int32 Dot = Path.Find(TEXT("."));
    const int32 Colon = Path.Find(TEXT(":"));
    if (Dot == INDEX_NONE || Colon <= Dot + 1 || Colon >= Path.Len() - 1) return false;
    const FString Package = Path.Left(Dot);
    if (Package.EndsWith(TEXT("/")) || !McpAssetPathCanonical::IsUnrealRoot(Package)) return false;
    if (Package.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1) == INDEX_NONE) return false;
    for (const TCHAR Char : Package)
    {
        if (Char <= TEXT(' ') || FCString::Strchr(INVALID_LONGPACKAGE_CHARACTERS, Char)) return false;
    }
    if (!IsObjectName(Path.Mid(Dot + 1, Colon - Dot - 1))) return false;
    TArray<FString> Subobjects;
    Path.Mid(Colon + 1).ParseIntoArray(Subobjects, TEXT("."), false);
    for (const FString& Name : Subobjects)
    {
        if (!IsObjectName(Name)) return false;
    }
    if (Subobjects.IsEmpty()) return false;
    OutObject = MoveTemp(Path);
    OutPackage = Package;
    return true;
}
}
