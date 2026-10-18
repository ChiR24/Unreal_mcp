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

bool FrameCostTicks(const AActor* Actor)
{
    if (Actor->PrimaryActorTick.bCanEverTick && Actor->PrimaryActorTick.IsTickFunctionEnabled()) return true;
    for (const UActorComponent* Component : Actor->GetComponents())
    {
        if (Component && Component->PrimaryComponentTick.bCanEverTick && Component->PrimaryComponentTick.IsTickFunctionEnabled())
        {
            return true;
        }
    }
    return false;
}

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
    TMap<FString, int32> TickingByClass;

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
        if (FrameCostTicks(Actor))
        {
            ++Ticking;
            ++TickingByClass.FindOrAdd(Actor->GetClass()->GetName());
        }
        for (const UActorComponent* Component : Actor->GetComponents())
        {
            if (const ULightComponent* Light = Cast<ULightComponent>(Component)) AddLight(Actor, Light);
            else if (const UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Component)) AddPrimitive(Actor, Prim);
        }
    }
};

FString FrameCostVerdict(const FString& Thread, double Ms, double FrameMs, double GameMs, double RenderMs, double RhiMs, double GpuMs, bool bPie)
{
    const FString Times = FString::Printf(TEXT("game thread %.1f ms, render thread %.1f ms, RHI %.1f ms, GPU %.1f ms"), GameMs, RenderMs, RhiMs, GpuMs);
    // No thread fills the frame: the engine waits between frames (an unfocused or minimized editor, vsync, a frame rate cap).
    if (Ms < FrameMs * 0.5)
    {
        return FString::Printf(TEXT("No thread bounds the frame: a frame takes %.1f ms but the busiest thread (%s) %.1f ms (%s); the engine waits between frames, as an unfocused editor, vsync or a frame rate cap make it. Focus the editor window to measure."),
            FrameMs, *Thread, Ms, *Times);
    }
    if (Thread == TEXT("gpu"))
    {
        return FString::Printf(TEXT("GPU-bound at %.1f ms (%s): look at movableShadowLights, heaviestMeshes and translucentPrimitives."), Ms, *Times);
    }
    if (Thread == TEXT("game"))
    {
        return FString::Printf(TEXT("Game-thread-bound at %.1f ms (%s): look at tickingActorsByClass%s."), Ms, *Times,
            bPie ? TEXT(" and worldTickMs") : TEXT("; outside Play the game thread also runs the editor"));
    }
    return FString::Printf(TEXT("Render-thread-bound at %.1f ms (%s): look at primitivesInView, heaviestMeshes and translucentPrimitives."), Ms, *Times);
}
} // namespace

bool HandleInspectFrameCostAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const bool bPie = GEditor && GEditor->PlayWorld;
    UWorld* World = bPie ? GEditor->PlayWorld.Get() : (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
    const double GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
    const double RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
    const double RhiMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);
    const double GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
    const double PieDelta = bPie && World ? World->GetDeltaSeconds() : 0.0;
    const double FrameMs = (PieDelta > 0.0 ? PieDelta : FApp::GetDeltaTime()) * 1000.0;
    struct FFrameCostThread { const TCHAR* Name; double Ms; };
    const FFrameCostThread Threads[] = {{TEXT("game"), GameMs}, {TEXT("render"), RenderMs}, {TEXT("rhi"), RhiMs}, {TEXT("gpu"), GpuMs}};
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
    for (const TPair<FString, int32>& Entry : Scan.TickingByClass)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("class"), Entry.Key);
        Row->SetNumberField(TEXT("count"), Entry.Value);
        ClassRows.Emplace(Entry.Value, Row);
    }

    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("worldType"), bPie ? TEXT("PIE") : (World ? TEXT("Editor") : TEXT("None")));
    Resp->SetNumberField(TEXT("frameTimeMs"), FrameMs);
    Resp->SetStringField(TEXT("busiestThread"), Thread);
    Resp->SetNumberField(TEXT("busiestThreadMs"), BusiestMs);
    Resp->SetNumberField(TEXT("gameThreadMs"), GameMs);
    const double WorldTickMs = McpWorldTickTimer::AverageMs(World);
    if (WorldTickMs >= 0.0) Resp->SetNumberField(TEXT("worldTickMs"), WorldTickMs);
    Resp->SetNumberField(TEXT("renderThreadMs"), RenderMs);
    Resp->SetNumberField(TEXT("rhiThreadMs"), RhiMs);
    Resp->SetNumberField(TEXT("gpuMs"), GpuMs);
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
    if (Warnings.Num() > 0) Resp->SetArrayField(TEXT("warnings"), Warnings);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FrameCostVerdict(Thread, BusiestMs, FrameMs, GameMs, RenderMs, RhiMs, GpuMs, bPie), Resp, FString());
    return true;
}

} // namespace McpEnvironmentHandlers
