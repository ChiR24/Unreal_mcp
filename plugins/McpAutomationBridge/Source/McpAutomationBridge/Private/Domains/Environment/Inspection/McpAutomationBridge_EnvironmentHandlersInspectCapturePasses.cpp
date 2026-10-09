#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"
#include "Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectViewIds.h"
#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorScreenshotSupport.h"

#include "Components/SceneCaptureComponent2D.h"
#include "EditorViewportClient.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "TextureResource.h"

// capture_passes: the level viewport's view as machine-readable passes, aligned pixel for pixel: an id image (one
// colour per actor, with the legend), the depth in cm (EXR) and the world normal. A model, a probe or a script can
// attribute a region of a frame to the actor drawn there, measure distances and read surface orientation.
namespace McpEnvironmentHandlers {
namespace {
// Scene depth past this (cm) is the sky or nothing.
constexpr float McpFarDepth = 1.0e7f;
constexpr int32 McpMaxLegend = 256;

// Renders World from the view into a float target of Size and returns its pixels; Source picks depth or normal.
TArray<FLinearColor> McpCapturePass(UWorld *World, const FVector &Location, const FRotator &Rotation, float Fov,
                                    const FIntPoint &Size, ESceneCaptureSource Source)
{
    UTextureRenderTarget2D *Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
    Target->ClearColor = FLinearColor::Black;
    Target->InitCustomFormat(Size.X, Size.Y, PF_A32B32G32R32F, true);
    Target->UpdateResourceImmediate(true);
    USceneCaptureComponent2D *Capture = NewObject<USceneCaptureComponent2D>(GetTransientPackage());
    Capture->bCaptureEveryFrame = false;
    Capture->bCaptureOnMovement = false;
    Capture->CaptureSource = Source;
    Capture->FOVAngle = Fov;
    Capture->TextureTarget = Target;
    Capture->SetWorldLocationAndRotation(Location, Rotation);
    Capture->RegisterComponentWithWorld(World);
    Capture->CaptureScene();
    TArray<FLinearColor> Pixels;
    Target->GameThread_GetRenderTargetResource()->ReadLinearColorPixels(Pixels);
    Capture->DestroyComponent();
    return Pixels;
}

bool McpWriteFile(const FString &Path, const TArray64<uint8> &Bytes)
{
    return Bytes.Num() > 0 && FFileHelper::SaveArrayToFile(Bytes, *Path);
}

double McpRound(double Value, double Scale)
{
    return FMath::RoundToDouble(Value * Scale) / Scale;
}

FString McpHex(const FColor &Color)
{
    return FString::Printf(TEXT("#%02x%02x%02x"), Color.R, Color.G, Color.B);
}
} // namespace

bool HandleInspectCapturePassesAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    auto Fail = [&](const FString &Message, const TCHAR *Code) {
        Bridge.SendAutomationError(RequestingSocket, RequestId, Message, Code);
        return true;
    };
    if (GEditor->PlayWorld && !GEditor->bIsSimulatingInEditor)
    {
        return Fail(TEXT("capture_passes reads the level viewport, which does not show the running game; stop Play In Editor first (Simulate is fine)."), TEXT("PIE_RUNNING"));
    }
    TSet<FString> Passes;
    const TArray<TSharedPtr<FJsonValue>> *Asked = nullptr;
    if (Payload->TryGetArrayField(TEXT("passes"), Asked) && Asked)
    {
        for (const TSharedPtr<FJsonValue> &Value : *Asked)
        {
            const FString Pass = Value.IsValid() ? Value->AsString().ToLower() : FString();
            if (Pass != TEXT("id") && Pass != TEXT("depth") && Pass != TEXT("normal"))
            {
                return Fail(FString::Printf(TEXT("pass '%s' is not id, depth or normal."), *Pass), TEXT("INVALID_ARGUMENT"));
            }
            Passes.Add(Pass);
        }
    }
    if (Passes.Num() == 0)
    {
        Passes = {TEXT("id"), TEXT("depth"), TEXT("normal")};
    }
    if (BringLevelEditorTabToFrontForMcp())
    {
        return Fail(TEXT("The level viewport was under another tab and has been brought forward; call again to capture it."), TEXT("VIEWPORT_NOT_READY"));
    }
    FEditorViewportClient *Client = GetActiveEditorViewportClientForMcp();
    FViewport *Viewport = Client ? Client->Viewport : nullptr;
    const FIntPoint ViewSize = Viewport ? Viewport->GetSizeXY() : FIntPoint::ZeroValue;
    if (ViewSize.X <= 0 || ViewSize.Y <= 0 || !Client->GetWorld())
    {
        return Fail(TEXT("No level viewport with a size is open."), TEXT("VIEWPORT_NOT_AVAILABLE"));
    }
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    if (Name.IsEmpty())
    {
        Name = FDateTime::Now().ToString(TEXT("Passes_%Y%m%d_%H%M%S"));
    }
    const FString Folder = GetJsonStringField(Payload, TEXT("outputPath"), TEXT("Saved/Captures"));
    FString Probe;
    FString PathError;
    if (!McpResolveProjectFilePath(Folder / (Name + TEXT("_id.png")), Probe, PathError))
    {
        return Fail(FString::Printf(TEXT("outputPath '%s' and name '%s' must stay inside the project."), *Folder, *Name), TEXT("SECURITY_VIOLATION"));
    }
    const FString Base = Probe.LeftChop(7);

