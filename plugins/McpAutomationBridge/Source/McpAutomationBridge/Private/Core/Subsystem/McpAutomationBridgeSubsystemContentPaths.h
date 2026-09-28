#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"

namespace McpAutomationBridgeSubsystemResponse
{
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
