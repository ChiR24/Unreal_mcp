#include "Domains/ControlEditor/Viewport/McpPathTracedShot.h"

#include "Containers/Ticker.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "Foundation/Render/McpConsoleVariableWrite.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "ShaderCompiler.h"

// screenshot pathTraced: a review image the path tracer renders to the samples asked and denoises, without the caller
// switching view modes and console variables and guessing when the image is done. The level viewport is drawn in Path
// Tracing view until its view state counts the samples (the renderer denoises on the frame that renders the last one),
// read back as a screenshot is, and everything is put back before the reply. The editor window is held on screen and
// unthrottled meanwhile: a minimized editor renders nothing and a background one about 3 frames a second.
namespace McpPathTracedShot
{
namespace
{
constexpr int32 DefaultSamples = 64;
// Under the client's 30 s limit with the capture after it; a shot short of its samples is taken with what it has.
constexpr double DeadlineSeconds = 22.0;
// No sample counted by then, with no shader compiling: the path tracer is not drawing this viewport.
constexpr double StartSeconds = 5.0;

// A console variable set for the shot and put back as it was found.
struct FHeldVariable
{
    IConsoleVariable* Variable = nullptr;
    FString Old;
    void Set(const TCHAR* Name, int32 Value)
    {
        Variable = IConsoleManager::Get().FindConsoleVariable(Name);
        Old = Variable ? Variable->GetString() : FString();
        if (Variable) { McpSetConsoleVariable(*Variable, FString::FromInt(Value)); }
    }
    void Restore() const { if (Variable) { McpSetConsoleVariable(*Variable, Old); } }
};

struct FShot
{
    TWeakObjectPtr<UEngineWireSubsystem> Self;
    FString RequestId;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> Socket;
    FString FullPath;
    FEditorViewportClient* Client = nullptr;
    EViewModeIndex OldMode = VMI_Lit;
    bool bWasGameView = false;
    FHeldVariable Samples;
    FHeldVariable Denoiser;
    FMcpEditorRunHold Hold;
    uint32 Wanted = DefaultSamples;
    bool bDenoise = true;
    double Started = 0.0;
};

const FText& OverrideText() { static const FText Text = NSLOCTEXT("EngineWire", "McpPathTracedShot", "EngineWire path-traced screenshot"); return Text; }

// The viewport client can go while the shot runs (a layout change): only one the editor still lists is touched.
bool ClientAlive(const FShot& Shot) { return GEditor && GEditor->GetAllViewportClients().Contains(Shot.Client); }

uint32 SamplesDone(const FShot& Shot)
{
#if RHI_RAYTRACING
    const FSceneViewStateInterface* State = Shot.Client->ViewState.GetReference();
    return State ? State->GetPathTracingSampleIndex() : 0;
#else
    return 0;
#endif
}

int32 ShaderJobsLeft() { return GShaderCompilingManager ? GShaderCompilingManager->GetNumRemainingJobs() : 0; }

bool RayTracingHere()
{
#if RHI_RAYTRACING
    const IConsoleVariable* RayTracing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
    const IConsoleVariable* PathTracing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PathTracing"));
    return GRHISupportsRayTracing && RayTracing && RayTracing->GetInt() != 0 && PathTracing && PathTracing->GetInt() != 0;
#else
    return false;
#endif
}

// Everything the shot changed goes back before it answers, taken or not.
void Restore(FShot& Shot)
{
    Shot.Samples.Restore();
    Shot.Denoiser.Restore();
    if (ClientAlive(Shot))
    {
        // View mode before game view: each applies the view mode to the show flags it puts in place.
        Shot.Client->SetViewMode(Shot.OldMode);
        Shot.Client->SetGameView(Shot.bWasGameView);
        Shot.Client->RemoveRealtimeOverride(OverrideText(), false);
        Shot.Client->Invalidate();
    }
    EndEditorRunForMcp(Shot.Hold);
}

void Reply(FShot& Shot, const TFunctionRef<void(UEngineWireSubsystem&)> Send)
{
    if (UEngineWireSubsystem* Self = Shot.Self.Get())
    {
        Self->RunLatentReply(Shot.RequestId, TEXT("control_editor"), [&]() { Send(*Self); });
    }
}

void Fail(FShot& Shot, const TCHAR* Code, const FString& Message)
{
    Restore(Shot);
    Reply(Shot, [&](UEngineWireSubsystem& Self) {
        Self.SendAutomationResponse(Shot.Socket, Shot.RequestId, false, Message, nullptr, Code);
    });
}

void Capture(FShot& Shot, uint32 Done)
{
    FViewport* Viewport = Shot.Client->Viewport;
    DrawViewportFramesForMcp(Viewport, Shot.Client->GetWorld(), 1);
    TArray<FColor> Bitmap;
    FIntPoint FrameSize = Viewport->GetSizeXY();
    FString Code, Error;
    const bool bRead = ReadViewFrameForMcp(Viewport, Shot.Payload, Bitmap, FrameSize, Code, Error);
    const FVector Location = Shot.Client->GetViewLocation();
    const FRotator Rotation = Shot.Client->GetViewRotation();
    Restore(Shot);
    if (!bRead)
    {
        Reply(Shot, [&](UEngineWireSubsystem& Self) { Self.SendAutomationResponse(Shot.Socket, Shot.RequestId, false, Error, nullptr, Code); });
        return;
    }
    FIntPoint OutputSize = FrameSize;
    FString ResolutionError;
    if (ResolveScreenshotResolutionForMcp(Shot.Payload, FrameSize, OutputSize, ResolutionError) && OutputSize != FrameSize)
    {
        TArray<FColor> Resampled;
        ResampleBitmapForMcp(Bitmap, FrameSize, Resampled, OutputSize);
        Bitmap = MoveTemp(Resampled);
    }
    else
    {
        OutputSize = FrameSize;
    }
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(OutputSize.X, OutputSize.Y, TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()), Png);

