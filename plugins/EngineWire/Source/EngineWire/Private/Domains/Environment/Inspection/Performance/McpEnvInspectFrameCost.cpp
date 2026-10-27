// inspect get_frame_cost: what a frame costs. Names the thread that bounds it and ranks what in view weighs on that
// thread: movable lights that cast shadows, the heaviest non-Nanite meshes, translucent primitives and ticking actors.
// The thread times are the same process-wide timers get_performance_stats reads (McpEnvInspectStats.cpp).

#include "Domains/Environment/McpEnvShared.h"
#include "Foundation/Diagnostics/McpWorldTickTimer.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Framework/Docking/TabManager.h"
#include "Materials/MaterialInterface.h"
#include "RHI.h"
#include "StaticMeshResources.h"

namespace McpEnvironmentHandlers {
namespace {
constexpr int32 FrameCostRows = 5;
// "In view": drawn by any viewport within this many seconds (UPrimitiveComponent::WasRecentlyRendered).
constexpr float FrameCostInViewSeconds = 0.5f;

using FFrameCostRows = TArray<TPair<double, TSharedPtr<FJsonObject>>>;

void FrameCostAddRanked(const TCHAR* Field, FFrameCostRows& Rows, TSharedPtr<FJsonObject>& Resp)
{
    Rows.StableSort([](const TPair<double, TSharedPtr<FJsonObject>>& A, const TPair<double, TSharedPtr<FJsonObject>>& B)
    {
        return A.Key > B.Key;
    });
    TArray<TSharedPtr<FJsonValue>> Out;
    for (int32 Index = 0; Index < Rows.Num() && Index < FrameCostRows; ++Index)
    {
        Out.Add(MakeShared<FJsonValueObject>(Rows[Index].Value));
    }
    Resp->SetArrayField(Field, Out);
}

TSharedPtr<FJsonObject> FrameCostRow(const AActor* Actor)
{
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("actor"), Actor->GetActorNameOrLabel());
    return Row;
}

FString FrameCostLightType(const ULightComponent* Light)
{
    switch (Light->GetLightType())
    {
    case LightType_Directional: return TEXT("Directional");
    case LightType_Point: return TEXT("Point");
    case LightType_Spot: return TEXT("Spot");
    case LightType_Rect: return TEXT("Rect");
    default: return TEXT("Other");
    }
}

bool FrameCostTicks(const FTickFunction& Tick)
{
    return Tick.bCanEverTick && Tick.IsTickFunctionEnabled();
}

// What runs an actor's own tick: a Blueprint graph's Event Tick, or the Tick of its C++ class.
const TCHAR* FrameCostActorTick(const AActor* Actor)
{
    const UFunction* Event = Actor->GetClass()->FindFunctionByName(FName(TEXT("ReceiveTick")));
    return Event && Event->GetOwnerClass() != AActor::StaticClass() ? TEXT("Event Tick") : TEXT("actor Tick");
}

// The ticking actors of one class, and what ticks on them (a component class ticks without any graph).
struct FFrameCostTicking
{
    int32 Count = 0;
    TSet<FString> Sources;
};

// One mesh asset drawn without Nanite: every component that draws it counts toward its triangles.
struct FFrameCostMesh
{
    FString Actor;
    double Triangles = 0.0;
    int32 Components = 0;
    int32 Instances = 0;
};

struct FFrameCostScan
{
    int32 Primitives = 0, Meshes = 0, Nanite = 0, ShadowLights = 0, Translucent = 0, Ticking = 0;
    FFrameCostRows LightRows, TranslucentRows;
    TMap<FString, FFrameCostMesh> MeshByAsset;
    TMap<FString, FFrameCostTicking> TickingByClass;

    void AddLight(const AActor* Actor, const ULightComponent* Light)
    {
        if (!Light->IsVisible() || !Light->CastShadows || Light->Mobility != EComponentMobility::Movable) return;
        ++ShadowLights;
        const ULocalLightComponent* Local = Cast<ULocalLightComponent>(Light);
        const double Radius = Local ? Local->AttenuationRadius : 0.0;
        TSharedPtr<FJsonObject> Row = FrameCostRow(Actor);
        Row->SetStringField(TEXT("light"), FrameCostLightType(Light));
        Row->SetNumberField(TEXT("radius"), FMath::RoundToDouble(Radius));
        // A directional light reaches everything, so it leads.
        LightRows.Emplace(Local ? Radius : TNumericLimits<double>::Max(), Row);
    }

