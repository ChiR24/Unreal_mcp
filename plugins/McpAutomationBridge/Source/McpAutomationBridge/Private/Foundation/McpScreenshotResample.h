// Shared screenshot downscaling for every capture surface.
//
// Three handlers capture pixels — the editor viewport, the full Slate window,
// and the game viewport — and each declares a `resolution` parameter. Keeping
// the implementation here means a fix to one is a fix to all three, rather than
// the previous state where the parameter was honoured by none of them.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Resolve the payload's "resolution" into the size this capture should encode at.
 *
 * The capture is a resample of one already-rendered frame, not a re-render, so
 * the requested WxH is treated as a bounding box: the aspect ratio is preserved
 * and a box at least as large as the source is a no-op rather than an upscale.
 *
 * Returns false and fills OutError only when "resolution" is present but
 * malformed; an absent resolution is success with OutSize == SourceSize.
 */
bool ResolveScreenshotResolutionForMcp(const TSharedPtr<FJsonObject> &Payload,
                                       FIntPoint SourceSize, FIntPoint &OutSize,
                                       FString &OutError);

/** Area-average resample of a BGRA bitmap. Alpha is forced opaque. */
void ResampleBitmapForMcp(const TArray<FColor> &SrcBitmap, FIntPoint SrcSize,
                          TArray<FColor> &OutBitmap, FIntPoint DstSize);

/**
 * The directory a capture is saved in: the payload's "path" (a directory inside
 * the project, relative to it or absolute) or DefaultDir when none was given.
 * Every capture surface declares "path"; the editor ones used to ignore it and
 * always wrote to Saved/Screenshots. False with OutError for a path that is
 * absolute outside the project or climbs out of it.
 */
bool ResolveScreenshotDirectoryForMcp(const TSharedPtr<FJsonObject> &Payload,
                                      const FString &DefaultDir, FString &OutDir,
                                      FString &OutError);
