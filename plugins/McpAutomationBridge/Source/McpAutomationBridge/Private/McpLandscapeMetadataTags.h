#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

/** Authoring dimensions encoded on MCP-created Landscape actors. */
struct FMcpLandscapeMetadata
{
  int32 ComponentsX = 0;
  int32 ComponentsY = 0;
  int32 QuadsPerComponent = 0;
};

namespace McpLandscapeMetadataTags
{
static constexpr const TCHAR* ComponentsXKey = TEXT("MCP_LandscapeComponentsX");
static constexpr const TCHAR* ComponentsYKey = TEXT("MCP_LandscapeComponentsY");
static constexpr const TCHAR* QuadsPerComponentKey = TEXT("MCP_LandscapeQuadsPerComponent");

/**
 * Replaces a single integer metadata tag on an actor.
 *
 * Landscape dimensions are stored as actor tags so generic actor tools can
 * recover authoring bounds before Unreal has generated landscape components.
 */
inline void AddIntTag(AActor* Actor, const TCHAR* Key, int32 Value)
{
  if (!Actor)
  {
    return;
  }

  const FString Prefix = FString::Printf(TEXT("%s="), Key);
  Actor->Tags.RemoveAll([&Prefix](const FName& TagName) {
    return TagName.ToString().StartsWith(Prefix);
  });
  Actor->Tags.Add(FName(*FString::Printf(TEXT("%s%d"), *Prefix, Value)));
}

/** Encodes MCP landscape authoring dimensions onto an actor's tags. */
inline void EncodeLandscapeMetadata(AActor* Actor, int32 ComponentsX, int32 ComponentsY, int32 QuadsPerComponent)
{
  AddIntTag(Actor, ComponentsXKey, ComponentsX);
  AddIntTag(Actor, ComponentsYKey, ComponentsY);
  AddIntTag(Actor, QuadsPerComponentKey, QuadsPerComponent);
}

/** Reads one integer metadata tag from an actor. */
inline bool TryReadIntTag(const AActor* Actor, const TCHAR* Key, int32& OutValue)
{
  if (!Actor)
  {
    return false;
  }

  const FString Prefix = FString::Printf(TEXT("%s="), Key);
  for (const FName& TagName : Actor->Tags)
  {
    const FString Tag = TagName.ToString();
    if (Tag.StartsWith(Prefix))
    {
      OutValue = FCString::Atoi(*Tag.RightChop(Prefix.Len()));
      return true;
    }
  }
  return false;
}

/** Decodes and validates MCP landscape authoring dimensions from actor tags. */
inline bool DecodeLandscapeMetadata(const AActor* Actor, FMcpLandscapeMetadata& OutMetadata)
{
  const bool bHasComponentsX = TryReadIntTag(Actor, ComponentsXKey, OutMetadata.ComponentsX);
  const bool bHasComponentsY = TryReadIntTag(Actor, ComponentsYKey, OutMetadata.ComponentsY);
  const bool bHasQuads = TryReadIntTag(Actor, QuadsPerComponentKey, OutMetadata.QuadsPerComponent);
  return bHasComponentsX && bHasComponentsY && bHasQuads &&
      OutMetadata.ComponentsX > 0 && OutMetadata.ComponentsY > 0 &&
      OutMetadata.QuadsPerComponent > 0;
}
} // namespace McpLandscapeMetadataTags
