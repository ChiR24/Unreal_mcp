#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"
#include "Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectViewIds.h"
#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "EditorViewportClient.h"
#include "EngineUtils.h"
#include "HitProxies.h"

// describe_view: what the level viewport's camera sees, as text. The editor draws every primitive's id into a hit
// proxy map (what a click selects by); counted per actor it gives each one's share of the frame, where it sits and
// how bright it is drawn, so a model that cannot see a picture can still check a shot.
namespace McpEnvironmentHandlers {
FMcpViewProxy McpViewProxyOf(const FColor &Id)
{
    HHitProxy *Proxy = GetHitProxyById(FHitProxyId(Id));
    if (!Proxy)
    {
        return {};
    }
    AActor *Actor = nullptr;
    const UPrimitiveComponent *Component = nullptr;
    if (HActor *ActorProxy = HitProxyCast<HActor>(Proxy))
    {
        Actor = ActorProxy->Actor;
        Component = ActorProxy->PrimComponent;
    }
    else if (HInstancedStaticMeshInstance *Instance = HitProxyCast<HInstancedStaticMeshInstance>(Proxy))
    {
        Component = Instance->Component;
        Actor = Component ? Component->GetOwner() : nullptr;
    }
    const bool bGameDraws = Actor && !(Component && (Component->IsEditorOnly() || Component->bHiddenInGame));
    return {bGameDraws ? EMcpViewKind::Actor : EMcpViewKind::Other, bGameDraws ? Actor : nullptr};
}

namespace {
// Every second pixel of every second row: a quarter of the pixels, plenty for shares of the frame.
constexpr int32 McpViewStride = 2;

struct FMcpViewActor {
    AActor *Actor = nullptr;
    int64 Pixels = 0;
    int32 MinX = MAX_int32;
    int32 MinY = MAX_int32;
    int32 MaxX = 0;
    int32 MaxY = 0;
    double Luma = 0.0;
};

// Rounded to 1 / Scale (10 for tenths). Dividing by the scale, not multiplying by a step, keeps 61.9 from printing
// as 61.900000000000006.
double McpViewRound(double Value, double Scale)
{
    return FMath::RoundToDouble(Value * Scale) / Scale;
}
} // namespace

bool HandleInspectViewContentsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("describe_view reads the level viewport, which does not show the running game; stop Play In Editor first (Simulate is fine)."),
            TEXT("PIE_RUNNING"));
        return true;
    }
    // A level viewport under another major tab is not painted and reads back black.
    if (BringLevelEditorTabToFrontForMcp())
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("The level viewport was under another tab and has been brought forward; call again to read it."),
            TEXT("VIEWPORT_NOT_READY"));
        return true;
    }
    FEditorViewportClient *Client = GetActiveEditorViewportClientForMcp();
    FViewport *Viewport = Client ? Client->Viewport : nullptr;
    const FIntPoint Size = Viewport ? Viewport->GetSizeXY() : FIntPoint::ZeroValue;
    if (Size.X <= 0 || Size.Y <= 0)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("No level viewport with a size is open."),
            TEXT("VIEWPORT_NOT_AVAILABLE"));
        return true;
    }

    // The first frames after a camera move still carry the old view's occlusion; a few frames later they match.
    const bool bMoved = Payload->HasField(TEXT("location")) || Payload->HasField(TEXT("rotation"));
    Client->SetViewLocation(ExtractVectorField(Payload, TEXT("location"), Client->GetViewLocation()));
    Client->SetViewRotation(ExtractRotatorField(Payload, TEXT("rotation"), Client->GetViewRotation()));
    Client->Invalidate();
    DrawViewportFramesForMcp(Viewport, Client->GetWorld(), bMoved ? 3 : 1);
    TArray<FColor> Colors;
    Viewport->ReadPixels(Colors, FReadSurfaceDataFlags(RCM_UNorm));
    Viewport->InvalidateHitProxy();
    const TArray<FColor> &Ids = Viewport->GetRawHitProxyData(FIntRect(0, 0, Size.X, Size.Y));
    const int32 PixelCount = Size.X * Size.Y;
    if (Colors.Num() != PixelCount || Ids.Num() != PixelCount)
    {
        Bridge.SendAutomationError(RequestingSocket, RequestId, TEXT("The viewport's pixels or ids could not be read."),
            TEXT("CAPTURE_FAILED"));
        return true;
    }

    TMap<uint32, FMcpViewProxy> Proxies;
    TMap<AActor *, FMcpViewActor> Seen;
    TArray<int64> Histogram;
    Histogram.SetNumZeroed(256);
    int64 Samples = 0, Background = 0, Other = 0, Clipped = 0, Dark = 0;
    for (int32 Y = 0; Y < Size.Y; Y += McpViewStride)
    {
        for (int32 X = 0; X < Size.X; X += McpViewStride)
        {
            const int32 Index = Y * Size.X + X;
            const FColor &Pixel = Colors[Index];
            const double Luma = (0.2126 * Pixel.R + 0.7152 * Pixel.G + 0.0722 * Pixel.B) / 255.0;
            ++Samples;
            ++Histogram[FMath::Clamp(FMath::RoundToInt(Luma * 255.0), 0, 255)];
            Clipped += FMath::Max3(Pixel.R, Pixel.G, Pixel.B) >= 250 ? 1 : 0;
            Dark += Luma < 0.05 ? 1 : 0;
            const FColor &Id = Ids[Index];
            const uint32 Key = McpViewIdKey(Id);
            const FMcpViewProxy *Proxy = Proxies.Find(Key);
            if (!Proxy)
            {
                Proxy = &Proxies.Add(Key, McpViewProxyOf(Id));
            }
            if (Proxy->Kind != EMcpViewKind::Actor)
            {
                ++(Proxy->Kind == EMcpViewKind::Background ? Background : Other);
                continue;
            }
            FMcpViewActor &Entry = Seen.FindOrAdd(Proxy->Actor);
            Entry.Actor = Proxy->Actor;
            ++Entry.Pixels;
            Entry.Luma += Luma;
            Entry.MinX = FMath::Min(Entry.MinX, X);
            Entry.MinY = FMath::Min(Entry.MinY, Y);
            Entry.MaxX = FMath::Max(Entry.MaxX, X);
            Entry.MaxY = FMath::Max(Entry.MaxY, Y);
        }
    }

    TArray<FMcpViewActor> Rows;
    Seen.GenerateValueArray(Rows);
    Rows.Sort([](const FMcpViewActor &A, const FMcpViewActor &B) { return A.Pixels > B.Pixels; });
    const int32 Limit = FMath::Clamp(GetJsonIntField(Payload, TEXT("limit"), 20), 1, 100);
    const FVector Camera = Client->GetViewLocation();
    const auto Percent = [Samples](int64 Count) { return McpViewRound(100.0 * Count / Samples, 100.0); };
    TArray<TSharedPtr<FJsonValue>> Listed;
    for (const FMcpViewActor &Row : Rows)
    {
        if (Listed.Num() == Limit)
        {
            break;
        }
        const TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Row.Actor->GetActorLabel());
        Item->SetStringField(TEXT("class"), Row.Actor->GetClass()->GetName());
        Item->SetNumberField(TEXT("coverage"), Percent(Row.Pixels));
        TArray<TSharedPtr<FJsonValue>> Box;
        Box.Add(MakeShared<FJsonValueNumber>(McpViewRound(100.0 * Row.MinX / Size.X, 10.0)));
        Box.Add(MakeShared<FJsonValueNumber>(McpViewRound(100.0 * Row.MinY / Size.Y, 10.0)));
        Box.Add(MakeShared<FJsonValueNumber>(McpViewRound(100.0 * FMath::Min(Row.MaxX + McpViewStride, Size.X) / Size.X, 10.0)));
        Box.Add(MakeShared<FJsonValueNumber>(McpViewRound(100.0 * FMath::Min(Row.MaxY + McpViewStride, Size.Y) / Size.Y, 10.0)));
        Item->SetArrayField(TEXT("box"), Box);
        Item->SetNumberField(TEXT("brightness"), McpViewRound(Row.Luma / Row.Pixels, 100.0));
        const FBox Bounds = Row.Actor->GetComponentsBoundingBox(true);
        const double Distance = Bounds.IsValid ? FMath::Sqrt(Bounds.ComputeSquaredDistanceToPoint(Camera))
                                               : FVector::Dist(Row.Actor->GetActorLocation(), Camera);
        Item->SetNumberField(TEXT("distance"), FMath::RoundToDouble(Distance));
        Listed.Add(MakeShared<FJsonValueObject>(Item));
    }

    const auto Percentile = [&Histogram, Samples](double Fraction) {
        int64 Below = 0;
        for (int32 Bin = 0; Bin < Histogram.Num(); ++Bin)
        {
            Below += Histogram[Bin];
            if (Below >= Fraction * Samples)
            {
                return McpViewRound(Bin / 255.0, 100.0);
            }
        }
        return 1.0;
    };
    const TSharedPtr<FJsonObject> Brightness = MakeShared<FJsonObject>();
    Brightness->SetNumberField(TEXT("p10"), Percentile(0.1));
    Brightness->SetNumberField(TEXT("median"), Percentile(0.5));
    Brightness->SetNumberField(TEXT("p90"), Percentile(0.9));
    Brightness->SetNumberField(TEXT("clipped"), Percent(Clipped));
    Brightness->SetNumberField(TEXT("dark"), Percent(Dark));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetObjectField(TEXT("cameraLocation"), McpHandlerUtils::VectorToJson(Camera));
    Result->SetObjectField(TEXT("cameraRotation"), McpHandlerUtils::RotatorToJson(Client->GetViewRotation()));
    Result->SetNumberField(TEXT("visibleActors"), Rows.Num());
    Result->SetArrayField(TEXT("actors"), Listed);
    Result->SetNumberField(TEXT("background"), Percent(Background));
    Result->SetNumberField(TEXT("other"), Percent(Other));
    Result->SetObjectField(TEXT("brightness"), Brightness);
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    const FString Message = Rows.Num() == 0
        ? FString::Printf(TEXT("No actor in view; %.1f%% of the frame is background."), Percent(Background))
        : FString::Printf(TEXT("%d actor(s) in view; the largest is %s at %.1f%% of the frame."), Rows.Num(),
                          *Rows[0].Actor->GetActorLabel(), Percent(Rows[0].Pixels));
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true, Message, Result);
    return true;
}
} // namespace McpEnvironmentHandlers
