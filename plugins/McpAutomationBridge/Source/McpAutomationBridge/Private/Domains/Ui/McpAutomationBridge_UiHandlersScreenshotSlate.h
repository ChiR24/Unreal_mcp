#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

// What `game_viewport` can and cannot show, said out loud in the receipt.
//
// FViewport::ReadPixels returns the 3D scene render target and nothing else.
// Every widget added with AddToViewport is a Slate widget layered ON TOP of
// that target and composited by the window, so a HUD, a menu, or any UMG at all
// is simply absent from these pixels -- while the receipt used to say nothing
// but "Screenshot captured". Anyone using this capability to check a UI got a
// confidently UI-free image and no hint that a whole layer had been dropped,
// which reads exactly like the UI failing to appear at runtime.
//
// Compositing the Slate layer here is NOT an option:
// FSlateApplication::TakeScreenshot hands the renderer a raw pointer to the
// caller's colour array and then asks for a draw. When that draw does not
// happen -- a throttled editor, a window that is not being painted -- the
// request stays registered against an array that has since left scope, and the
// next ordinary Slate paint writes through it and takes the editor down with an
// invalid-shared-pointer assert. That was tried, and it crashed the editor
// nineteen seconds after the call that looked like it had merely "declined".
//
// So `game_viewport` keeps the safe read and states its limit; the caller who
// needs the widget layer uses mode `full_editor_window`, which captures the OS
// window and therefore contains the composited UMG.
inline FString McpSceneOnlyCaptureWarning() {
  return FString(
      TEXT("This image is the 3D scene render target ONLY. No widget added "
           "with AddToViewport (HUD, menu, any UMG) appears in it, because "
           "Slate composites those over the viewport rather than into it. Do "
           "NOT read an empty-looking UI here as the UI being absent at "
           "runtime -- re-capture with mode 'full_editor_window' to see the "
           "widget layer."));
}
#endif
