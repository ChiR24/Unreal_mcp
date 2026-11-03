// McpViewRender.cpp — screenshot renderSize: the level viewport's view drawn off-screen at a size of the caller's choosing.
//
// The level viewport is as wide as the editor's layout makes it, so a picture from the game's camera showed another
// slice of the scene than the game's own 16:9 frame (with the field of view held across, a wider viewport cuts the top
// and bottom off). The engine's high resolution screenshot draws the viewport's view (its camera, show flags, post
// process, exposure) into an off-screen viewport of any size with the same horizontal field of view, so the frame is
// what a game window of that size shows.
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UnrealClient.h"

namespace
{
constexpr int32 MinRenderSide = 64;
constexpr int32 MaxRenderSide = 3840;
// Frames drawn before the one kept: the engine's default 4 leaves Lumen's lighting and the temporal anti-aliasing of a
// view whose size just changed short of where the viewport's own frames are.
constexpr int32 RunUpFrames = 16;

bool ParseRenderSize(const FString& Text, FIntPoint& OutSize, FString& OutError)
{
    FString Width, Height;
    const FString Trimmed = Text.TrimStartAndEnd().ToLower();
    if (!Trimmed.Split(TEXT("x"), &Width, &Height) || !Width.IsNumeric() || !Height.IsNumeric() || Width.Contains(TEXT("."))
        || Height.Contains(TEXT(".")))
    {
        OutError = FString::Printf(TEXT("renderSize '%s' is not WxH, for example 1920x1080."), *Text);
        return false;
    }
    OutSize = FIntPoint(FCString::Atoi(*Width), FCString::Atoi(*Height));
    if (OutSize.X < MinRenderSide || OutSize.Y < MinRenderSide || OutSize.X > MaxRenderSide || OutSize.Y > MaxRenderSide)
    {
        OutError = FString::Printf(TEXT("renderSize '%s': each side must be %d to %d pixels."), *Text, MinRenderSide, MaxRenderSide);
        return false;
    }
    return true;
}

// The engine's high resolution screenshot state is shared with the editor's own screenshot dialog and with every
// viewport that draws next, so all of it is put back on every path. The engine always saves the frame to a file as
// well; it goes to a scratch file that is deleted.
bool RenderViewAtSize(FViewport* Viewport, FIntPoint Size, TArray<FColor>& OutPixels, FIntPoint& OutSize)
{
    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    const FString Scratch = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("EngineWire/ViewRender.png"));
    const FIntRect OldRegion = Config.CaptureRegion;
    const FIntRect OldUnscaledRegion = Config.UnscaledCaptureRegion;
    const bool bOldMask = Config.bMaskEnabled;
    const bool bOldHdr = Config.bCaptureHDR;
    const bool bOldDump = Config.bDumpBufferVisualizationTargets;
    const FString OldFilename = Config.FilenameOverride;
    const uint32 OldX = GScreenshotResolutionX;
    const uint32 OldY = GScreenshotResolutionY;
    const bool bOldAutomation = GIsAutomationTesting;
    IConsoleVariable* Delay = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HighResScreenshotDelay"));
    const int32 OldDelay = Delay ? Delay->GetInt() : 0;
    bool bCaptured = false;
    const FDelegateHandle Listener = FScreenshotRequest::OnScreenshotCaptured().AddLambda(
        [&](int32 Width, int32 Height, const TArray<FColor>& Colors)
        {
            OutPixels = Colors;
            OutSize = FIntPoint(Width, Height);
            bCaptured = Colors.Num() == Width * Height && Colors.Num() > 0;
        });
    ON_SCOPE_EXIT
    {
        FScreenshotRequest::OnScreenshotCaptured().Remove(Listener);
        Config.CaptureRegion = OldRegion;
        Config.UnscaledCaptureRegion = OldUnscaledRegion;
        Config.bMaskEnabled = bOldMask;
        Config.bCaptureHDR = bOldHdr;
        Config.bDumpBufferVisualizationTargets = bOldDump;
        Config.FilenameOverride = OldFilename;
        GScreenshotResolutionX = OldX;
        GScreenshotResolutionY = OldY;
        GIsHighResScreenshot = false;
        GIsAutomationTesting = bOldAutomation;
        if (Delay && Delay->GetInt() != OldDelay)
        {
            Delay->Set(OldDelay, ECVF_SetByCode);
        }
        IFileManager::Get().Delete(*Scratch, false, true, true);
        IFileManager::Get().Delete(*FPaths::ChangeExtension(Scratch, TEXT("exr")), false, true, true);
    };
    Config.CaptureRegion = FIntRect(0, 0, 0, 0);
    Config.UnscaledCaptureRegion = FIntRect(0, 0, 0, 0);
    Config.bMaskEnabled = false;
    Config.bCaptureHDR = false;
    Config.bDumpBufferVisualizationTargets = false;
    Config.FilenameOverride = Scratch;
    GScreenshotResolutionX = Size.X;
    GScreenshotResolutionY = Size.Y;
    if (Delay && OldDelay < RunUpFrames)
    {
        Delay->Set(RunUpFrames, ECVF_SetByCode);
    }
    // Set as an automation test sets it: UE 5.0 and 5.1 hand the frame to listeners only then, and in every 5.x it keeps
    // away the "screenshot saved" toast, which while the editor is minimized opens as a window of its own over whatever
    // the user has in front. Nothing else on the drawing path reads it.
    GIsAutomationTesting = true;
    // One synchronous draw: every viewport's Draw checks the global flag, so nothing else may draw before this one.
    if (Viewport->TakeHighResScreenShot())
    {
        Viewport->Draw();
    }
    return bCaptured;
}
} // namespace

