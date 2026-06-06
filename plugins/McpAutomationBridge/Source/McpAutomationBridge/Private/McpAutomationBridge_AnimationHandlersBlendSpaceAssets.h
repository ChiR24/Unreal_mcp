#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Animation/AimOffsetBlendSpace.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if __has_include("Animation/BlendSpaceBase.h")
#include "Animation/BlendSpaceBase.h"
#define MCP_HAS_BLENDSPACE_BASE 1
#elif __has_include("BlendSpaceBase.h")
#include "BlendSpaceBase.h"
#define MCP_HAS_BLENDSPACE_BASE 1
#else
#define MCP_HAS_BLENDSPACE_BASE 0
#endif
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if __has_include("Factories/BlendSpaceFactoryNew.h") &&                    \
    __has_include("Factories/BlendSpaceFactory1D.h")
#include "Factories/BlendSpaceFactory1D.h"
#include "Factories/BlendSpaceFactoryNew.h"
#define MCP_HAS_BLENDSPACE_FACTORY 1
#else
#define MCP_HAS_BLENDSPACE_FACTORY 0
#endif
#endif

class USkeleton;

namespace McpAnimationHandlers {
#if WITH_EDITOR && MCP_HAS_BLENDSPACE_FACTORY
UObject *CreateBlendSpaceAsset(const FString &AssetName,
                               const FString &PackagePath,
                               USkeleton *TargetSkeleton,
                               bool bTwoDimensional, FString &OutError);
void ApplyBlendSpaceConfiguration(UObject *BlendSpaceAsset,
                                  const TSharedPtr<FJsonObject> &Payload,
                                  bool bTwoDimensional);
#endif
} // namespace McpAnimationHandlers
