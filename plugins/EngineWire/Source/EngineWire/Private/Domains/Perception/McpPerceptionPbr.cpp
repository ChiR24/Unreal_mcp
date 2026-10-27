// Copyright (c) 2024 EngineWire Contributors
//
// audit_materials' pbr check: the viewport drawn in the Buffer Visualization view mode once each for the G-buffer's base
// colour, metallic and roughness, each cell read where the frame read its actor (the hit proxy at the cell's centre),
// then what every lit, opaque actor in view shows judged against physically based ranges. The view mode, the buffer and
// game view are put back as they were.

#include "Domains/Perception/McpPerception.h"

#include "Components/PrimitiveComponent.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "EditorViewportClient.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "ShaderCompiler.h"

namespace McpPerception
{
namespace
{
constexpr double PbrShaderWaitSeconds = 6.0; // one budget for the three buffers' shaders, compiled on first use
constexpr int32 PbrMinCells = 20;
constexpr int32 PbrMaxRows = 30;
const TCHAR* const PbrBuffers[] = {TEXT("BaseColor"), TEXT("Metallic"), TEXT("Roughness")};

struct FPbrActor
{
    TArray<FColor> Base;
    TArray<float> Metal;
    TArray<float> Rough;
};

// A buffer's grey as drawn back to its value: the view shows the value through the display's sRGB encoding.
float PbrValue(uint8 Shown)
{
    const float Encoded = Shown / 255.f;
    return Encoded <= 0.04045f ? Encoded / 12.92f : FMath::Pow((Encoded + 0.055f) / 1.055f, 2.4f);
}

// Why an actor's G-buffer says nothing of its look: an unlit material writes black base colour and a translucent one
// writes none (what is behind shows through). Empty when every material it draws is lit and opaque.
FString PbrSkipReason(const AActor* Actor)
{
    TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
    for (const UPrimitiveComponent* Primitive : Primitives)
    {
        TArray<UMaterialInterface*> Materials;
        Primitive->GetUsedMaterials(Materials);
        for (const UMaterialInterface* Material : Materials)
        {
            if (Material && Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit))
            {
                return FString::Printf(TEXT("unlit material %s"), *Material->GetName());
            }
            if (Material && Material->GetBlendMode() != BLEND_Opaque && Material->GetBlendMode() != BLEND_Masked)
            {
                return FString::Printf(TEXT("translucent material %s"), *Material->GetName());
            }
        }
    }
    return FString();
}

// The viewport drawn once per buffer: the pixels of each, in PbrBuffers' order, empty when one could not be read.
TArray<TArray<FColor>> PbrReadBuffers(FEditorViewportClient& Client, FIntPoint& OutSize)
{
    FViewport* Viewport = Client.Viewport;
    const EViewModeIndex Was = Client.GetViewMode();
    const FName WasBuffer = Client.CurrentBufferVisualizationMode;
    const bool bSwitch = !Client.IsInGameView();
    if (bSwitch)
    {
        Client.SetGameView(true); // game view first and off last: it saves and puts back the show flags
    }
    TArray<TArray<FColor>> Out;
    const double Until = FPlatformTime::Seconds() + PbrShaderWaitSeconds;
    for (const TCHAR* Buffer : PbrBuffers)
    {
        Client.ChangeBufferVisualizationMode(FName(Buffer)); // also sets VMI_VisualizeBuffer
        Client.Invalidate();
        DrawViewportFramesForMcp(Viewport, Client.GetWorld(), 2);
        while (GShaderCompilingManager && GShaderCompilingManager->IsCompiling() && FPlatformTime::Seconds() < Until)
        {
            GShaderCompilingManager->ProcessAsyncResults(false, false);
            DrawViewportFramesForMcp(Viewport, Client.GetWorld(), 1);
        }
        Viewport->ReadPixels(Out.AddDefaulted_GetRef(), FReadSurfaceDataFlags(RCM_UNorm));
    }
    OutSize = Viewport->GetSizeXY();
    Client.CurrentBufferVisualizationMode = WasBuffer;
    Client.SetViewMode(Was);
    if (bSwitch)
    {
        Client.SetGameView(false);
    }
    Client.Invalidate();
    DrawViewportFramesForMcp(Viewport, Client.GetWorld(), 1);
    const bool bRead = !Out.ContainsByPredicate([&OutSize](const TArray<FColor>& Pixels) { return Pixels.Num() != OutSize.X * OutSize.Y; });
    return bRead ? Out : TArray<TArray<FColor>>();
}

template <typename T>
T PbrMedian(TArray<T> Values)
{
    Values.Sort();
    return Values.Num() > 0 ? Values[Values.Num() / 2] : T();
}

double PbrRound(double Value, double Scale)
{
    return FMath::RoundToDouble(Value * Scale) / Scale;
}
} // namespace

