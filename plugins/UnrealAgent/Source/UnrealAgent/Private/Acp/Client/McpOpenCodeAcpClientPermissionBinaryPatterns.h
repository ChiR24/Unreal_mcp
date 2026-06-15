#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::OpenCodeAcp::PermissionBinaryPatterns
{
    bool HasUnrealBinaryAssetExtensionOrGlob(const FString& Value);
    bool IsHarmlessBinaryExtensionMention(const FString& Value);
}