bool RefuseRenderSizeForMcp(UEngineWireSubsystem* Bridge, TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, const FString& Mode, bool bEjectedView)
{
    FString Text;
    if (!Payload->TryGetStringField(TEXT("renderSize"), Text))
    {
        return false;
    }
    FIntPoint Size;
    FString Error;
    FString Code = TEXT("INVALID_ARGUMENT");
    if (ParseRenderSize(Text, Size, Error))
    {
        if (Mode == TEXT("full_editor_window"))
        {
            Error = TEXT("renderSize draws the level viewport's view; full_editor_window photographs a window as it is on screen.");
        }
        else if (GEditor->PlayWorld && !bEjectedView)
        {
            Code = TEXT("PIE_VIEW_NOT_EJECTED");
            Error = TEXT("renderSize draws the level viewport's view, which is hidden behind the game while the player plays: eject first (control_editor play control=eject), or take the picture of the game without renderSize.");
        }
        else
        {
            return false;
        }
    }
    SendStandardErrorResponse(Bridge, Socket, RequestId, Code, Error, nullptr);
    return true;
}

bool ReadViewFrameForMcp(FViewport* Viewport, const TSharedPtr<FJsonObject>& Payload, TArray<FColor>& OutPixels,
                         FIntPoint& InOutSize, FString& OutCode, FString& OutError)
{
    FString Text;
    FIntPoint Size;
    OutCode = TEXT("CAPTURE_FAILED");
    if (Payload->TryGetStringField(TEXT("renderSize"), Text) && ParseRenderSize(Text, Size, OutError))
    {
        if (!RenderViewAtSize(Viewport, Size, OutPixels, InOutSize))
        {
            OutError = FString::Printf(TEXT("The view drawn at %dx%d came back empty (a viewport showing HDR output gives no 8-bit frame)."), Size.X, Size.Y);
            return false;
        }
    }
    else if (!Viewport->ReadPixels(OutPixels, FReadSurfaceDataFlags(RCM_UNorm)) || OutPixels.Num() == 0)
    {
        OutError = TEXT("Failed to read pixels from viewport");
        return false;
    }
    for (FColor& Pixel : OutPixels)
    {
        Pixel.A = 255;
    }
    return true;
}
