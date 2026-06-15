#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::OpenCodeAcp::PermissionPaths
{
    bool ResolveExistingPath(const FString& Path, FString& OutResolved);
    bool ResolvesToUnrealBinaryAsset(
        const FString& Candidate,
        const FString& WorkingDirectory);
    bool ResolvesToUnrealContent(
        const FString& Candidate,
        const FString& WorkingDirectory);
    bool ResolvesToUnrealProjectState(
        const FString& Candidate,
        const FString& WorkingDirectory);
    bool TraversesSymbolicLink(
        const FString& Candidate,
        const FString& WorkingDirectory);
}