    void AddPrimitive(const AActor* Actor, const UPrimitiveComponent* Prim)
    {
        if (!Prim->IsVisible() || !Prim->WasRecentlyRendered(FrameCostInViewSeconds)) return;
        ++Primitives;
        if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Prim))
        {
            const UStaticMesh* Asset = Mesh->GetStaticMesh();
            const FStaticMeshRenderData* Render = Asset ? Asset->GetRenderData() : nullptr;
            if (Render && Render->LODResources.Num() > 0)
            {
                ++Meshes;
                if (Asset->HasValidNaniteData())
                {
                    ++Nanite;
                }
                else
                {
                    const UInstancedStaticMeshComponent* Instanced = Cast<UInstancedStaticMeshComponent>(Mesh);
                    const int32 Instances = Instanced ? Instanced->GetInstanceCount() : 1;
                    FFrameCostMesh& Entry = MeshByAsset.FindOrAdd(Asset->GetPathName());
                    if (Entry.Components++ == 0) Entry.Actor = Actor->GetActorNameOrLabel();
                    Entry.Instances += Instances;
                    Entry.Triangles += double(Render->LODResources[0].GetNumTriangles()) * Instances;
                }
            }
        }
        int32 Sections = 0;
        for (int32 Index = 0; Index < Prim->GetNumMaterials(); ++Index)
        {
            const UMaterialInterface* Material = Prim->GetMaterial(Index);
            const EBlendMode Blend = Material ? Material->GetBlendMode() : BLEND_Opaque;
            Sections += Blend != BLEND_Opaque && Blend != BLEND_Masked ? 1 : 0;
        }
        if (Sections == 0) return;
        ++Translucent;
        TSharedPtr<FJsonObject> Row = FrameCostRow(Actor);
        Row->SetStringField(TEXT("component"), Prim->GetName());
        Row->SetNumberField(TEXT("translucentSections"), Sections);
        TranslucentRows.Emplace(Sections, Row);
    }

    void AddActor(const AActor* Actor)
    {
        TSet<FString> Sources;
        if (FrameCostTicks(Actor->PrimaryActorTick)) Sources.Add(FrameCostActorTick(Actor));
        for (const UActorComponent* Component : Actor->GetComponents())
        {
            if (!Component) continue;
            if (FrameCostTicks(Component->PrimaryComponentTick)) Sources.Add(Component->GetClass()->GetName());
            if (const ULightComponent* Light = Cast<ULightComponent>(Component)) AddLight(Actor, Light);
            else if (const UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Component)) AddPrimitive(Actor, Prim);
        }
        if (Sources.Num() == 0) return;
        ++Ticking;
        FFrameCostTicking& Entry = TickingByClass.FindOrAdd(Actor->GetClass()->GetName());
        ++Entry.Count;
        Entry.Sources.Append(Sources);
    }
};

// Thread and frame times in milliseconds, and the world's own tick (-1 until it has ticked).
struct FFrameCostTimes { double Game, Render, Rhi, Gpu, Frame, WorldTick; bool bPie; };

FString FrameCostVerdict(const FString& Thread, double Ms, const FFrameCostTimes& T)
{
    const FString Times = FString::Printf(TEXT("game thread %.1f ms, render thread %.1f ms, RHI %.1f ms, GPU %.1f ms"), T.Game, T.Render, T.Rhi, T.Gpu);
    // No thread fills the frame: the engine waits between frames (a minimized or throttled editor, vsync, a frame rate cap).
    if (Ms < T.Frame * 0.5)
    {
        return FString::Printf(TEXT("No thread bounds the frame: a frame takes %.1f ms but the busiest thread (%s) %.1f ms (%s); the engine waits between frames, as a minimized or background-throttled editor, vsync or a frame rate cap make it. control_editor configure_editor setting window restores the editor and turns the background throttle off, for a real measure."),
            T.Frame, *Thread, Ms, *Times);
    }
    const bool bGame = Thread == TEXT("game");
    const TCHAR* Bound = Thread == TEXT("gpu") ? TEXT("GPU") : Thread == TEXT("rhi") ? TEXT("RHI-thread") : bGame ? TEXT("Game-thread") : TEXT("Render-thread");
    FString Hint = Thread == TEXT("gpu") ? TEXT(": look at movableShadowLights, heaviestMeshes and translucentPrimitives.")
        : Thread == TEXT("rhi") ? TEXT(": it hands the render thread's draw calls to the graphics driver, so look at primitivesInView and heaviestMeshes.")
        : !bGame ? TEXT(": look at primitivesInView, heaviestMeshes and translucentPrimitives.")
        : !T.bPie ? TEXT(": look at tickingActorsByClass; outside Play the game thread also runs the editor.")
        : TEXT(": look at tickingActorsByClass and worldTickMs.");
    // In Play the game thread also runs the editor's UI and viewports: say so when they, not the game, fill it.
    if (bGame && T.bPie && T.WorldTick >= 0.0 && T.WorldTick < Ms * 0.5)
    {
        Hint = FString::Printf(TEXT(", but the game's own tick takes only %.1f ms of it: the rest is the editor's UI and viewports, which a packaged game does not run. tickingActorsByClass shows the game's part."), T.WorldTick);
    }
    return FString::Printf(TEXT("%s-bound at %.1f ms (%s)%s"), Bound, Ms, *Times, *Hint);
}
} // namespace

