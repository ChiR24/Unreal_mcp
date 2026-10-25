#include "Domains/ControlEditor/Input/McpEditorGameCursor.h"

#include "Editor.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SceneViewport.h"

bool McpSimulateGameMouseMove(double X, double Y, bool &bSuccess, bool &bRoutedToPIE, bool &bHandledByPIE,
                              FString &Message) {
  UWorld *World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
  if (!World) {
    return false;
  }
  UGameViewportClient *Client = World->GetGameViewport();
  FSceneViewport *Viewport = Client ? Client->GetGameViewport() : nullptr;
  const FGeometry Geometry = Viewport ? Viewport->GetCachedGeometry() : FGeometry();
  const FVector2D Local = Geometry.GetLocalSize();
  const FIntPoint Pixels = Viewport ? Viewport->GetSizeXY() : FIntPoint::ZeroValue;
  bRoutedToPIE = true;
  if (Local.X <= 0.0 || Local.Y <= 0.0 || Pixels.X <= 0 || Pixels.Y <= 0) {
    bSuccess = bHandledByPIE = false;
    Message = TEXT("The game viewport has no size (a minimized editor window), so the game has no cursor to move: "
                   "restore the window (control_editor configure_editor setting window) and move again.");
    return true;
  }
  if (X < 0.0 || Y < 0.0 || X >= Pixels.X || Y >= Pixels.Y) {
    bSuccess = bHandledByPIE = false;
    Message = FString::Printf(TEXT("(%g, %g) is outside the game viewport, which is %d x %d pixels: x and y are "
                                   "pixels of the game viewport while a game runs."),
                              X, Y, Pixels.X, Pixels.Y);
    return true;
  }
  // Viewport pixels back to the geometry's local units, then to the absolute point the event carries: the viewport
  // turns it into the same pixels when it caches the cursor.
  const FVector2D Point = Geometry.LocalToAbsolute(FVector2D(X * Local.X / Pixels.X, Y * Local.Y / Pixels.Y));
  const FPointerEvent Move(0, Point, Point, TSet<FKey>(), EKeys::Invalid, 0.0f, FModifierKeysState());
  Viewport->OnMouseMove(Geometry, Move);
  FIntPoint Now;
  Viewport->GetMousePos(Now);
  bSuccess = bHandledByPIE = true;
  Message = FString::Printf(TEXT("The game's cursor is at (%d, %d) of its %d x %d viewport, where Get Mouse Position "
                                 "and Get Hit Result Under Cursor read it; the system cursor was NOT moved."),
                            Now.X, Now.Y, Pixels.X, Pixels.Y);
  return true;
}