TArray<FString> PbrFindings(const TArray<FColor>& Base, const TArray<float>& Metal, const TArray<float>& Rough)
{
    int32 Dark = 0, Bright = 0, Between = 0, Mirror = 0, Metals = 0, DarkMetals = 0;
    for (int32 Cell = 0; Cell < Base.Num(); ++Cell)
    {
        const uint8 Most = FMath::Max3(Base[Cell].R, Base[Cell].G, Base[Cell].B);
        const bool bMetal = Metal[Cell] >= 0.9f; // a metal's base colour is its reflectance: bright, never under charcoal
        Metals += bMetal ? 1 : 0;
        DarkMetals += bMetal && Most < 155 ? 1 : 0;
        Dark += !bMetal && Most < 30 ? 1 : 0;
        Bright += !bMetal && FMath::Min3(Base[Cell].R, Base[Cell].G, Base[Cell].B) > 240 ? 1 : 0; // silver is near 250
        Between += Metal[Cell] > 0.1f && Metal[Cell] < 0.9f ? 1 : 0;
        Mirror += Rough[Cell] < 0.05f ? 1 : 0;
    }
    const int32 Cells = Base.Num();
    TArray<FString> Out;
    const auto Note = [&Out](bool bOn, const TCHAR* Kind) { if (bOn) { Out.Add(Kind); } };
    Note(Dark * 4 >= Cells && Dark > 0, TEXT("too_dark"));
    Note(Bright * 4 >= Cells && Bright > 0, TEXT("too_bright"));
    Note(DarkMetals * 4 >= Metals && DarkMetals > 0, TEXT("dark_metal"));
    Note(Between * 4 >= Cells && Between > 0, TEXT("partial_metallic"));
    Note(Mirror * 2 >= Cells && Mirror > 0, TEXT("mirror_roughness"));
    return Out;
}

TSharedPtr<FJsonObject> PbrAsRendered(FEditorViewportClient& Client, const FFrame& Frame)
{
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    FIntPoint Size;
    const TArray<TArray<FColor>> Buffers = PbrReadBuffers(Client, Size);
    if (Buffers.Num() != int32(UE_ARRAY_COUNT(PbrBuffers)))
    {
        Out->SetStringField(TEXT("note"), TEXT("the buffer visualization view could not be read"));
        return Out;
    }
    TMap<AActor*, FPbrActor> ByActor;
    for (int32 Y = 0; Y < Frame.Height; ++Y)
    {
        for (int32 X = 0; X < Frame.Width; ++X)
        {
            AActor* Actor = Frame.Actor[Y * Frame.Width + X];
            const int32 Pixel = FMath::Min(Y * Frame.Block + Frame.Block / 2, Size.Y - 1) * Size.X + FMath::Min(X * Frame.Block + Frame.Block / 2, Size.X - 1);
            if (Actor)
            {
                FPbrActor& Entry = ByActor.FindOrAdd(Actor);
                Entry.Base.Add(Buffers[0][Pixel]);
                Entry.Metal.Add(PbrValue(Buffers[1][Pixel].R));
                Entry.Rough.Add(PbrValue(Buffers[2][Pixel].R));
            }
        }
    }
    ByActor.ValueStableSort([](const FPbrActor& L, const FPbrActor& R) { return L.Base.Num() > R.Base.Num(); });
    TArray<TSharedPtr<FJsonValue>> Rows, Clean, Skipped; // flagged actors first, then the rest, PbrMaxRows in all
    int32 Judged = 0, Flagged = 0;
    for (const TPair<AActor*, FPbrActor>& Pair : ByActor)
    {
        const FPbrActor& Cells = Pair.Value;
        if (Cells.Base.Num() < PbrMinCells)
        {
            continue;
        }
        const FString Skip = PbrSkipReason(Pair.Key);
        if (!Skip.IsEmpty())
        {
            TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("actorName"), Pair.Key->GetActorLabel());
            Row->SetStringField(TEXT("reason"), Skip);
            Skipped.Add(MakeShared<FJsonValueObject>(Row));
            continue;
        }
        ++Judged;
        const TArray<FString> Findings = PbrFindings(Cells.Base, Cells.Metal, Cells.Rough);
        Flagged += Findings.Num() > 0 ? 1 : 0;
        TArray<uint8> Reds, Greens, Blues;
        for (const FColor& Colour : Cells.Base)
        {
            Reds.Add(Colour.R);
            Greens.Add(Colour.G);
            Blues.Add(Colour.B);
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorName"), Pair.Key->GetActorLabel());
        Row->SetNumberField(TEXT("share"), PbrRound(double(Cells.Base.Num()) / (Frame.Width * Frame.Height), 1000.0));
        TArray<TSharedPtr<FJsonValue>> Median;
        for (const TArray<uint8>* Channel : {&Reds, &Greens, &Blues})
        {
            Median.Add(MakeShared<FJsonValueNumber>(PbrMedian(*Channel)));
        }
        Row->SetArrayField(TEXT("baseColor"), Median);
        Row->SetNumberField(TEXT("metallic"), PbrRound(PbrMedian(Cells.Metal), 100.0));
        Row->SetNumberField(TEXT("roughness"), PbrRound(PbrMedian(Cells.Rough), 100.0));
        TArray<TSharedPtr<FJsonValue>> Kinds;
        for (const FString& Kind : Findings)
        {
            Kinds.Add(MakeShared<FJsonValueString>(Kind));
        }
        Row->SetArrayField(TEXT("findings"), Kinds);
        (Findings.Num() > 0 ? Rows : Clean).Add(MakeShared<FJsonValueObject>(Row));
    }
    Rows.Append(Clean);
    Rows.SetNum(FMath::Min(Rows.Num(), PbrMaxRows));
    Out->SetNumberField(TEXT("judged"), Judged);
    Out->SetNumberField(TEXT("flagged"), Flagged);
    Out->SetArrayField(TEXT("actors"), Rows);
    if (Skipped.Num() > 0)
    {
        Out->SetArrayField(TEXT("skipped"), Skipped);
    }
    return Out;
}
} // namespace McpPerception
