#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::Validation
{
    TArray<FString> GetManagedOpenCodeConfigDirectories();
    TArray<FString> GetOpenCodeConfigDirectories(const FString& ProjectDirectory);
    FString ResolveOpenCodePath(
        const FString& ProjectDirectory,
        const FString& Path);
}
