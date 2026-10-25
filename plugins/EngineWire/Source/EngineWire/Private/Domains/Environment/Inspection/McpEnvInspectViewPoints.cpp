#include "Domains/Environment/Inspection/McpEnvInspectViewIds.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "Domains/Environment/McpEnvShared.h"

#include "CollisionQueryParams.h"
#include "EditorViewportClient.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "Engine/HitResult.h" // 5.0 declares FHitResult in EngineTypes.h
#endif

// describe_view's points (what is under a spot of the frame, drawn and in the world) and labels (the frame with
// each listed actor boxed and numbered). Split from McpEnvInspectViewContents.cpp.
namespace McpEnvironmentHandlers {
namespace {
// Far enough for anything a level holds; a line that meets nothing reports no hit.
constexpr double McpViewTraceLength = 2000000.0;
// The labelled frame is at most this wide (capture_passes' default), keeping its PNG far under the reply cap.
constexpr int32 McpLabelWidth = 960;
// 3 x 5 pixel digits: five rows of three bits from the top, a row's leftmost pixel in its highest bit.
constexpr uint16 McpDigits[10] = {0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF};
const FColor McpLabelColours[] = {FColor(255, 70, 70), FColor(70, 200, 255), FColor(255, 210, 0), FColor(120, 255, 120),
                                  FColor(255, 120, 255), FColor(255, 150, 60), FColor(170, 150, 255), FColor(255, 255, 255)};
} // namespace

bool McpDescribeViewPoints(const FEditorViewportClient &Client, const FIntPoint &Size, const TArray<FColor> &Ids,
                           TMap<uint32, FMcpViewProxy> &Proxies, const TArray<TSharedPtr<FJsonValue>> &Points,
                           int32 Tolerance, TArray<TSharedPtr<FJsonValue>> &OutRows, FString &OutError)
{
    if (Client.IsOrtho())
    {
        OutError = TEXT("points need a perspective level viewport; this one is orthographic (top, side or front).");
        return false;
    }
    if (Points.Num() == 0 || Points.Num() > 64)
    {
        OutError = TEXT("points takes 1-64 entries of {x, y}.");
        return false;
    }
    // The inverse of McpDescribeUnseenActor's projection: the level editor keeps the horizontal FOV (MaintainXFOV).
    const FMatrix Axes = FRotationMatrix(Client.GetViewRotation());
    const double TanX = FMath::Tan(FMath::DegreesToRadians(Client.ViewFOV * 0.5));
    const double TanY = TanX * Size.Y / Size.X;
    const FVector Camera = Client.GetViewLocation();
    UWorld *World = Client.GetWorld();
    const FCollisionQueryParams Params(FName(TEXT("McpViewPoint")), true);
    // A pointer into Proxies lasts only until the next lookup adds to it.
    const auto ProxyAt = [&Ids, &Size, &Proxies](int32 PixelX, int32 PixelY) -> const FMcpViewProxy *
    {
        const FColor &Id = Ids[PixelY * Size.X + PixelX];
        const FMcpViewProxy *Found = Proxies.Find(McpViewIdKey(Id));
        return Found ? Found : &Proxies.Add(McpViewIdKey(Id), McpViewProxyOf(Id));
    };
    for (const TSharedPtr<FJsonValue> &Value : Points)
    {
        const TSharedPtr<FJsonObject> *Point = nullptr;
        double X = -1.0;
        double Y = -1.0;
        if (!Value.IsValid() || !Value->TryGetObject(Point) || !Point || !(*Point)->TryGetNumberField(TEXT("x"), X) ||
            !(*Point)->TryGetNumberField(TEXT("y"), Y) || X < 0.0 || X > 1.0 || Y < 0.0 || Y > 1.0)
        {
            OutError = TEXT("Each point is {x, y}, both from 0 to 1: x from the left of the frame, y from its top.");
            return false;
        }
        const TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("x"), X);
        Row->SetNumberField(TEXT("y"), Y);
        // What the eye sees there: the id drawn at that pixel. With tolerance, a spot just off an actor (beside a thin
        // pole, past an edge) moves to the nearest pixel within that many where one is drawn.
        int32 PixelX = FMath::Min(int32(X * Size.X), Size.X - 1);
        int32 PixelY = FMath::Min(int32(Y * Size.Y), Size.Y - 1);
        int32 Nearest = ProxyAt(PixelX, PixelY)->Kind == EMcpViewKind::Actor ? 0 : MAX_int32;
        const int32 FromX = PixelX, FromY = PixelY;
        for (int32 DY = -Tolerance; DY <= Tolerance && Nearest > 0; ++DY)
        {
            for (int32 DX = -Tolerance; DX <= Tolerance; ++DX)
            {
                const int32 CandidateX = FromX + DX, CandidateY = FromY + DY;
                if (DX * DX + DY * DY < FMath::Min(Nearest, Tolerance * Tolerance + 1) && CandidateX >= 0 && CandidateY >= 0 &&
                    CandidateX < Size.X && CandidateY < Size.Y && ProxyAt(CandidateX, CandidateY)->Kind == EMcpViewKind::Actor)
                {
                    Nearest = DX * DX + DY * DY;
                    PixelX = CandidateX;
                    PixelY = CandidateY;
                }
            }
        }
        if (Nearest > 0 && Nearest != MAX_int32)
        {
            X = (PixelX + 0.5) / Size.X;
            Y = (PixelY + 0.5) / Size.Y;
            const TSharedPtr<FJsonObject> Snapped = MakeShared<FJsonObject>();
            Snapped->SetNumberField(TEXT("x"), McpViewRound(X, 10000.0));
            Snapped->SetNumberField(TEXT("y"), McpViewRound(Y, 10000.0));
            Row->SetObjectField(TEXT("snappedTo"), Snapped);
        }
        const FMcpViewProxy *Proxy = ProxyAt(PixelX, PixelY);
        Row->SetStringField(TEXT("drawn"), Proxy->Kind == EMcpViewKind::Actor ? TEXT("actor")
            : Proxy->Kind == EMcpViewKind::Other ? TEXT("editor") : TEXT("background"));
        if (Proxy->Kind == EMcpViewKind::Actor)
        {
            Row->SetStringField(TEXT("actorName"), Proxy->Actor->GetActorLabel());
        }
        // Where that is in the world: the line from the camera through it, against Visibility collision.
        const FVector Direction = (Axes.GetScaledAxis(EAxis::X) + Axes.GetScaledAxis(EAxis::Y) * ((2.0 * X - 1.0) * TanX) +
                                   Axes.GetScaledAxis(EAxis::Z) * ((1.0 - 2.0 * Y) * TanY)).GetSafeNormal();
        FHitResult Hit;
        if (World && World->LineTraceSingleByChannel(Hit, Camera, Camera + Direction * McpViewTraceLength, ECC_Visibility, Params))
        {
            const TSharedPtr<FJsonObject> Trace = MakeShared<FJsonObject>();
            Trace->SetObjectField(TEXT("location"), McpHandlerUtils::VectorToJson(Hit.ImpactPoint));
            Trace->SetObjectField(TEXT("normal"), McpHandlerUtils::VectorToJson(Hit.ImpactNormal));
            Trace->SetNumberField(TEXT("distance"), FMath::RoundToDouble(Hit.Distance));
            if (const AActor *Actor = Hit.GetActor())
            {
                Trace->SetStringField(TEXT("actorName"), Actor->GetActorLabel());
            }
            if (const UPrimitiveComponent *Component = Hit.GetComponent())
            {
                Trace->SetStringField(TEXT("componentName"), Component->GetName());
            }
            Row->SetObjectField(TEXT("hit"), Trace);
        }
        OutRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    return true;
}

FString McpLabelledViewPng(const TArray<FColor> &Colors, const FIntPoint &Size, const TArray<FIntRect> &Boxes)
{
    const int32 Width = FMath::Min(Size.X, McpLabelWidth);
    const int32 Height = FMath::Max(1, FMath::RoundToInt(double(Size.Y) * Width / Size.X));
    const double Scale = double(Size.X) / Width;
    TArray<FColor> Image;
    Image.SetNumUninitialized(Width * Height);
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            FColor Pixel = Colors[FMath::Min(int32(Y * Scale), Size.Y - 1) * Size.X + FMath::Min(int32(X * Scale), Size.X - 1)];
            Pixel.A = 255;
            Image[Y * Width + X] = Pixel;
        }
    }
    const auto Fill = [&Image, Width, Height](int32 X0, int32 Y0, int32 X1, int32 Y1, const FColor &Colour)
    {
        for (int32 Y = FMath::Max(Y0, 0); Y < FMath::Min(Y1, Height); ++Y)
        {
            for (int32 X = FMath::Max(X0, 0); X < FMath::Min(X1, Width); ++X)
            {
                Image[Y * Width + X] = Colour;
            }
        }
    };
    constexpr int32 Dot = 3; // one font pixel, drawn as Dot x Dot
    for (int32 Index = 0; Index < Boxes.Num(); ++Index)
    {
        const FColor Colour = McpLabelColours[Index % UE_ARRAY_COUNT(McpLabelColours)];
        const int32 Left = int32(Boxes[Index].Min.X / Scale);
        const int32 Top = int32(Boxes[Index].Min.Y / Scale);
        const int32 Right = FMath::Max(int32(Boxes[Index].Max.X / Scale), Left + 4);
        const int32 Bottom = FMath::Max(int32(Boxes[Index].Max.Y / Scale), Top + 4);
        Fill(Left, Top, Right, Top + 2, Colour);
        Fill(Left, Bottom - 2, Right, Bottom, Colour);
        Fill(Left, Top, Left + 2, Bottom, Colour);
        Fill(Right - 2, Top, Right, Bottom, Colour);
        // The number in a tag at the box's top-left corner, kept inside the frame.
        const FString Number = FString::FromInt(Index + 1);
        const int32 TagWidth = Number.Len() * 4 * Dot + Dot;
        const int32 TagHeight = 7 * Dot;
        const int32 TagX = FMath::Clamp(Left, 0, FMath::Max(Width - TagWidth, 0));
        const int32 TagY = FMath::Clamp(Top, 0, FMath::Max(Height - TagHeight, 0));
        Fill(TagX, TagY, TagX + TagWidth, TagY + TagHeight, Colour);
        for (int32 Char = 0; Char < Number.Len(); ++Char)
        {
            const uint16 Glyph = McpDigits[Number[Char] - TEXT('0')];
            for (int32 Bit = 0; Bit < 15; ++Bit)
            {
                if (Glyph & (1 << (14 - Bit)))
                {
                    const int32 X = TagX + Dot + Char * 4 * Dot + (Bit % 3) * Dot;
                    const int32 Y = TagY + Dot + (Bit / 3) * Dot;
                    Fill(X, Y, X + Dot, Y + Dot, FColor::Black);
                }
            }
        }
    }
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Width, Height, TArrayView64<const FColor>(Image.GetData(), Image.Num()), Png);
    return FBase64::Encode(Png.GetData(), static_cast<uint32>(Png.Num()));
}

