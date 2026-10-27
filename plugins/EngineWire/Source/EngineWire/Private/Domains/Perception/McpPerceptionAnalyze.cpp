// Copyright (c) 2024 EngineWire Contributors
//
// inspect analyze_view (McpPerception.h): the level viewport read as describe_view reads it (McpCaptureView), cut into a
// grid of cells about 240 across (each the average of its block of pixels, and the actor drawn at its centre), then
// judged for the aspects asked. The camera moves only when location or rotation is given, as describe_view's does.

#include "Domains/Perception/McpPerception.h"

#include "Algo/Find.h"
#include "Components/DirectionalLightComponent.h"
#include "Domains/Environment/Inspection/McpEnvInspectViewIds.h"
#include "Domains/Environment/McpEnvShared.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "Foundation/McpScreenshotResample.h"

namespace McpPerception
{
namespace
{
constexpr int32 GridWidth = 240; // cells across: fine enough for a figure, coarse enough to keep every aspect quick
const TCHAR* const AllAspects[] = {TEXT("saliency"), TEXT("palette"), TEXT("light"), TEXT("cost")};
} // namespace

FFrame BuildFrame(const McpEnvironmentHandlers::FMcpViewCapture& View)
{
    using namespace McpEnvironmentHandlers;
    FFrame Frame;
    Frame.Block = FMath::Max(1, FMath::DivideAndRoundUp(View.Size.X, GridWidth));
    Frame.Width = FMath::Max(1, View.Size.X / Frame.Block);
    Frame.Height = FMath::Max(1, View.Size.Y / Frame.Block);
    TMap<uint32, FMcpViewProxy> Proxies;
    for (int32 Y = 0; Y < Frame.Height; ++Y)
    {
        for (int32 X = 0; X < Frame.Width; ++X)
        {
            FLinearColor Sum(0.f, 0.f, 0.f, 0.f);
            for (int32 Row = Y * Frame.Block; Row < (Y + 1) * Frame.Block; ++Row)
            {
                for (int32 Column = X * Frame.Block; Column < (X + 1) * Frame.Block; ++Column)
                {
                    const FColor& Pixel = View.Colors[Row * View.Size.X + Column];
                    Sum += FLinearColor(Pixel.R / 255.f, Pixel.G / 255.f, Pixel.B / 255.f, 1.f);
                }
            }
            const FLinearColor Shown = Sum / float(Frame.Block * Frame.Block);
            const FColor& Id = View.Ids[(Y * Frame.Block + Frame.Block / 2) * View.Size.X + X * Frame.Block + Frame.Block / 2];
            const uint32 Key = McpViewIdKey(Id);
            const FMcpViewProxy* Proxy = Proxies.Find(Key);
            if (!Proxy)
            {
                Proxy = &Proxies.Add(Key, McpViewProxyOf(Id));
            }
            Frame.Color.Add(Shown);
            Frame.Lab.Add(OkLab(Shown));
            Frame.Actor.Add(Proxy->Kind == EMcpViewKind::Actor ? Proxy->Actor : nullptr);
        }
    }
    return Frame;
}

namespace
{
// The scene's sun: the brightest directional light that is on; OutToSun points from the scene toward it.
bool FindSun(UWorld* World, FVector3f& OutToSun)
{
    const UDirectionalLightComponent* Sun = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        TInlineComponentArray<UDirectionalLightComponent*> Lights(*It);
        for (const UDirectionalLightComponent* Light : Lights)
        {
            Sun = Light->IsVisible() && (!Sun || Light->Intensity > Sun->Intensity) ? Light : Sun;
        }
    }
    if (Sun)
    {
        OutToSun = FVector3f(-Sun->GetForwardVector());
    }
    return Sun != nullptr;
}
} // namespace

bool HandleAnalyzeView(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    TSet<FString> Aspects;
    for (const FString& Asked : McpHandlerUtils::GetStringArrayField(Payload, TEXT("aspects")))
    {
        const TCHAR* const* Known = Algo::FindByPredicate(AllAspects, [&Asked](const TCHAR* Name) { return Asked.Equals(Name, ESearchCase::IgnoreCase); });
        if (!Known)
        {
            Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("aspects takes saliency, palette, light and cost; "
                "'%s' is none of them."), *Asked), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Aspects.Add(*Known);
    }
    if (Aspects.Num() == 0)
    {
        Aspects = {TEXT("saliency"), TEXT("palette"), TEXT("light")}; // cost redraws the viewport in another view mode
    }
    const FString SubjectName = GetJsonStringField(Payload, TEXT("subject"));
    AActor* Subject = SubjectName.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(SubjectName);
    if (!SubjectName.IsEmpty() && !Subject)
    {
        Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("No actor is named '%s'."), *SubjectName), TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    McpEnvironmentHandlers::FMcpViewCapture View;
    FString Error, Code;
    if (!McpEnvironmentHandlers::McpCaptureView(Payload, TEXT("analyze_view"), true, View, Error, Code))
    {
        Bridge.SendAutomationError(Socket, RequestId, Error, Code);
        return true;
    }
    const FFrame Frame = BuildFrame(View);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("cellsAcross"), Frame.Width);
    Result->SetNumberField(TEXT("cellsDown"), Frame.Height);
    if (Aspects.Contains(TEXT("saliency")))
    {
        Result->SetObjectField(TEXT("saliency"), Saliency(Frame, Subject));
    }
    if (Aspects.Contains(TEXT("palette")))
    {
        Result->SetObjectField(TEXT("palette"), Palette(Frame));
    }
    if (Aspects.Contains(TEXT("light")))
    {
        FVector3f ToSun = FVector3f::UpVector;
        TArray<FVector3f> Normals;
        if (Subject && FindSun(View.Client->GetWorld(), ToSun))
        {
            for (const FLinearColor& Normal : McpEnvironmentHandlers::McpCapturePass(View.Client->GetWorld(), View.Client->GetViewLocation(),
                     View.Client->GetViewRotation(), View.Client->ViewFOV, FIntPoint(Frame.Width, Frame.Height), SCS_Normal))
            {
                Normals.Add(FVector3f(Normal.R, Normal.G, Normal.B));
            }
        }
        Result->SetObjectField(TEXT("light"), Light(Frame, Subject, Normals, ToSun));
    }
    if (Aspects.Contains(TEXT("cost")))
    {
        Result->SetObjectField(TEXT("cost"), ShaderCost(*View.Client, Frame));
    }
    Result->SetObjectField(TEXT("cameraLocation"), McpHandlerUtils::VectorToJson(View.Client->GetViewLocation()));
    Result->SetObjectField(TEXT("cameraRotation"), McpHandlerUtils::RotatorToJson(View.Client->GetViewRotation()));
    // Until a material's shaders finish compiling it draws as the engine's stand-in, and every aspect reads that: a stack
    // of translucent cubes first read as cheap as the floor.
    if (const int32 Jobs = McpShaderJobsRemaining())
    {
        TArray<TSharedPtr<FJsonValue>> Warnings;
        Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%d shader job(s) were still compiling: what they cover drew "
            "with the engine's stand-in material, so this frame is not its final look or cost; call again when they finish."), Jobs)));
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Analyzed the view (%d x %d cells): %s."),
        Frame.Width, Frame.Height, *FString::Join(Aspects.Array(), TEXT(", "))), Result, FString());
    return true;
}
} // namespace McpPerception
