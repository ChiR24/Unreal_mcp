#pragma once

#include "CoreMinimal.h"

namespace McpSequencePathSecurity {

enum class ELocalPathUse : uint8 {
  MediaInput,
  RenderOutput
};

enum class ERemoteMediaUrlError : uint8 {
  InvalidArgument,
  NotAllowed
};

bool ValidateLocalPath(const FString &InputPath, ELocalPathUse PathUse,
                       FString &OutResolvedPath, FString &OutError);
/**
 * Re-validates an already-resolved absolute filesystem path against the
 * allowed roots and symlink policy.
 *
 * Contract: the input MUST be an already-resolved absolute filesystem path
 * (the form produced by `ValidateLocalPath`'s `OutResolvedPath` parameter).
 * Short forms like `/Saved/foo.mp4` and relative paths are NOT supported
 * and will be rejected as "changed after validation" because the internal
 * re-resolution expands them and the round-trip equality check fails.
 *
 * This function exists to defend against TOCTOU between the original
 * validation and the use of the path (symlink swap, file replacement, etc.).
 * It re-runs the full validation pipeline and then verifies the path is
 * unchanged from what the caller passed in.
 */
bool RevalidateResolvedLocalPath(const FString &ResolvedPath,
                                 ELocalPathUse PathUse, FString &OutError);
bool ValidateWritableAssetPath(const FString &InputPath,
                               FString &OutResolvedPath, FString &OutError);
bool ValidateRemoteMediaUrl(const FString &InputUrl, FString &OutResolvedUrl,
                            ERemoteMediaUrlError &OutErrorType,
                            FString &OutError);

}
