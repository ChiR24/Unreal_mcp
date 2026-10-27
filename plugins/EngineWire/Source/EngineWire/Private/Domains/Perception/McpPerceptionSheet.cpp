// Copyright (c) 2024 EngineWire Contributors
//
// inspect contact_sheet: one image of an actor from several sides, since one camera hides a sunk foot, a missing back
// or a wrong scale. Front, back, left, right and top are true elevations (orthographic, measured in the actor's own
// turned frame, so its front faces the camera however it is turned) with a grid every round number of centimetres; the
// three-quarter view is in perspective. Only the actor and the actors whose drawn boxes come within reach of it are
// drawn, so a wall farther off never hides it, while the floor it stands in stays; what is near enough to be drawn, it
// shows through. Each view is three scene captures (normals and depth of everything drawn, depth of the actor alone),
// shaded as clay by McpPerceptionSheetDraw: the level viewport is not moved.

#include "Domains/Perception/McpPerceptionSheet.h"

#include "Algo/Find.h"
#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "Domains/Environment/Inspection/McpEnvInspectViewIds.h"
#include "Domains/Environment/McpEnvShared.h"
#include "Domains/Perception/McpPerception.h"
#include "EngineUtils.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace McpPerception
{
namespace
{
constexpr int32 ShownLimit = 64;           // actors drawn besides the subject
constexpr double SheetMargin = 1.15;       // the subject fills a tile less this much
constexpr float ThreeQuarterFov = 40.f;
constexpr double GridLinesAcross = 6.0;

struct FSheetView
{
    const TCHAR* Name;
    const TCHAR* Label;
    FVector Toward; // from the subject to the camera, in its own turned frame (X ahead, Y right, Z up)
    bool bOrtho;
};
const FSheetView SheetViews[] = {
    {TEXT("front"), TEXT("front"), FVector(1.0, 0.0, 0.0), true},    {TEXT("back"), TEXT("back"), FVector(-1.0, 0.0, 0.0), true},
    {TEXT("left"), TEXT("left"), FVector(0.0, -1.0, 0.0), true},     {TEXT("right"), TEXT("right"), FVector(0.0, 1.0, 0.0), true},
    {TEXT("top"), TEXT("top"), FVector(0.0, 0.0, 1.0), true},
    {TEXT("three_quarter"), TEXT("three quarter"), FVector(0.61, 0.61, 0.5), false},
};

// 1, 2 or 5 times a power of ten: about GridLinesAcross lines across Width centimetres.
double GridStep(double Width)
{
    const double Raw = Width / GridLinesAcross;
    const double Power = FMath::Pow(10.0, FMath::FloorToDouble(FMath::LogX(10.0, Raw)));
    const double Unit = Raw / Power;
    return Power * (Unit >= 5.0 ? 5.0 : Unit >= 2.0 ? 2.0 : 1.0);
}

// Lines every Step centimetres, one passing Offset centimetres from the tile's middle, as pixels from its edge.
TArray<float> GridLines(double Offset, double Step, double Half, double PixelsPerCm, int32 TileSize, bool bDown)
{
    TArray<float> Lines;
    for (double Cm = Offset - Step * FMath::FloorToDouble((Offset + Half) / Step); Cm <= Half; Cm += Step)
    {
        Lines.Add(float(TileSize * 0.5 + (bDown ? -Cm : Cm) * PixelsPerCm));
    }
    return Lines;
}
} // namespace

bool HandleContactSheet(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                        TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const auto Refuse = [&](const FString& Message, const TCHAR* Code) { Bridge.SendAutomationError(Socket, RequestId, Message, Code); return true; };
    if (GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor)
    {
        return Refuse(TEXT("contact_sheet draws the level being edited; stop Play In Editor first (Simulate is fine)."), TEXT("PIE_RUNNING"));
    }
    const FString Name = GetJsonStringField(Payload, TEXT("actorName"));
    AActor* Subject = McpHandlerUtils::FindActorByName(Name);
    if (!Subject)
    {
        return Refuse(FString::Printf(TEXT("No actor is named '%s'."), *Name), TEXT("ACTOR_NOT_FOUND"));
    }
    TArray<const FSheetView*> Views;
    for (const FString& Asked : McpHandlerUtils::GetStringArrayField(Payload, TEXT("views")))
    {
        const FSheetView* View = Algo::FindByPredicate(SheetViews, [&Asked](const FSheetView& Known) { return Asked.Equals(Known.Name, ESearchCase::IgnoreCase); });
        if (!View)
        {
            return Refuse(FString::Printf(TEXT("views takes front, back, left, right, top and three_quarter; '%s' is none of them."), *Asked), TEXT("INVALID_ARGUMENT"));
        }
        Views.AddUnique(View);
    }
    if (Views.Num() == 0)
    {
        for (const FSheetView& All : SheetViews)
        {
            Views.Add(&All); // all six when none is named
        }
    }
    const int32 TileSize = FMath::Clamp(GetJsonIntField(Payload, TEXT("tileSize"), 320), 128, 480);
    // The subject's drawn box in its own turned frame (yaw only, so top stays up).
    const FTransform Frame(FRotator(0.0, Subject->GetActorRotation().Yaw, 0.0), Subject->GetActorLocation());
    const FBox Local = McpPlacement::DrawnBoundsIn(*Subject, Frame);
    if (!Local.IsValid)
    {
        return Refuse(FString::Printf(TEXT("'%s' draws nothing the game shows (no visible mesh), so there is nothing to look at."), *Subject->GetActorLabel()), TEXT("NOTHING_DRAWN"));
    }
    const FVector Middle = Frame.TransformPosition(Local.GetCenter());
    const FVector Extent = Local.GetExtent();
    // What the views draw: the subject and what comes within reach of its box (the floor it stands in, what rests on it).
    const FBox Reach = McpPlacement::DrawnBounds(*Subject).ExpandBy(FMath::Max(50.0, 0.25 * Local.GetSize().GetMax()));
    TArray<AActor*> Shown = {Subject};
    TArray<FBox> ShownBoxes = {McpPlacement::DrawnBounds(*Subject)};
    TArray<TSharedPtr<FJsonValue>> Beside;
    for (TActorIterator<AActor> It(Subject->GetWorld()); It && Shown.Num() < ShownLimit; ++It)
    {
        // As describe_relations: no lights, volumes or subsystem debug-draw actors, whose boxes can span the level.
        const bool bSkip = *It == Subject || It->IsHidden() || McpPlacement::IsBoundsOnlyActor(*It);
        const FBox Box = bSkip ? FBox(ForceInit) : McpPlacement::DrawnBounds(**It);
        if (Box.IsValid && Box.Intersect(Reach))
        {
            Shown.Add(*It);
            ShownBoxes.Add(Box);
            Beside.Add(MakeShared<FJsonValueString>(It->GetActorLabel()));
        }
    }
    const TArray<AActor*> Alone = {Subject};
    TArray<Sheet::FTile> Tiles;
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FSheetView* View : Views)
    {
        const bool bTop = View->Toward.Z > 0.99;
        const bool bOrtho = View->bOrtho;
        // Top looks straight down with the front at the bottom of the tile, as plans sit above elevations.
        const FRotator Own = bTop ? FRotator(-90.0, 180.0, 0.0) : (-View->Toward).Rotation();
        const FMatrix Axes = FRotationMatrix(Own);
        const FVector Right = Axes.GetScaledAxis(EAxis::Y), Up = Axes.GetScaledAxis(EAxis::Z);
        const double Half = FMath::Max(FMath::Abs(Right.X) * Extent.X + FMath::Abs(Right.Y) * Extent.Y + FMath::Abs(Right.Z) * Extent.Z,
                                       FMath::Abs(Up.X) * Extent.X + FMath::Abs(Up.Y) * Extent.Y + FMath::Abs(Up.Z) * Extent.Z) * SheetMargin + 5.0;
        // An elevation looks from outside everything drawn: from inside a floor slab none of its faces show.
        const FVector Toward = Frame.TransformVector(View->Toward.GetSafeNormal());
        double Beyond = Extent.Size();
        for (const FBox& Box : ShownBoxes)
        {
            Beyond = FMath::Max(Beyond, FVector::DotProduct(Box.GetCenter() - Middle, Toward) + FVector::DotProduct(Box.GetExtent(), Toward.GetAbs()));
        }
        const double Distance = bOrtho ? Beyond + 100.0 : Extent.Size() * SheetMargin / FMath::Sin(FMath::DegreesToRadians(ThreeQuarterFov * 0.5));
        const FVector Camera = Middle + Toward * Distance;
        const FRotator Look = (Frame.GetRotation() * FQuat(Own)).Rotator();
        const FVector WorldRight = Look.RotateVector(FVector::RightVector), WorldUp = Look.RotateVector(FVector::UpVector);
        const auto Pass = [&](ESceneCaptureSource Source, const TArray<AActor*>& Actors) {
            return McpEnvironmentHandlers::McpCapturePass(Subject->GetWorld(), Camera, Look, ThreeQuarterFov, FIntPoint(TileSize, TileSize),
                                                          Source, bOrtho ? float(2.0 * Half) : 0.f, &Actors);
        };
        const TArray<FLinearColor> Normals = Pass(SCS_Normal, Shown), Depth = Pass(SCS_SceneDepth, Shown), SubjectDepth = Pass(SCS_SceneDepth, Alone);
        if (Normals.Num() != TileSize * TileSize || Depth.Num() != Normals.Num() || SubjectDepth.Num() != Normals.Num())
        {
            return Refuse(FString::Printf(TEXT("The %s view could not be captured."), View->Label), TEXT("CAPTURE_FAILED"));
        }
        Sheet::FTile Tile;
        Tile.Label = View->Label;
        Tile.Pixels = Sheet::Shade(Normals, Depth, SubjectDepth, Look.Vector(), WorldRight, WorldUp);
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("view"), View->Name);
        if (bOrtho)
        {
            // Across through the subject's pivot; up at round heights above the world's zero in an elevation.
            const double Step = GridStep(2.0 * Half);
            const double PixelsPerCm = TileSize / (2.0 * Half);
            Tile.Columns = GridLines(FVector::DotProduct(Subject->GetActorLocation() - Camera, WorldRight), Step, Half, PixelsPerCm, TileSize, false);
            Tile.Rows = GridLines(bTop ? FVector::DotProduct(Subject->GetActorLocation() - Camera, WorldUp) : -Camera.Z, Step, Half, PixelsPerCm, TileSize, true);
            Tile.Label += FString::Printf(TEXT("   grid %s cm"), *FString::SanitizeFloat(Step, 0));
            Row->SetNumberField(TEXT("grid"), Step);
        }
        Tiles.Add(MoveTemp(Tile));
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    const FString Title = FString::Printf(TEXT("%s   %.0f x %.0f x %.0f cm"), *Subject->GetActorLabel(), 2.0 * Extent.X, 2.0 * Extent.Y, 2.0 * Extent.Z);
    int32 Width = 0, Height = 0;
    const TArray<FColor> Pixels = Sheet::Compose(Title, Tiles, TileSize, Width, Height);
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
    const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Captures") /
        FString::Printf(TEXT("ContactSheet_%s_%s.png"), *FPaths::MakeValidFileName(Subject->GetActorLabel()), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (FFileHelper::SaveArrayToFile(Png, *File))
    {
        Result->SetStringField(TEXT("file"), File);
    }
    TSharedPtr<FJsonObject> Size = MakeShared<FJsonObject>();
    Size->SetNumberField(TEXT("x"), FMath::RoundToDouble(2.0 * Extent.X));
    Size->SetNumberField(TEXT("y"), FMath::RoundToDouble(2.0 * Extent.Y));
    Size->SetNumberField(TEXT("z"), FMath::RoundToDouble(2.0 * Extent.Z));
    Result->SetObjectField(TEXT("size"), Size);
    Result->SetArrayField(TEXT("views"), Rows);
    Result->SetArrayField(TEXT("shown"), Beside);
    Result->SetNumberField(TEXT("width"), Width);
    Result->SetNumberField(TEXT("height"), Height);
    if (Png.Num() <= MaxScreenshotPngBytesForBase64ForMcp)
    {
        Result->SetStringField(TEXT("imageBase64"), FBase64::Encode(Png.GetData(), static_cast<uint32>(Png.Num())));
        Result->SetStringField(TEXT("mimeType"), TEXT("image/png"));
    }
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Contact sheet of %s: %d view(s), %d x %d."),
        *Subject->GetActorLabel(), Tiles.Num(), Width, Height), Result, FString());
    return true;
}
} // namespace McpPerception
