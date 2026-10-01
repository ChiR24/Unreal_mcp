// Shared screenshot downscaling for every capture surface.
//
// Three handlers capture pixels — the editor viewport, the full Slate window,
// and the game viewport — and each declares a `resolution` parameter. Keeping
// the implementation here means a fix to one is a fix to all three, rather than
// the previous state where the parameter was honoured by none of them.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/Function.h"

/**
 * Resolve the payload's "resolution" into the size this capture should encode at.
 *
 * The capture is a resample of one already-rendered frame, not a re-render, so
 * the requested WxH is treated as a bounding box: the aspect ratio is preserved
 * and a box at least as large as the source is a no-op rather than an upscale.
 *
 * Returns false and fills OutError only when "resolution" is present but
 * malformed. An absent resolution keeps the source size for a file-only capture
 * and fits McpInlineScreenshotBox for one whose image is handed back inline.
 */
bool ResolveScreenshotResolutionForMcp(const TSharedPtr<FJsonObject> &Payload,
                                       FIntPoint SourceSize, FIntPoint &OutSize,
                                       FString &OutError);

/**
 * Whether the capture hands its PNG back inline (returnBase64, true unless the
 * caller says false). A caller on the far side of the bridge is usually a model
 * that cannot open the saved file, so a plain "take a screenshot" must show it.
 */
bool McpScreenshotReturnsImage(const TSharedPtr<FJsonObject> &Payload);

/** The box an inline image fits when no resolution was asked for: a native
 *  2580x1460 editor window is ~2 MB of PNG and can pass the 3 MB base64 cap
 *  once busy, while 1600x900 stays well under it. */
constexpr const TCHAR *McpInlineScreenshotBox = TEXT("1600x900");

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

/** Shader compile jobs still outstanding (GShaderCompilingManager); 0 when none, or when there is no manager. */
int32 McpShaderJobsRemaining();

/** The longest a waitForShaders capture waits: the bridge's client gives up on a call at 30 s. */
constexpr double McpShaderWaitMaxSeconds = 25.0;

/**
 * Puts shadersCompiling (the outstanding shader jobs) on a capture or editor-state reply and, while some are, a
 * warning: surfaces they cover are drawn with the engine's default material (black foliage, grey ground), which
 * nothing in the picture says. A wait McpDeferForShaderCompile ran is reported as shaderWait. Payload may be null.
 */
void McpAddShaderCompileState(const TSharedPtr<FJsonObject> &Resp,
                              const TSharedPtr<FJsonObject> &Payload);

/**
 * waitForShaders: when the payload asks for it and shaders are compiling, polls on the core ticker (the game
 * thread keeps running, which is what lets the compiling finish) for at most McpShaderWaitMaxSeconds, then calls
 * Resume with the payload carrying shaderWait {waitedSeconds, jobsLeft, timedOut}. True when it deferred: the
 * caller returns at once and Resume captures. False when the capture should go ahead now (not asked for,
 * nothing compiling, or already waited).
 */
bool McpDeferForShaderCompile(const TSharedPtr<FJsonObject> &Payload,
                              TFunction<void(const TSharedPtr<FJsonObject> &)> Resume);
