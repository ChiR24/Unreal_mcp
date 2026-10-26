// The top-down picture of analyze_level_layout's spaces (see McpLevelLayout.h): one square per sample, each space its
// own colour (rooms light, corridors darker), door samples white, the start green and the goal red. Where floors stack,
// the highest is drawn.
#include "Domains/Navigation/Analytics/McpLevelLayout.h"

#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace McpLevelLayout
{
namespace
{
constexpr int32 LayoutImageSide = 1024; // the longer side, in pixels, give or take a cell
constexpr int64 LayoutMaxPixels = 4096 * 4096;

FColor LayoutSpaceColor(int32 Space, bool bRoom)
{
    const float Hue = FMath::Fmod(Space * 137.508f, 360.f); // golden-angle steps keep neighbours apart
    return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue / 360.f * 255.f), bRoom ? 120 : 200, bRoom ? 235 : 150).ToFColor(true);
}
} // namespace

bool WriteImage(const FGraph& Graph, const FLayout& Layout, int32 Start, int32 Goal, const FString& Path)
{
    const int32 Px = FMath::Clamp(LayoutImageSide / FMath::Max3(Graph.GridSize.X, Graph.GridSize.Y, 1), 2, 16); // per cell
    const int32 Width = Graph.GridSize.X * Px;
    const int32 Height = Graph.GridSize.Y * Px;
    if (Width <= 0 || Height <= 0 || int64(Width) * Height > LayoutMaxPixels)
    {
        return false;
    }
    TArray<FColor> Pixels;
    Pixels.Init(FColor(24, 24, 24, 255), Width * Height);
    TArray<double> TopZ; // the highest sample of each column, the one drawn
    TopZ.Init(-TNumericLimits<double>::Max(), Graph.GridSize.X * Graph.GridSize.Y);
    for (int32 Node = 0; Node < Graph.Points.Num(); ++Node)
    {
        double& Top = TopZ[Graph.Cells[Node].Y * Graph.GridSize.X + Graph.Cells[Node].X];
        Top = FMath::Max(Top, Graph.Points[Node].Z);
    }
    const auto Paint = [&](int32 Node, const FColor& Color, int32 Inset)
    {
        const FIntPoint Cell = Graph.Cells[Node];
        for (int32 Y = Inset; Y < Px - Inset; ++Y)
        {
            for (int32 X = Inset; X < Px - Inset; ++X)
            {
                // World +Y is up in the picture, as the level reads from above with +X to the right.
                Pixels[((Graph.GridSize.Y - 1 - Cell.Y) * Px + Y) * Width + Cell.X * Px + X] = Color;
            }
        }
    };
    for (int32 Node = 0; Node < Graph.Points.Num(); ++Node)
    {
        if (Graph.Points[Node].Z < TopZ[Graph.Cells[Node].Y * Graph.GridSize.X + Graph.Cells[Node].X])
        {
            continue;
        }
        const int32 Space = Layout.SpaceOf[Node];
        const bool bDoor = Graph.Adjacency[Node].ContainsByPredicate([&](int32 Next) { return Layout.SpaceOf[Next] != Space; });
        Paint(Node, LayoutSpaceColor(Space, Layout.Spaces[Space].bRoom), 0);
        if (bDoor)
        {
            Paint(Node, FColor::White, Px / 4);
        }
    }
    if (Start != INDEX_NONE)
    {
        Paint(Start, FColor::Green, 0);
    }
    if (Goal != INDEX_NONE)
    {
        Paint(Goal, FColor::Red, 0);
    }
    IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    const TSharedPtr<IImageWrapper> Png = Module.CreateImageWrapper(EImageFormat::PNG);
    if (!Png.IsValid() || !Png->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
    {
        return false;
    }
    const TArray64<uint8> Bytes = Png->GetCompressed(100);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return Bytes.Num() > 0 && FFileHelper::SaveArrayToFile(Bytes, *Path);
}
} // namespace McpLevelLayout