    const bool bAll = Done >= Shot.Wanted;
    TSharedPtr<FJsonObject> Traced = McpHandlerUtils::CreateResultObject();
    Traced->SetNumberField(TEXT("samples"), Done);
    Traced->SetNumberField(TEXT("samplesAsked"), Shot.Wanted);
    // The renderer denoises on the frame that renders the last sample, so a shot cut short is not denoised.
    Traced->SetBoolField(TEXT("denoised"), Shot.bDenoise && bAll);
    Traced->SetNumberField(TEXT("seconds"), FMath::RoundToInt((FPlatformTime::Seconds() - Shot.Started) * 10.0) / 10.0);
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("filename"), FPaths::GetCleanFilename(Shot.FullPath));
    Resp->SetStringField(TEXT("view"), TEXT("editor_viewport"));
    Resp->SetNumberField(TEXT("width"), OutputSize.X);
    Resp->SetNumberField(TEXT("height"), OutputSize.Y);
    if (OutputSize != FrameSize)
    {
        Resp->SetNumberField(TEXT("viewportWidth"), FrameSize.X);
        Resp->SetNumberField(TEXT("viewportHeight"), FrameSize.Y);
    }
    Resp->SetObjectField(TEXT("cameraLocation"), McpHandlerUtils::VectorToJson(Location));
    Resp->SetObjectField(TEXT("cameraRotation"), McpHandlerUtils::RotatorToJson(Rotation));
    Resp->SetObjectField(TEXT("pathTraced"), Traced);
    if (!bAll)
    {
        TArray<TSharedPtr<FJsonValue>> Warnings;
        Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
            TEXT("The path tracer reached %u of %u samples in %.0f s, so the picture is noisier than asked and not denoised "
                 "(the denoiser runs on the last sample): ask fewer samples, or a smaller viewport."),
            Done, Shot.Wanted, DeadlineSeconds)));
        Resp->SetArrayField(TEXT("warnings"), Warnings);
    }
    Reply(Shot, [&](UEngineWireSubsystem& Self) {
        SendScreenshotReceiptForMcp(&Self, Shot.Socket, Shot.RequestId, Shot.Payload, Resp, Png.GetData(), Png.Num(),
                                    Shot.FullPath, TEXT("Path-traced screenshot"));
    });
}

bool Tick(const TSharedRef<FShot>& Shot)
{
    if (!Shot->Self.IsValid())
    {
        Restore(*Shot);
        return false;
    }
    if (!ClientAlive(*Shot) || GEditor->PlayWorld)
    {
        Fail(*Shot, TEXT("VIEWPORT_CHANGED"), TEXT("The level viewport closed or Play started while the path tracer was drawing; nothing was captured."));
        return false;
    }
    const uint32 Done = SamplesDone(*Shot);
    const double Elapsed = FPlatformTime::Seconds() - Shot->Started;
    const int32 Jobs = ShaderJobsLeft();
    if (Done == 0 && (Elapsed > DeadlineSeconds || (Elapsed > StartSeconds && Jobs == 0)))
    {
        Fail(*Shot, Jobs > 0 ? TEXT("SHADERS_COMPILING") : TEXT("PATH_TRACER_NOT_RUNNING"),
             Jobs > 0 ? FString::Printf(TEXT("The path tracer's shaders are still compiling (%d jobs left), so no sample was drawn in %.0f s; call again once they finish."), Jobs, DeadlineSeconds)
                      : FString(TEXT("The level viewport drew no path-traced sample: the path tracer is not running here (check the project's Support Path Tracing setting and the viewport's scalability).")));
        return false;
    }
    if (Done >= Shot->Wanted || (Done > 0 && Elapsed > DeadlineSeconds))
    {
        Capture(*Shot, Done);
        return false;
    }
    return true;
}
} // namespace

bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    bool bPathTraced = false;
    if (!Payload.IsValid() || !Payload->TryGetBoolField(TEXT("pathTraced"), bPathTraced) || !bPathTraced)
    {
        return false;
    }
    const auto Refuse = [&](const TCHAR* Code, const FString& Message) {
        Self->SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };
    FString Mode;
    Payload->TryGetStringField(TEXT("mode"), Mode);
    if (!Mode.IsEmpty() && !Mode.Equals(TEXT("editor_viewport"), ESearchCase::IgnoreCase))
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), TEXT("pathTraced draws the level viewport: leave mode out or set it to editor_viewport."));
    }
    if (Payload->HasField(TEXT("renderSize")) || Payload->HasField(TEXT("camera")))
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), TEXT("pathTraced reads the level viewport while the path tracer accumulates in it: place the view with location, rotation and fov, not camera, and leave renderSize out."));
    }
    if (GEditor->PlayWorld)
    {
        return Refuse(TEXT("PIE_ACTIVE"), TEXT("pathTraced draws the level viewport, which Play covers: stop Play first."));
    }
    if (!RayTracingHere())
    {
        return Refuse(TEXT("PATH_TRACING_UNAVAILABLE"), TEXT("This editor cannot path trace: it needs a GPU and graphics API with ray tracing (DirectX 12 on Windows) and the project settings Support Hardware Ray Tracing and Support Path Tracing (r.RayTracing, r.PathTracing), which take effect after an editor restart."));
    }
    FEditorViewportClient* Client = GetActiveEditorViewportClientForMcp();
    if (!Client || !Client->Viewport)
    {
        return Refuse(TEXT("NO_VIEWPORT"), TEXT("No level viewport is open to path trace."));
    }
    FString Directory, PathError, ResolutionError;
    FIntPoint Bounded;
    if (!ResolveScreenshotDirectoryForMcp(Payload, FPaths::ProjectSavedDir() / TEXT("Screenshots"), Directory, PathError))
    {
        return Refuse(TEXT("SECURITY_VIOLATION"), PathError);
    }
    if (!ResolveScreenshotResolutionForMcp(Payload, Client->Viewport->GetSizeXY(), Bounded, ResolutionError))
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), ResolutionError);
    }
    double Samples = DefaultSamples;
    Payload->TryGetNumberField(TEXT("samples"), Samples);
    IFileManager::Get().MakeDirectory(*Directory, true);
    BringLevelEditorTabToFrontForMcp();

    const TSharedRef<FShot> Shot = MakeShared<FShot>();
    Shot->Self = Self;
    Shot->RequestId = RequestId;
    Shot->Payload = Payload;
    Shot->Socket = Socket;
    Shot->FullPath = Directory / MakeSafeScreenshotFilenameForMcp(Payload);
    Shot->Client = Client;
    Shot->Wanted = static_cast<uint32>(FMath::Clamp(Samples, 1.0, 4096.0));
    Payload->TryGetBoolField(TEXT("denoise"), Shot->bDenoise);
    PlaceViewCameraForMcp(Client, Payload);
    Shot->Hold = BeginEditorRunForMcp();
    Shot->OldMode = Client->GetViewMode();
    // A review image of the scene, as the game shows it: no editor icons, grid, axis widget or selection on top.
    Shot->bWasGameView = Client->IsInGameView();
    Client->SetGameView(true);
    Shot->Samples.Set(TEXT("r.PathTracing.SamplesPerPixel"), static_cast<int32>(Shot->Wanted));
    Shot->Denoiser.Set(TEXT("r.PathTracing.Denoiser"), Shot->bDenoise ? 1 : 0);
    Client->AddRealtimeOverride(true, OverrideText());
    Client->SetViewMode(VMI_PathTracing);
    Client->Invalidate();
    // Every shot accumulates afresh, with its own settings, and its first frame is drawn here: the view state keeps an
    // earlier shot's count until the path tracer draws again, and read first it passed a 1-sample frame off as done.
    if (FSceneInterface* Scene = Client->GetScene()) { Scene->InvalidatePathTracedOutput(); }
    DrawViewportFramesForMcp(Client->Viewport, Client->GetWorld(), 1);
    Shot->Started = FPlatformTime::Seconds();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Shot](float) { return Tick(Shot); }));
    return true;
}
} // namespace McpPathTracedShot
