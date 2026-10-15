#include "Domains/Environment/Inspection/McpEnvInspectViewIds.h"

#include "Components/PrimitiveComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EditorViewportClient.h"
#include "GameFramework/Actor.h"

// describe_view's answer for an actor the caller named that drew no pixel. Split from McpEnvInspectViewContents.cpp.
namespace McpEnvironmentHandlers {
namespace {
// The box of what the game draws. The sprite the editor gives an actor with only a scene root is editor-only and
// far bigger than a small prop: an 84 cm robot read as a 256 cm cube round its pivot.
FBox McpDrawnBounds(const AActor &Actor)
{
    FBox Box(ForceInit);
    TInlineComponentArray<UPrimitiveComponent *> Parts(&Actor);
    for (const UPrimitiveComponent *Part : Parts)
    {
        if (Part->IsRegistered() && Part->IsVisible() && !Part->bHiddenInGame && !Part->IsEditorOnly())
        {
            Box += Part->Bounds.GetBox();
        }
    }
    return Box.IsValid ? Box : Actor.GetComponentsBoundingBox(true);
}
} // namespace

TSharedPtr<FJsonObject> McpDescribeUnseenActor(AActor *Actor, const FEditorViewportClient &Client, const FIntPoint &Size,
                                               const TArray<FColor> &Ids, TMap<uint32, FMcpViewProxy> &Proxies)
{
    const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), Actor->GetActorLabel());
    const FBox Bounds = McpDrawnBounds(*Actor);
    const FMatrix Axes = FRotationMatrix(Client.GetViewRotation());
    const double TanX = FMath::Tan(FMath::DegreesToRadians(Client.ViewFOV * 0.5));
    const double TanY = TanX * Size.Y / Size.X;
    FBox2D Screen(ForceInit);
    for (int32 Corner = 0; Bounds.IsValid && Corner < 8; ++Corner)
    {
        const FVector Delta = FVector(Corner & 1 ? Bounds.Max.X : Bounds.Min.X, Corner & 2 ? Bounds.Max.Y : Bounds.Min.Y,
                                      Corner & 4 ? Bounds.Max.Z : Bounds.Min.Z) - Client.GetViewLocation();
        const double Forward = FVector::DotProduct(Delta, Axes.GetScaledAxis(EAxis::X));
        if (Forward > 1.0)
        {
            Screen += FVector2D((1.0 + FVector::DotProduct(Delta, Axes.GetScaledAxis(EAxis::Y)) / Forward / TanX) * 0.5 * Size.X,
                                (1.0 - FVector::DotProduct(Delta, Axes.GetScaledAxis(EAxis::Z)) / Forward / TanY) * 0.5 * Size.Y);
        }
    }
    const FBox2D Frame(FVector2D::ZeroVector, FVector2D(Size.X, Size.Y));
    if (!Screen.bIsValid || !Screen.Intersect(Frame))
    {
        Row->SetStringField(TEXT("reason"), TEXT("outside the frame"));
        return Row;
    }
    // What is drawn over the box the bounds project to, by share of the box: the frontmost id per pixel.
    const FBox2D Box = Screen.Overlap(Frame);
    const double InFrame = McpViewRound(100.0 * Box.GetArea() / FMath::Max(Screen.GetArea(), 1.0), 10.0);
    Row->SetNumberField(TEXT("inFrame"), InFrame);
    TMap<AActor *, int64> Covers;
    int64 Total = 0;
    for (int32 Y = FMath::FloorToInt(Box.Min.Y); Y < FMath::Min(FMath::CeilToInt(Box.Max.Y), Size.Y); Y += McpViewStride)
    {
        for (int32 X = FMath::FloorToInt(Box.Min.X); X < FMath::Min(FMath::CeilToInt(Box.Max.X), Size.X); X += McpViewStride)
        {
            const FColor &Id = Ids[Y * Size.X + X];
            const FMcpViewProxy *Proxy = Proxies.Find(McpViewIdKey(Id));
            Proxy = Proxy ? Proxy : &Proxies.Add(McpViewIdKey(Id), McpViewProxyOf(Id));
            ++Total;
            Covers.FindOrAdd(Proxy->Kind == EMcpViewKind::Actor ? Proxy->Actor : nullptr) += 1;
        }
    }
    Covers.Remove(nullptr);
    TArray<TPair<AActor *, int64>> Sorted;
    for (const TPair<AActor *, int64> &Cover : Covers)
    {
        Sorted.Add(Cover);
    }
    Sorted.Sort([](const TPair<AActor *, int64> &A, const TPair<AActor *, int64> &B) { return A.Value > B.Value; });
    // What shows through the empty parts of its box cannot hide it: an actor centred farther from the camera (a
    // storefront behind a balloon cluster), or ground below its middle, a thin slab (a carpet) or what it stands
    // over (a floor block 5 m thick).
    const FVector Middle = Bounds.GetCenter();
    const double Depth = FVector::DotProduct(Middle - Client.GetViewLocation(), Axes.GetScaledAxis(EAxis::X));
    TArray<TSharedPtr<FJsonValue>> HiddenBy;
    for (int32 Index = 0; Index < Sorted.Num() && HiddenBy.Num() < 3; ++Index)
    {
        const FBox Other = McpDrawnBounds(*Sorted[Index].Key);
        const bool bUnder = Middle.X >= Other.Min.X && Middle.X <= Other.Max.X && Middle.Y >= Other.Min.Y && Middle.Y <= Other.Max.Y;
        const bool bGround = Other.Max.Z < Middle.Z && (bUnder || Other.GetSize().Z < 0.25 * Bounds.GetSize().Z);
        if (Other.IsValid && (bGround ||
            FVector::DotProduct(Other.GetCenter() - Client.GetViewLocation(), Axes.GetScaledAxis(EAxis::X)) > Depth))
        {
            continue;
        }
        const TSharedPtr<FJsonObject> Cover = MakeShared<FJsonObject>();
        Cover->SetStringField(TEXT("name"), Sorted[Index].Key->GetActorLabel());
        Cover->SetNumberField(TEXT("share"), McpViewRound(100.0 * Sorted[Index].Value / FMath::Max<int64>(Total, 1), 10.0));
        HiddenBy.Add(MakeShared<FJsonValueObject>(Cover));
    }
    Row->SetStringField(TEXT("reason"), InFrame < 50.0 ? TEXT("at the edge of the frame, mostly outside it")
        : HiddenBy.Num() > 0 ? TEXT("hidden behind what is drawn over its box")
        : TEXT("not drawn, and nothing nearer is drawn over its box (hidden in game, or smaller than the sampling)"));
    Row->SetArrayField(TEXT("hiddenBy"), HiddenBy);
    return Row;
}
} // namespace McpEnvironmentHandlers
