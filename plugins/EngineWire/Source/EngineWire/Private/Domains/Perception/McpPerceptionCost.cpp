// Copyright (c) 2024 EngineWire Contributors
//
// analyze_view's shader cost: the viewport drawn once in the Shader Complexity view mode, each cell's colour decoded to
// the nearest step of the engine's own ramp (UEngine::ShaderComplexityColors, green for cheap through red to white),
// summed per actor through the frame's hit proxy ids, then the view mode the user had put back.

#include "Domains/Perception/McpPerception.h"

#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "EditorViewportClient.h"
#include "Engine/Engine.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "HAL/PlatformTime.h"
#include "ShaderCompiler.h"

namespace McpPerception
{
namespace
{
constexpr double HighCost = 0.75; // the ramp's red and beyond: a third of the maximum complexity, the legend's "Bad"
constexpr int32 CostlyActors = 5;
constexpr double ShaderWaitSeconds = 5.0;

// The view mode draws its legend into the frame (a bar 64 px in from the sides and 52 px up from the bottom, 18 px tall,
// inside an 18 px black border) and a crosshair at the centre (ShaderComplexityApplyPixelShader.usf, 5.0-5.8): the
// legend's red end over a floor read as the floor costing high.
bool IsOverlay(int32 X, int32 Y, const FIntPoint& Size)
{
    const bool bLegend = X >= 46 && X < Size.X - 46 && Y >= Size.Y - 88 && Y < Size.Y - 34;
    const bool bCross = (X == Size.X / 2 || Y == Size.Y / 2) && FMath::Max(FMath::Abs(X - Size.X / 2), FMath::Abs(Y - Size.Y / 2)) < 17;
    return bLegend || bCross;
}
} // namespace

TSharedPtr<FJsonObject> ShaderCost(FEditorViewportClient& Client, const FFrame& Frame)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    const TArray<FLinearColor>& Ramp = GEngine->ShaderComplexityColors;
    FViewport* Viewport = Client.Viewport;
    if (Ramp.Num() < 2 || !Viewport)
    {
        Out->SetStringField(TEXT("note"), TEXT("the engine has no shader complexity colours to read the view by"));
        return Out;
    }
    // Game view first and off last: it saves and restores the show flags, so they come back as the user had them.
    const EViewModeIndex Was = Client.GetViewMode();
    const bool bSwitch = !Client.IsInGameView();
    if (bSwitch)
    {
        Client.SetGameView(true);
    }
    Client.SetViewMode(VMI_ShaderComplexity);
    Client.Invalidate();
    DrawViewportFramesForMcp(Viewport, Client.GetWorld(), 2);
    // The view mode's shaders compile on first use, and until they land a material draws at its stand-in's cost (a stack
    // of translucent cubes first read as cheap as the floor): draw on as they arrive, for a few seconds at most.
    for (const double Until = FPlatformTime::Seconds() + ShaderWaitSeconds;
         GShaderCompilingManager && GShaderCompilingManager->IsCompiling() && FPlatformTime::Seconds() < Until;)
    {
        GShaderCompilingManager->ProcessAsyncResults(false, false);
        DrawViewportFramesForMcp(Viewport, Client.GetWorld(), 1);
    }
    TArray<FColor> Pixels;
    Viewport->ReadPixels(Pixels, FReadSurfaceDataFlags(RCM_UNorm));
    const FIntPoint Size = Viewport->GetSizeXY();
    Client.SetViewMode(Was);
    if (bSwitch)
    {
        Client.SetGameView(false);
    }
    Client.Invalidate();
    DrawViewportFramesForMcp(Viewport, Client.GetWorld(), 1);
    if (Pixels.Num() != Size.X * Size.Y)
    {
        Out->SetStringField(TEXT("note"), TEXT("the shader complexity view could not be read"));
        return Out;
    }
    TArray<FColor> Steps;
    for (const FLinearColor& Step : Ramp)
    {
        Steps.Add(Step.ToFColor(true));
    }
    int32 High = 0, Cells = 0;
    double Sum = 0.0;
    TMap<const AActor*, TPair<int32, int32>> ByActor; // cells, of them at a high cost
    for (int32 Y = 0; Y < Frame.Height; ++Y)
    {
        for (int32 X = 0; X < Frame.Width; ++X)
        {
            const int32 PixelX = FMath::Min(X * Frame.Block + Frame.Block / 2, Size.X - 1);
            const int32 PixelY = FMath::Min(Y * Frame.Block + Frame.Block / 2, Size.Y - 1);
            if (IsOverlay(PixelX, PixelY, Size))
            {
                continue;
            }
            const FColor& Pixel = Pixels[PixelY * Size.X + PixelX];
            int32 Nearest = 0;
            int32 NearestDistance = MAX_int32;
            for (int32 Index = 0; Index < Steps.Num(); ++Index)
            {
                const int32 Distance = FMath::Square(Pixel.R - Steps[Index].R) + FMath::Square(Pixel.G - Steps[Index].G) +
                                       FMath::Square(Pixel.B - Steps[Index].B);
                Nearest = Distance < NearestDistance ? Index : Nearest;
                NearestDistance = FMath::Min(Distance, NearestDistance);
            }
            const double Cost = double(Nearest) / (Steps.Num() - 1);
            const bool bHigh = Cost >= HighCost;
            ++Cells;
            Sum += Cost;
            High += bHigh ? 1 : 0;
            TPair<int32, int32>& Entry = ByActor.FindOrAdd(Frame.Actor[Y * Frame.Width + X]);
            ++Entry.Key;
            Entry.Value += bHigh ? 1 : 0;
        }
    }
    TArray<TPair<const AActor*, TPair<int32, int32>>> Costly;
    for (const TPair<const AActor*, TPair<int32, int32>>& Entry : ByActor)
    {
        if (Entry.Key && Entry.Value.Value * 100 >= Cells) // a high cost over at least 1% of the frame
        {
            Costly.Emplace(Entry.Key, Entry.Value);
        }
    }
    Costly.Sort([](const auto& A, const auto& B) { return A.Value.Value > B.Value.Value; });
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (int32 Index = 0; Index < Costly.Num() && Index < CostlyActors; ++Index)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorName"), McpActorRef(Costly[Index].Key));
        Row->SetNumberField(TEXT("frameShare"), FMath::RoundToDouble(1000.0 * Costly[Index].Value.Value / Cells) / 10.0);
        Row->SetNumberField(TEXT("ofItself"), FMath::RoundToDouble(1000.0 * Costly[Index].Value.Value / Costly[Index].Value.Key) / 10.0);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    Out->SetNumberField(TEXT("high"), Cells > 0 ? FMath::RoundToDouble(1000.0 * High / Cells) / 10.0 : 0.0);
    Out->SetNumberField(TEXT("mean"), Cells > 0 ? FMath::RoundToDouble(100.0 * Sum / Cells) / 100.0 : 0.0);
    Out->SetArrayField(TEXT("actors"), Rows);
    return Out;
}
} // namespace McpPerception
