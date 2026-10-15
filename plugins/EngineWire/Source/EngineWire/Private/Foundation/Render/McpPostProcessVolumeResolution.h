#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class APostProcessVolume;
class UWorld;

// Shared post-process volume resolver.
//
// Both the Render domain (configure_exposure, configure_pp_blend, set_bloom_*,
// set_tonemapper_type, configure_bloom, set_exposure_compensation) and the
// Lighting domain (set_exposure, set_ambient_occlusion) route through this
// single deterministic class-based resolver so the two advertised exposure
// actions no longer disagree.
//
// Resolution policy (records-unchanged branch of plan line 381):
//   - actorName matching a PostProcessVolume -> that volume (Explicit); the
//     post-process, exposure and screen records all declare it.
//   - exactly one unbound APostProcessVolume -> that volume (ClassDefault).
//   - more than one unbound -> AMBIGUOUS + candidate identities, never a silent
//     pick.
//   - zero unbound -> bAllowSpawn ? spawn unbound (Spawned) : ACTOR_NOT_FOUND.
namespace McpRenderHandlers
{
    // Resolves a PostProcessVolume for the given payload. On failure returns
    // nullptr and fills OutError/OutErrorCode with a typed receipt
    // (AMBIGUOUS / ACTOR_NOT_FOUND / NO_WORLD / EXECUTION_ERROR).
    APostProcessVolume* McpResolvePostProcessVolume(
        UWorld* World,
        const TSharedPtr<FJsonObject>& Payload,
        bool bAllowSpawn,
        FString& OutError,
        FString& OutErrorCode);
}
