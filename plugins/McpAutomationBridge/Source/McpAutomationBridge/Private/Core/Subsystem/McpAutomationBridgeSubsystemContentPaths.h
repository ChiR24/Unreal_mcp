#pragma once

#include "CoreMinimal.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Misc/PackageName.h"

namespace McpAutomationBridgeSubsystemResponse
{
// A content mount that is registered right now ("/MoverExamples", "/MetaHumanCharacter", a Fab pack) is as public as
// /Game: it is how a package path starts, and the only way to say which of the caller's paths a reply is about.
// Redacting it answered "Invalid package path '[path redacted]'" for an argument the caller had just sent. The token at
// Index must be the bare mount name, then the end of the token or a '/' (the rule the engine roots follow). A host root
// (/etc, /home, /Users) never counts, whatever is mounted under that name. Asks the engine, so not from a log device.
inline bool IsRegisteredContentMountAt(const FString& Value, int32 Index)
{
    int32 End = Index + 1;
    while (End < Value.Len() && (FChar::IsAlnum(Value[End]) || Value[End] == TEXT('_')))
    {
        ++End;
    }
    if (End == Index + 1 || (End < Value.Len() && (Value[End] == TEXT('-') || Value[End] == TEXT('.') || Value[End] == TEXT('\\'))))
    {
        return false;
    }
    const FString Name = Value.Mid(Index + 1, End - Index - 1);
    return !McpAssetPathCanonical::IsHostFilesystemRootSegment(Name) &&
           FPackageName::MountPointExists(TEXT("/") + Name + TEXT("/"));
}

// A host path to a package file under mounted content says which asset an engine warning is about
// (the disk path of M_Wall.uasset, then ": Default Material will be used"). Redacting the whole run
// left "[path redacted], Default Material will be used", which names nothing. The part up to the
// package file becomes its package path (/Game/Maps/M_Wall); OutRest is whatever followed it.
inline bool MapContentPathForResponse(const FString& Token, FString& OutPackage, FString& OutRest)
{
    for (const TCHAR* Extension : {TEXT(".uasset"), TEXT(".umap")})
    {
        const int32 At = Token.Find(Extension, ESearchCase::IgnoreCase);
        const int32 End = At + FCString::Strlen(Extension);
        if (At != INDEX_NONE && FPackageName::TryConvertFilenameToLongPackageName(Token.Left(End), OutPackage))
        {
            OutRest = Token.Mid(End);
            return true;
        }
    }
    return false;
}
}