    const bool bMoved = Payload->HasField(TEXT("location")) || Payload->HasField(TEXT("rotation"));
    Client->SetViewLocation(ExtractVectorField(Payload, TEXT("location"), Client->GetViewLocation()));
    Client->SetViewRotation(ExtractRotatorField(Payload, TEXT("rotation"), Client->GetViewRotation()));
    Client->Invalidate();
    DrawViewportFramesForMcp(Viewport, Client->GetWorld(), bMoved ? 3 : 1);
    const int32 Width = FMath::Clamp(GetJsonIntField(Payload, TEXT("width"), 960), 64, 4096);
    const FIntPoint Size(Width, FMath::Max(1, FMath::RoundToInt(double(Width) * ViewSize.Y / ViewSize.X)));
    const FVector Camera = Client->GetViewLocation();
    const FRotator Facing = Client->GetViewRotation();
    const int32 Count = Size.X * Size.Y;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Base), true);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetObjectField(TEXT("cameraLocation"), McpHandlerUtils::VectorToJson(Camera));
    Result->SetObjectField(TEXT("cameraRotation"), McpHandlerUtils::RotatorToJson(Facing));
    Result->SetNumberField(TEXT("fov"), Client->ViewFOV);
    Result->SetNumberField(TEXT("width"), Size.X);
    Result->SetNumberField(TEXT("height"), Size.Y);
    const TSharedPtr<FJsonObject> Files = MakeShared<FJsonObject>();

    // The depth tells sky from geometry for the normal summary too.
    TArray<float> Depth;
    if (Passes.Contains(TEXT("depth")) || Passes.Contains(TEXT("normal")))
    {
        for (const FLinearColor &Pixel : McpCapturePass(Client->GetWorld(), Camera, Facing, Client->ViewFOV, Size, SCS_SceneDepth))
        {
            Depth.Add(Pixel.R);
        }
    }
    if (Passes.Contains(TEXT("depth")) && Depth.Num() == Count)
    {
        TSharedPtr<IImageWrapper> Exr = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper")).CreateImageWrapper(EImageFormat::EXR);
        const bool bSet = Exr.IsValid() && Exr->SetRaw(Depth.GetData(), Depth.Num() * sizeof(float), Size.X, Size.Y, ERGBFormat::GrayF, 32);
        if (bSet && McpWriteFile(Base + TEXT("_depth.exr"), Exr->GetCompressed()))
        {
            Files->SetStringField(TEXT("depth"), Base + TEXT("_depth.exr"));
        }
        TArray<float> Near = Depth.FilterByPredicate([](float Value) { return Value < McpFarDepth; });
        Near.Sort();
        const TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
        if (Near.Num() > 0)
        {
            Summary->SetNumberField(TEXT("min"), FMath::RoundToDouble(Near[0]));
            Summary->SetNumberField(TEXT("median"), FMath::RoundToDouble(Near[Near.Num() / 2]));
            Summary->SetNumberField(TEXT("max"), FMath::RoundToDouble(Near.Last()));
        }
        Summary->SetNumberField(TEXT("sky"), McpRound(100.0 * (Count - Near.Num()) / Count, 100.0));
        Result->SetObjectField(TEXT("depth"), Summary);
    }
    if (Passes.Contains(TEXT("normal")))
    {
        const TArray<FLinearColor> Normals = McpCapturePass(Client->GetWorld(), Camera, Facing, Client->ViewFOV, Size, SCS_Normal);
        if (Normals.Num() == Count)
        {
            TArray<FColor> Image;
            Image.SetNumUninitialized(Count);
            int64 Up = 0, Down = 0, Drawn = 0;
            for (int32 Index = 0; Index < Count; ++Index)
            {
                const FVector3f Normal(Normals[Index].R, Normals[Index].G, Normals[Index].B);
                Image[Index] = FLinearColor(Normal.X * 0.5f + 0.5f, Normal.Y * 0.5f + 0.5f, Normal.Z * 0.5f + 0.5f).ToFColor(false);
                if (Depth.Num() == Count ? Depth[Index] < McpFarDepth : Normal.SizeSquared() > 0.25f)
                {
                    ++Drawn;
                    Up += Normal.Z > 0.7f ? 1 : 0;
                    Down += Normal.Z < -0.7f ? 1 : 0;
                }
            }
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Image.GetData(), Image.Num()), Png);
            if (McpWriteFile(Base + TEXT("_normal.png"), Png))
            {
                Files->SetStringField(TEXT("normal"), Base + TEXT("_normal.png"));
            }
            const TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
            Summary->SetNumberField(TEXT("up"), Drawn > 0 ? McpRound(100.0 * Up / Drawn, 100.0) : 0.0);
            Summary->SetNumberField(TEXT("down"), Drawn > 0 ? McpRound(100.0 * Down / Drawn, 100.0) : 0.0);
            Summary->SetNumberField(TEXT("side"), Drawn > 0 ? McpRound(100.0 * (Drawn - Up - Down) / Drawn, 100.0) : 0.0);
            Result->SetObjectField(TEXT("normal"), Summary);
        }
    }
    if (Passes.Contains(TEXT("id")))
    {
        Viewport->InvalidateHitProxy();
        const TArray<FColor> &Ids = Viewport->GetRawHitProxyData(FIntRect(0, 0, ViewSize.X, ViewSize.Y));
        if (Ids.Num() == ViewSize.X * ViewSize.Y)
        {
            // Each pixel of the output takes the id of the nearest viewport pixel.
            TArray<FMcpViewProxy> Drawn;
            Drawn.SetNum(Count);
            TMap<uint32, FMcpViewProxy> Known;
            TMap<AActor *, int64> Pixels;
            for (int32 Y = 0; Y < Size.Y; ++Y)
            {
                for (int32 X = 0; X < Size.X; ++X)
                {
                    const FColor &Id = Ids[FMath::Min(Y * ViewSize.Y / Size.Y, ViewSize.Y - 1) * ViewSize.X + FMath::Min(X * ViewSize.X / Size.X, ViewSize.X - 1)];
                    const FMcpViewProxy *Proxy = Known.Find(McpViewIdKey(Id));
                    if (!Proxy)
                    {
                        Proxy = &Known.Add(McpViewIdKey(Id), McpViewProxyOf(Id));
                    }
                    Drawn[Y * Size.X + X] = *Proxy;
                    if (Proxy->Kind == EMcpViewKind::Actor)
                    {
                        ++Pixels.FindOrAdd(Proxy->Actor);
                    }
                }
            }
            Pixels.ValueSort([](int64 A, int64 B) { return A > B; });
            // Hues a golden angle apart, largest actor first, so neighbours in the list never share a colour.
            TMap<AActor *, FColor> Colors;
            TArray<TSharedPtr<FJsonValue>> Legend;
            for (const TPair<AActor *, int64> &Entry : Pixels)
            {
                const FColor Color = FLinearColor(FMath::Fmod(Colors.Num() * 137.508f, 360.0f), 0.8f, 1.0f).HSVToLinearRGB().ToFColor(true);
                Colors.Add(Entry.Key, Color);
                if (Legend.Num() < McpMaxLegend)
                {
                    const TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
                    Item->SetStringField(TEXT("color"), McpHex(Color));
                    Item->SetStringField(TEXT("name"), Entry.Key->GetActorLabel());
                    Item->SetStringField(TEXT("class"), Entry.Key->GetClass()->GetName());
                    Item->SetNumberField(TEXT("coverage"), McpRound(100.0 * Entry.Value / Count, 100.0));
                    Legend.Add(MakeShared<FJsonValueObject>(Item));
                }
            }
            TArray<FColor> Image;
            Image.SetNumUninitialized(Count);
            for (int32 Index = 0; Index < Count; ++Index)
            {
                Image[Index] = Drawn[Index].Kind == EMcpViewKind::Actor ? Colors[Drawn[Index].Actor]
                             : Drawn[Index].Kind == EMcpViewKind::Other ? FColor(64, 64, 64) : FColor::Black;
            }
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Image.GetData(), Image.Num()), Png);
            if (McpWriteFile(Base + TEXT("_id.png"), Png))
            {
                Files->SetStringField(TEXT("id"), Base + TEXT("_id.png"));
            }
            Result->SetArrayField(TEXT("ids"), Legend);
        }
    }
    Result->SetObjectField(TEXT("files"), Files);
    McpHandlerUtils::MarkNoAssetsChanged(Result);
    if (Files->Values.Num() < Passes.Num())
    {
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
            FString::Printf(TEXT("Only %d of %d passes were written."), Files->Values.Num(), Passes.Num()), Result, TEXT("CAPTURE_FAILED"));
        return true;
    }
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("Captured %d pass(es) at %dx%d."), Files->Values.Num(), Size.X, Size.Y), Result);
    return true;
}
} // namespace McpEnvironmentHandlers