bool McpCaptureView(const TSharedPtr<FJsonObject> &Payload, const TCHAR *What, bool bGameView, FMcpViewCapture &Out,
                    FString &OutError, FString &OutCode)
{
    const auto Fail = [&OutError, &OutCode](const FString &Error, const TCHAR *Code) {
        OutError = Error;
        OutCode = Code;
        return false;
    };
    if (GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor)
    {
        return Fail(FString::Printf(TEXT("%s reads the level viewport, which does not show the running game; stop Play In "
                                         "Editor first (Simulate is fine)."), What), TEXT("PIE_RUNNING"));
    }
    // A level viewport under another major tab is not painted and reads back black.
    if (BringLevelEditorTabToFrontForMcp())
    {
        return Fail(TEXT("The level viewport was under another tab and has been brought forward; call again to read it."),
                    TEXT("VIEWPORT_NOT_READY"));
    }
    Out.Client = GetActiveEditorViewportClientForMcp();
    FViewport *Viewport = Out.Client ? Out.Client->Viewport : nullptr;
    Out.Size = Viewport ? Viewport->GetSizeXY() : FIntPoint::ZeroValue;
    if (Out.Size.X <= 0 || Out.Size.Y <= 0)
    {
        return Fail(TEXT("No level viewport with a size is open."), TEXT("VIEWPORT_NOT_AVAILABLE"));
    }
    // The first frames after a camera move still carry the old view's occlusion; a few frames later they match.
    const bool bMoved = Payload->HasField(TEXT("location")) || Payload->HasField(TEXT("rotation"));
    const bool bSwitch = bGameView && !Out.Client->IsInGameView();
    Out.Client->SetViewLocation(ExtractVectorField(Payload, TEXT("location"), Out.Client->GetViewLocation()));
    Out.Client->SetViewRotation(ExtractRotatorField(Payload, TEXT("rotation"), Out.Client->GetViewRotation()));
    if (bSwitch)
    {
        Out.Client->SetGameView(true);
    }
    Out.Client->Invalidate();
    DrawViewportFramesForMcp(Viewport, Out.Client->GetWorld(), bMoved || bSwitch ? 3 : 1);
    Viewport->ReadPixels(Out.Colors, FReadSurfaceDataFlags(RCM_UNorm));
    Viewport->InvalidateHitProxy();
    Out.Ids = Viewport->GetRawHitProxyData(FIntRect(0, 0, Out.Size.X, Out.Size.Y));
    if (bSwitch)
    {
        Out.Client->SetGameView(false);
        Out.Client->Invalidate();
    }
    const int32 PixelCount = Out.Size.X * Out.Size.Y;
    if (Out.Colors.Num() != PixelCount || Out.Ids.Num() != PixelCount)
    {
        return Fail(TEXT("The viewport's pixels or ids could not be read."), TEXT("CAPTURE_FAILED"));
    }
    return true;
}
} // namespace McpEnvironmentHandlers