bool HandleInspectFrameCostAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const bool bPie = GEditor && GEditor->PlayWorld;
    UWorld* World = bPie ? GEditor->PlayWorld.Get() : (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
    const double PieDelta = bPie && World ? World->GetDeltaSeconds() : 0.0;
    const FFrameCostTimes T{FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime),
        FPlatformTime::ToMilliseconds(GRHIThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()),
        (PieDelta > 0.0 ? PieDelta : FApp::GetDeltaTime()) * 1000.0, McpWorldTickTimer::AverageMs(World), bPie};
    struct FFrameCostThread { const TCHAR* Name; double Ms; };
    const FFrameCostThread Threads[] = {{TEXT("game"), T.Game}, {TEXT("render"), T.Render}, {TEXT("rhi"), T.Rhi}, {TEXT("gpu"), T.Gpu}};
    FString Thread;
    double BusiestMs = -1.0;
    for (const FFrameCostThread& Candidate : Threads)
    {
        if (Candidate.Ms > BusiestMs) { Thread = Candidate.Name; BusiestMs = Candidate.Ms; }
    }

    FFrameCostScan Scan;
    if (World)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (IsValid(*It)) Scan.AddActor(*It);
        }
    }
    FFrameCostRows MeshRows;
    for (const TPair<FString, FFrameCostMesh>& Entry : Scan.MeshByAsset)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("mesh"), Entry.Key);
        Row->SetNumberField(TEXT("triangles"), Entry.Value.Triangles);
        Row->SetNumberField(TEXT("components"), Entry.Value.Components);
        Row->SetNumberField(TEXT("instances"), Entry.Value.Instances);
        Row->SetStringField(TEXT("actor"), Entry.Value.Actor);
        MeshRows.Emplace(Entry.Value.Triangles, Row);
    }
    FFrameCostRows ClassRows;
    for (const TPair<FString, FFrameCostTicking>& Entry : Scan.TickingByClass)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("class"), Entry.Key);
        Row->SetNumberField(TEXT("count"), Entry.Value.Count);
        TArray<FString> Sources = Entry.Value.Sources.Array();
        Sources.Sort();
        TArray<TSharedPtr<FJsonValue>> Ticks;
        for (const FString& Source : Sources) Ticks.Add(MakeShared<FJsonValueString>(Source));
        Row->SetArrayField(TEXT("ticks"), Ticks);
        ClassRows.Emplace(Entry.Value.Count, Row);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("worldType"), bPie ? TEXT("PIE") : (World ? TEXT("Editor") : TEXT("None")));
    Resp->SetNumberField(TEXT("frameTimeMs"), T.Frame);
    Resp->SetStringField(TEXT("busiestThread"), Thread);
    Resp->SetNumberField(TEXT("busiestThreadMs"), BusiestMs);
    Resp->SetNumberField(TEXT("gameThreadMs"), T.Game);
    if (T.WorldTick >= 0.0) Resp->SetNumberField(TEXT("worldTickMs"), T.WorldTick);
    Resp->SetNumberField(TEXT("renderThreadMs"), T.Render);
    Resp->SetNumberField(TEXT("rhiThreadMs"), T.Rhi);
    Resp->SetNumberField(TEXT("gpuMs"), T.Gpu);
    Resp->SetNumberField(TEXT("primitivesInView"), Scan.Primitives);
    Resp->SetNumberField(TEXT("meshesInView"), Scan.Meshes);
    Resp->SetNumberField(TEXT("naniteMeshesInView"), Scan.Nanite);
    FrameCostAddRanked(TEXT("heaviestMeshes"), MeshRows, Resp);
    Resp->SetNumberField(TEXT("movableShadowLightCount"), Scan.ShadowLights);
    FrameCostAddRanked(TEXT("movableShadowLights"), Scan.LightRows, Resp);
    Resp->SetNumberField(TEXT("translucentPrimitiveCount"), Scan.Translucent);
    FrameCostAddRanked(TEXT("translucentPrimitives"), Scan.TranslucentRows, Resp);
    Resp->SetNumberField(TEXT("tickingActorCount"), Scan.Ticking);
    FrameCostAddRanked(TEXT("tickingActorsByClass"), ClassRows, Resp);

    // A minimized editor draws no frame: nothing reads as in view and the times are the last drawn frame's.
    const TSharedPtr<SWindow> Root = FGlobalTabmanager::Get()->GetRootWindow();
    TArray<TSharedPtr<FJsonValue>> Warnings;
    if (Root.IsValid() && Root->IsWindowMinimized())
    {
        Resp->SetBoolField(TEXT("windowMinimized"), true);
        Warnings.Add(MakeShared<FJsonValueString>(TEXT(
            "The editor window is minimized, so no frame is drawn: nothing reads as in view and the thread times are "
            "left from the last drawn frame. Restore it (control_editor configure_editor setting window) before measuring.")));
    }
    else if (World && Scan.Primitives == 0)
    {
        Warnings.Add(MakeShared<FJsonValueString>(TEXT(
            "Nothing was drawn in the last half second, so the in-view lists are empty: no viewport redraws this world "
            "(the editor is in the background and throttled, realtime is off, or its tab is hidden). control_editor "
            "configure_editor setting window restores it and lifts the throttle; then start Play or turn realtime on "
            "(control_editor configure_viewport) and read again.")));
    }
    if (Warnings.Num() > 0) Resp->SetArrayField(TEXT("warnings"), Warnings);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FrameCostVerdict(Thread, BusiestMs, T), Resp, FString());
    return true;
}

} // namespace McpEnvironmentHandlers
