#pragma once

#include "CoreMinimal.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"
#include "Misc/PackageName.h"

/**
 * The package create_niagara_system / create_niagara_emitter write for a folder and a name, or an empty string
 * and OutError when the request must be refused.
 *
 * The creators handed the sanitized folder plus the name straight to CreatePackage. SanitizeProjectRelativePath
 * accepts any character under /Game, so a folder such as "/Game/My FX" passed: create_niagara_emitter answered
 * "Emitter created." with nothing saved (FromPackageNameChecked: Invalid LongPackageName), and
 * create_niagara_system answered CREATE_FAILED. So the whole package name the creator will write is sanitized and
 * must be a long package name under a writable root. (The name itself is checked before this, by
 * ValidateNiagaraIdentifier.)
 */
inline FString McpNiagaraCreatePackageName(const FString& Folder, const FString& AssetName, FString& OutError)
{
    FString Requested = Folder.TrimStartAndEnd();
    Requested.RemoveFromEnd(TEXT("/"));
    const FString Candidate = Requested + TEXT("/") + AssetName;
    const FString PackageName = SanitizeProjectRelativePath(Candidate);
    if (PackageName.IsEmpty())
    {
        OutError = McpPathRefusalMessage(TEXT("path"), Candidate) + TEXT(" Nothing was created.");
        return FString();
    }
    FText Reason;
    if (!FPackageName::IsValidLongPackageName(PackageName, false, &Reason))
    {
        OutError = FString::Printf(
            TEXT("'path' %s with name %s does not make a package that can be saved (%s): %s Nothing was created."),
            *Folder, *AssetName, *PackageName, *Reason.ToString());
        return FString();
    }
    return PackageName;
}
