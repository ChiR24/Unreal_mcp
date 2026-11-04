// set_project_thumbnail: the picture the Epic launcher and the project browser show for the project, the level view in
// game view (no icons or gizmos) drawn as a square, written where the editor's own "capture project thumbnail" writes it
// (<ProjectDir>/<ProjectName>.png, UThumbnailManager::CaptureProjectThumbnail's 192 px center crop of the viewport).
#include "Domains/Ui/McpUiPrivate.h"

#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "Domains/ControlEditor/McpEditorSupport.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Foundation/BridgeHelpers/Security/McpProjectFileWrite.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"

namespace McpUiHandlers
{
bool HandleProjectThumbnailAction(const FString &LowerSub, const TSharedPtr<FJsonObject> &Payload,
                                  const TSharedPtr<FJsonObject> &Resp, bool &bSuccess, FString &Message,
                                  FString &ErrorCode)
{
    if (LowerSub != TEXT("set_project_thumbnail"))
    {
        return false;
    }
    const auto Refuse = [&Message, &ErrorCode](const TCHAR *Code, const FString &Why)
    {
        ErrorCode = Code;
        Message = Why;
        return true;
    };
    if (GEditor->PlayWorld)
    {
        return Refuse(TEXT("PIE_RUNNING"), TEXT("The thumbnail is drawn from the level viewport, which the running game hides: stop Play In Editor first."));
    }
    FEditorViewportClient *Client = GetActiveEditorViewportClientForMcp();
    FViewport *Viewport = Client ? Client->Viewport : nullptr;
    if (!Viewport)
    {
        return Refuse(TEXT("VIEWPORT_NOT_AVAILABLE"), TEXT("No level viewport is open to draw the thumbnail from."));
    }
    double Side = 512;
    Payload->TryGetNumberField(TEXT("size"), Side);
    Side = FMath::Clamp(FMath::RoundToDouble(Side), 64.0, 1024.0);
    PlaceViewCameraForMcp(Client, Payload);
    const bool bWasGameView = Client->IsInGameView();
    Client->SetGameView(true);
    TSharedPtr<FJsonObject> Render = MakeShared<FJsonObject>();
    Render->SetStringField(TEXT("renderSize"), FString::Printf(TEXT("%dx%d"), static_cast<int32>(Side), static_cast<int32>(Side)));
    TArray<FColor> Pixels;
    FIntPoint Size(static_cast<int32>(Side), static_cast<int32>(Side));
    FString Code;
    FString Error;
    const bool bRead = ReadViewFrameForMcp(Viewport, Render, Pixels, Size, Code, Error);
    Client->SetGameView(bWasGameView);
    Client->Invalidate();
    if (!bRead || Pixels.Num() != Size.X * Size.Y)
    {
        return Refuse(TEXT("CAPTURE_FAILED"), Error.IsEmpty() ? TEXT("The view could not be drawn.") : Error);
    }
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / (FString(FApp::GetProjectName()) + TEXT(".png")));
    if (Png.Num() == 0 || !McpWriteProjectFile(Path, TArrayView<const uint8>(Png.GetData(), static_cast<int32>(Png.Num())), Error))
    {
        return Refuse(TEXT("WRITE_FAILED"), Error.IsEmpty() ? TEXT("The picture could not be encoded.") : Error);
    }
    bSuccess = true;
    Message = FString::Printf(TEXT("Project thumbnail written (%dx%d): the launcher and the project browser show it from the next time they list projects."), Size.X, Size.Y);
    Resp->SetStringField(TEXT("path"), Path);
    Resp->SetNumberField(TEXT("width"), Size.X);
    Resp->SetNumberField(TEXT("height"), Size.Y);
    Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
    Resp->SetStringField(TEXT("imageBase64"), FBase64::Encode(Png.GetData(), static_cast<uint32>(Png.Num())));
    return true;
}
} // namespace McpUiHandlers
