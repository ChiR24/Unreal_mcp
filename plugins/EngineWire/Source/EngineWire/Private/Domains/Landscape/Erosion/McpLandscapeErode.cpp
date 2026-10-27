// Copyright (c) 2024 EngineWire Contributors
//
// build_environment erode_landscape: a landscape worn the way water wears ground, so that every cell drains. Its heights
// over a region (the region's border held, so the result joins the rest) go through McpTerrainErosion: depressions
// filled, valleys cut by stream power, gullies by droplets, an optional talus slump, filled again; then written to the
// edit layer sculpt writes, as one undo step, and answered with the rivers the ground now carries.

#include "Domains/Landscape/Erosion/McpTerrainErosion.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Dom/JsonObject.h"
#include "Domains/Landscape/McpLandscapeLookup.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "HAL/PlatformTime.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"

namespace McpLandscapeErosion
{
namespace
{
constexpr int64 ErodeMaxCells = 4194304;      // 2048 x 2048
constexpr int32 ErodeSteps = 20;              // stream-power steps: enough that an F above 1 stays smooth
constexpr double ErodeDropletSeconds = 15.0;  // a guard only: the default droplet count fits well inside it

int32 ErodeLayerCount(const ALandscape* Landscape)
{
    if (!Landscape->HasLayersContent())
    {
        return 0;
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
    return Landscape->GetLayersConst().Num();
#else
    return Landscape->GetLayerCount();
#endif
}

// A raw sample is 32768 at the landscape's own height and 128 to one Z-scale step (scale.Z cm).
double ErodeToCm(uint16 Raw, double ScaleZ)
{
    return (static_cast<double>(Raw) - 32768.0) / 128.0 * ScaleZ;
}

// Rounding is monotone, so a path that never climbs in cm never climbs in the written samples either.
uint16 ErodeToRaw(double Cm, double ScaleZ)
{
    return static_cast<uint16>(FMath::Clamp(FMath::RoundToDouble(Cm / ScaleZ * 128.0 + 32768.0), 0.0, 65535.0));
}

// Value to the nearest 1 / Per (Per 10: tenths). Dividing by Per, not multiplying by 0.1: 594 x 0.1 is
// 59.400000000000006, 594 / 10 is 59.4.
double ErodeRound(double Value, double Per)
{
    return FMath::RoundToDouble(Value * Per) / Per;
}

TArray<TSharedPtr<FJsonValue>> ErodeRivers(const TArray<McpTerrainErosion::FRiver>& Rivers, const ALandscape* Landscape, int32 MinX, int32 MinY)
{
    const FTransform ToWorld = Landscape->GetActorTransform();
    const double ScaleZ = Landscape->GetActorScale3D().Z;
    TArray<TSharedPtr<FJsonValue>> Out;
    for (const McpTerrainErosion::FRiver& River : Rivers)
    {
        TArray<TSharedPtr<FJsonValue>> Points;
        for (const FVector& Point : River.Points)
        {
            // The landscape's own space (a vertex per unit, height in Z-scale steps) through its whole transform.
            const FVector World = ToWorld.TransformPosition(FVector(MinX + Point.X, MinY + Point.Y, Point.Z / ScaleZ));
            const TSharedPtr<FJsonObject> Spot = MakeShared<FJsonObject>();
            Spot->SetNumberField(TEXT("x"), ErodeRound(World.X, 1.0));
            Spot->SetNumberField(TEXT("y"), ErodeRound(World.Y, 1.0));
            Spot->SetNumberField(TEXT("z"), ErodeRound(World.Z, 1.0));
            Points.Add(MakeShared<FJsonValueObject>(Spot));
        }
        const TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("catchmentM2"), ErodeRound(River.CatchmentM2, 1.0));
        Entry->SetNumberField(TEXT("lengthM"), ErodeRound(River.LengthCm / 100.0, 10.0));
        Entry->SetArrayField(TEXT("points"), Points);
        Out.Add(MakeShared<FJsonValueObject>(Entry));
    }
    return Out;
}
} // namespace

bool HandleErodeLandscape(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const double Start = FPlatformTime::Seconds();
    ULandscapeInfo* Info = nullptr;
    ALandscape* Landscape = McpLandscapeHandlers::ResolveLandscapeOrReply(Bridge, RequestId, Payload, Socket, &Info);
    if (!Landscape)
    {
        return true;
    }
    int32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    const FVector Scale = Landscape->GetActorScale3D();
    if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY) || Scale.Z <= 0.0)
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("The landscape has no components, or a Z scale of 0 or below."), TEXT("INVALID_LANDSCAPE"));
        return true;
    }
    const TSharedPtr<FJsonObject>* Region = nullptr;
    if (Payload->TryGetObjectField(TEXT("region"), Region) && Region)
    {
        int32 X1 = MinX, Y1 = MinY, X2 = MaxX, Y2 = MaxY;
        (*Region)->TryGetNumberField(TEXT("minX"), X1);
        (*Region)->TryGetNumberField(TEXT("minY"), Y1);
        (*Region)->TryGetNumberField(TEXT("maxX"), X2);
        (*Region)->TryGetNumberField(TEXT("maxY"), Y2);
        if (X1 < MinX || Y1 < MinY || X2 > MaxX || Y2 > MaxY || X2 - X1 < 4 || Y2 - Y1 < 4)
        {
            Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("region must lie within the landscape's vertices, x %d to %d and y %d to %d, and span 5 or more each way."),
                                                                        MinX, MaxX, MinY, MaxY), TEXT("INVALID_REGION"));
            return true;
        }
        MinX = X1;
        MinY = Y1;
        MaxX = X2;
        MaxY = Y2;
    }
    const int32 W = MaxX - MinX + 1, H = MaxY - MinY + 1;
    const int64 Cells = static_cast<int64>(W) * H;
    if (Cells > ErodeMaxCells)
    {
        Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("The landscape has %lld vertices; erode at most %lld at once: name a region."), Cells, ErodeMaxCells),
                                   TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const bool bFill = GetJsonBoolField(Payload, TEXT("fillDepressions"), true);
    const bool bDryRun = GetJsonBoolField(Payload, TEXT("dryRun"), false);
    const double Valleys = FMath::Clamp(GetJsonNumberField(Payload, TEXT("valleys"), 0.3), 0.0, 1.0);
    const int32 DropletCount = FMath::Clamp(GetJsonIntField(Payload, TEXT("droplets"), static_cast<int32>(FMath::Min<int64>(Cells / 4, 200000))), 0, 1000000);
    const double Talus = FMath::Clamp(GetJsonNumberField(Payload, TEXT("talusAngle"), 0.0), 0.0, 80.0);
    const int32 Seed = GetJsonIntField(Payload, TEXT("seed"), 1);
    const double CellArea = FMath::Abs(Scale.X * Scale.Y);
    double RiverAreaM2 = GetJsonNumberField(Payload, TEXT("riverAreaM2"), 0.0);
    RiverAreaM2 = RiverAreaM2 > 0.0 ? RiverAreaM2 : 0.01 * Cells * CellArea / 10000.0;

    // Read the edit layer the write goes to (sculpt's: the one being edited, else the first). With no layer selected
    // the interface reads the merged surface, and a dryRun would answer for heights the edit never touches.
    TArray<uint16> Raw;
    Raw.SetNumZeroed(W * H);
    const bool bLayered = Landscape->HasLayersContent();
    const FGuid Selected = Landscape->GetEditingLayer();
    if (bLayered)
    {
        Landscape->SetEditingLayer(McpLandscapeHandlers::LandscapeWriteLayer(Landscape));
    }
    {
        int32 X1 = MinX, Y1 = MinY, X2 = MaxX, Y2 = MaxY;
        FLandscapeEditDataInterface Read(Info);
        Read.GetHeightData(X1, Y1, X2, Y2, Raw.GetData(), 0);
    }
    if (bLayered)
    {
        Landscape->SetEditingLayer(Selected);
    }

    McpTerrainErosion::FGrid Grid;
    Grid.Width = W;
    Grid.Height = H;
    Grid.CellX = FMath::Abs(Scale.X);
    Grid.CellY = FMath::Abs(Scale.Y);
    Grid.Heights.SetNumUninitialized(W * H);
    for (int32 Index = 0; Index < W * H; ++Index)
    {
        Grid.Heights[Index] = ErodeToCm(Raw[Index], Scale.Z);
    }
    const TArray<double> Before = Grid.Heights;
    const McpTerrainErosion::FFill Filled = bFill ? McpTerrainErosion::Fill(Grid) : McpTerrainErosion::FFill();
    // valleys: how far an erosion front travels up the largest river, as a share of the region's longer side.
    const double Wave = Valleys * FMath::Max(W * Grid.CellX, H * Grid.CellY) / ErodeSteps;
    for (int32 Step = 0; Valleys > 0.0 && Step < ErodeSteps; ++Step)
    {
        McpTerrainErosion::StreamPower(Grid, McpTerrainErosion::Flow(Grid), Wave);
    }
    const int32 Ran = DropletCount > 0 ? McpTerrainErosion::Droplets(Grid, DropletCount, Seed, Start + ErodeDropletSeconds) : 0;
    if (Talus > 0.0)
    {
        McpTerrainErosion::Thermal(Grid, Talus, 10);
    }
    if (bFill)
    {
        McpTerrainErosion::Fill(Grid);
    }
    // Judged on what is written: the rounded samples, back in cm.
    double Lowered = 0.0, Raised = 0.0;
    int32 Changed = 0;
    for (int32 Index = 0; Index < W * H; ++Index)
    {
        Raw[Index] = ErodeToRaw(Grid.Heights[Index], Scale.Z);
        Grid.Heights[Index] = ErodeToCm(Raw[Index], Scale.Z);
        const double Delta = Grid.Heights[Index] - Before[Index];
        Lowered = FMath::Min(Lowered, Delta);
        Raised = FMath::Max(Raised, Delta);
        Changed += Delta != 0.0 ? 1 : 0;
    }
    const int32 Undrained = McpTerrainErosion::Undrained(Grid);
    const TArray<McpTerrainErosion::FRiver> Rivers = McpTerrainErosion::Rivers(Grid, McpTerrainErosion::Flow(Grid), RiverAreaM2 * 10000.0, 20);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (!bDryRun && Changed > 0)
    {
        const McpLandscapeHandlers::FMcpLandscapeEditScope Edit(Landscape, TEXT("Erode Landscape"), false);
        FLandscapeEditDataInterface Write(Info);
        Write.SetHeightData(MinX, MinY, MaxX, MaxY, Raw.GetData(), W, true);
        Write.Flush();
        Landscape->MarkPackageDirty();
        Edit.DescribeInto(Result);
    }
    const int32 Layers = ErodeLayerCount(Landscape);
    // Never -0 (an untouched region printed "lowered up to -0 cm"): FMath::Max(0.0, -0.0) answers its second argument.
    const double Cut = Lowered < 0.0 ? -Lowered : 0.0;
    const TSharedPtr<FJsonObject> Area = MakeShared<FJsonObject>();
    Area->SetNumberField(TEXT("minX"), MinX);
    Area->SetNumberField(TEXT("minY"), MinY);
    Area->SetNumberField(TEXT("maxX"), MaxX);
    Area->SetNumberField(TEXT("maxY"), MaxY);
    Result->SetStringField(TEXT("landscapeName"), Landscape->GetActorLabel());
    Result->SetObjectField(TEXT("region"), Area);
    Result->SetNumberField(TEXT("cells"), static_cast<double>(Cells));
    Result->SetNumberField(TEXT("changedCells"), Changed);
    Result->SetNumberField(TEXT("depressionsFilled"), Filled.Depressions);
    Result->SetNumberField(TEXT("maxLoweredCm"), ErodeRound(Cut, 10.0));
    Result->SetNumberField(TEXT("maxRaisedCm"), ErodeRound(Raised, 10.0));
    Result->SetNumberField(TEXT("dropletsRun"), Ran);
    Result->SetBoolField(TEXT("drains"), Undrained == 0);
    if (Undrained > 0)
    {
        Result->SetNumberField(TEXT("undrainedCells"), Undrained);
    }
    Result->SetNumberField(TEXT("editLayers"), Layers);
    Result->SetArrayField(TEXT("rivers"), ErodeRivers(Rivers, Landscape, MinX, MinY));
    Result->SetNumberField(TEXT("seconds"), ErodeRound(FPlatformTime::Seconds() - Start, 100.0));
    McpHandlerUtils::AddVerification(Result, Landscape);
    const FString Drainage = Undrained == 0 ? FString(TEXT("every cell drains"))
                                            : FString::Printf(TEXT("%d cell(s) drain nowhere (fillDepressions false keeps their basins)"), Undrained);
    FString Message = FString::Printf(TEXT("%s %lld vertices of '%s': %d depression(s) filled, lowered up to %.0f cm and raised up to %.0f cm, %d droplet(s)%s; %s; %d river(s)."),
                                      bDryRun ? TEXT("Would erode") : TEXT("Eroded"), Cells, *Landscape->GetActorLabel(), Filled.Depressions, Cut, Raised, Ran,
                                      Ran < DropletCount ? TEXT(" (stopped at the time limit)") : TEXT(""), *Drainage, Rivers.Num());
    if (Layers > 1)
    {
        Message += FString::Printf(TEXT(" The landscape has %d edit layers: this is one of them, and the others still add on top."), Layers);
    }
    Bridge.SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}
} // namespace McpLandscapeErosion
