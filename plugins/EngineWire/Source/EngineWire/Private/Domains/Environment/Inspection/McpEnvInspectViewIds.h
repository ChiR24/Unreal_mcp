#pragma once

#include "CoreMinimal.h"

class AActor;

// The level viewport's hit proxy map holds, per pixel, the id of what was drawn there (what a click selects by).
// describe_view counts it per actor; capture_passes paints it as an id image.
namespace McpEnvironmentHandlers {
enum class EMcpViewKind : uint8 { Background, Other, Actor };

struct FMcpViewProxy {
    EMcpViewKind Kind = EMcpViewKind::Background;
    AActor *Actor = nullptr;
};

// An actor the game draws, Other for what only the editor draws (icons, gizmos, shapes hidden in game) or what no
// actor owns (BSP), Background where nothing was drawn (sky, void).
FMcpViewProxy McpViewProxyOf(const FColor &Id);

// The id a hit proxy colour stands for, to cache lookups by.
inline uint32 McpViewIdKey(const FColor &Id)
{
    return (uint32(Id.R) << 16) | (uint32(Id.G) << 8) | uint32(Id.B);
}
} // namespace McpEnvironmentHandlers
