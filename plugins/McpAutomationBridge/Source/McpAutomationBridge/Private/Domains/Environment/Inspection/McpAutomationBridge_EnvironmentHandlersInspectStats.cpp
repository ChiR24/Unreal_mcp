#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
#include "Animation/SkeletalMeshActor.h"
#include "Camera/CameraActor.h"
#include "Engine/Brush.h"
#include "Engine/Light.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Volume.h"

namespace McpEnvironmentHandlers {

namespace {
// Exclusive class buckets over every AActor in the world plus the component
// total. blueprintActors is counted independently: a Blueprint-generated
// StaticMeshActor lands in staticMeshActors AND blueprintActors.
void McpAppendSceneStatCategories(UWorld *World, TSharedPtr<FJsonObject> Resp)
{
    int32 StaticMeshActors = 0, SkeletalMeshActors = 0, Lights = 0, Cameras = 0;
    int32 Volumes = 0, Brushes = 0, BlueprintActors = 0, Other = 0, Components = 0, Hidden = 0;
    if (World)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor *Actor = *It;
            if (!Actor)
            {
                continue;
            }
            Components += Actor->GetComponents().Num();
            Hidden += Actor->IsHidden() ? 1 : 0;
            BlueprintActors += Actor->GetClass()->ClassGeneratedBy != nullptr ? 1 : 0;
            if (Actor->IsA<AStaticMeshActor>())
            {
                ++StaticMeshActors;
            }
            else if (Actor->IsA<ASkeletalMeshActor>())
            {
                ++SkeletalMeshActors;
            }
            else if (Actor->IsA<ALight>())
            {
                ++Lights;
            }
            else if (Actor->IsA<ACameraActor>())
            {
                ++Cameras;
            }
            else if (Actor->IsA<AVolume>())
            {
                ++Volumes;
            }
            else if (Actor->IsA<ABrush>())
            {
                ++Brushes;
            }
            else
            {
                ++Other;
            }
        }
    }
    TSharedPtr<FJsonObject> Categories = McpHandlerUtils::CreateResultObject();
    Categories->SetNumberField(TEXT("staticMeshActors"), StaticMeshActors);
    Categories->SetNumberField(TEXT("skeletalMeshActors"), SkeletalMeshActors);
    Categories->SetNumberField(TEXT("lights"), Lights);
    Categories->SetNumberField(TEXT("cameras"), Cameras);
    Categories->SetNumberField(TEXT("volumes"), Volumes);
    Categories->SetNumberField(TEXT("brushes"), Brushes);
    Categories->SetNumberField(TEXT("blueprintActors"), BlueprintActors);
    Categories->SetNumberField(TEXT("other"), Other);
    Resp->SetObjectField(TEXT("actorsByCategory"), Categories);
    Resp->SetNumberField(TEXT("componentCount"), Components);
    Resp->SetNumberField(TEXT("hiddenActorCount"), Hidden);
}
} // namespace

// Stats sub-actions of the inspect settings dispatcher: get_scene_stats,
// get_performance_stats, get_memory_stats. Split from
// McpAutomationBridge_EnvironmentHandlersInspectSettings.cpp so each file
// stays inside the 250-pure-line CI ceiling; the dispatcher delegates here
// unchanged.

bool HandleInspectStatsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSubAction,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp)
{
        if (LowerSubAction.Equals(TEXT("get_scene_stats")))
        {
            // Count the SAME actor set control_actor.list reports (editor:
            // EditorActorSubsystem level actors; PIE: the PIE world). A raw
            // TActorIterator also counts editor-internal actors and read ~9
            // higher than list in the same session — publish both so the
            // difference is visible instead of contradictory.
            int32 ActorCount = 0;
            int32 TotalWorldActors = 0;
            UWorld* World = nullptr;
            if (GEditor && GEditor->PlayWorld)
            {
                World = GEditor->PlayWorld.Get();
            }
            else if (GEditor)
            {
                World = GEditor->GetEditorWorldContext().World();
            }
            if (World)
            {
                if (GEditor->PlayWorld.Get() == World)
                {
                    // PIE: the level-actor set IS the PIE world's actors.
                    for (TActorIterator<AActor> It(World); It; ++It)
                    {
                        ++ActorCount;
                        ++TotalWorldActors;
                    }
                }
                else
                {
                    if (UEditorActorSubsystem* ActorSS =
                            GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
                    {
                        for (AActor* Actor : ActorSS->GetAllLevelActors())
                        {
                            if (Actor) ++ActorCount;
                        }
                    }
                    for (TActorIterator<AActor> It(World); It; ++It)
                    {
                        ++TotalWorldActors;
                    }
                }
            }
            Resp->SetNumberField(TEXT("actorCount"), ActorCount);
            Resp->SetNumberField(TEXT("totalWorldActors"), TotalWorldActors);
            // Both numbers under explicit names: nonEditorActorCount is the
            // level-actor set control_actor.list reports; allActorCount is
            // every AActor (WorldSettings, builder brush, DefaultPhysicsVolume
            // and other editor-only/transient actors included).
            Resp->SetNumberField(TEXT("nonEditorActorCount"), ActorCount);
            Resp->SetNumberField(TEXT("allActorCount"), TotalWorldActors);
            Resp->SetNumberField(TEXT("editorOnlyActorCount"), FMath::Max(0, TotalWorldActors - ActorCount));
            Resp->SetStringField(TEXT("actorCountSemantics"),
                TEXT("actorCount/nonEditorActorCount = level actors (EditorActorSubsystem.GetAllLevelActors, the same set control_actor.list reports); ")
                TEXT("allActorCount/totalWorldActors = every AActor in the world, including editor-only/transient actors such as WorldSettings, the builder brush and DefaultPhysicsVolume."));
            McpAppendSceneStatCategories(World, Resp);
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Scene stats retrieved"), Resp, FString());
            return true;
        }
        else if (LowerSubAction.Equals(TEXT("get_performance_stats")))
        {
            // BUG-e85282: when a PIE/play session is active, measure THAT world (actor count + frame delta), not
            // the editor world. GEditor->PlayWorld is the active PIE world (nullptr outside PIE). The thread
            // timers below are process-global (they reflect whatever the engine is ticking) — flagged as such.
            double DeltaSeconds = FApp::GetDeltaTime();
            UWorld* StatWorld = nullptr;
            FString WorldType = TEXT("None");
            if (GEditor && GEditor->PlayWorld)
            {
                StatWorld = GEditor->PlayWorld;
                WorldType = TEXT("PIE");
                const double PieDelta = StatWorld->GetDeltaSeconds();
                if (PieDelta > 0.0)
                {
                    DeltaSeconds = PieDelta;
                }
            }
            else if (GEditor && GEditor->GetEditorWorldContext().World())
            {
                StatWorld = GEditor->GetEditorWorldContext().World();
                WorldType = TEXT("Editor");
            }

            const double FrameTimeMs = DeltaSeconds > 0.0 ? DeltaSeconds * 1000.0 : 0.0;
            const double EstimatedFps = DeltaSeconds > 0.0 ? 1.0 / DeltaSeconds : 0.0;
            const double GameThreadMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
            const double RenderThreadMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
            const double RHIThreadMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);
            const double GPUFrameMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());

            int32 ActorCount = 0;
            if (StatWorld)
            {
                for (TActorIterator<AActor> It(StatWorld); It; ++It)
                {
                    ActorCount++;
                }
            }

            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetStringField(TEXT("worldType"), WorldType);
            Resp->SetBoolField(TEXT("threadTimersAreProcessGlobal"), true);
            Resp->SetNumberField(TEXT("deltaSeconds"), DeltaSeconds);
            Resp->SetNumberField(TEXT("frameTimeMs"), FrameTimeMs);
            Resp->SetNumberField(TEXT("estimatedFps"), EstimatedFps);
            Resp->SetNumberField(TEXT("fps"), EstimatedFps);
            // `fps` comes from the frame delta, which an idle/unfocused editor
            // throttles hard — it read 3 FPS while the viewport showed 60 and
            // the thread timings implied ~84. Publish the busiest thread and a
            // throttle flag so a consumer can tell a real stall from an idle
            // editor instead of reading `fps` as a performance verdict.
            const double BusiestThreadMs = FMath::Max(
                FMath::Max(GameThreadMs, RenderThreadMs), GPUFrameMs);
            Resp->SetNumberField(TEXT("busiestThreadMs"), BusiestThreadMs);
            Resp->SetNumberField(TEXT("threadTimeDerivedFps"),
                                 BusiestThreadMs > 0.0 ? 1000.0 / BusiestThreadMs : 0.0);
            Resp->SetBoolField(TEXT("frameDeltaMayBeEditorThrottled"),
                               WorldType != TEXT("PIE"));
            Resp->SetNumberField(TEXT("gameThreadMs"), GameThreadMs);
            Resp->SetNumberField(TEXT("renderThreadMs"), RenderThreadMs);
            Resp->SetNumberField(TEXT("rhiThreadMs"), RHIThreadMs);
            Resp->SetNumberField(TEXT("gpuMs"), GPUFrameMs);
            Resp->SetNumberField(TEXT("actorCount"), ActorCount);
            Resp->SetBoolField(TEXT("isBenchmarking"), FApp::IsBenchmarking());
            Resp->SetBoolField(TEXT("useFixedTimeStep"), FApp::UseFixedTimeStep());
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Performance stats retrieved"), Resp, FString());
            return true;
        }
        else if (LowerSubAction.Equals(TEXT("get_memory_stats")))
        {
            const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
            const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetNumberField(TEXT("totalPhysicalBytes"), static_cast<double>(MemoryStats.TotalPhysical));
            Resp->SetNumberField(TEXT("availablePhysicalBytes"), static_cast<double>(MemoryStats.AvailablePhysical));
            Resp->SetNumberField(TEXT("usedPhysicalBytes"), static_cast<double>(MemoryStats.UsedPhysical));
            Resp->SetNumberField(TEXT("peakUsedPhysicalBytes"), static_cast<double>(MemoryStats.PeakUsedPhysical));
            Resp->SetNumberField(TEXT("totalVirtualBytes"), static_cast<double>(MemoryStats.TotalVirtual));
            Resp->SetNumberField(TEXT("availableVirtualBytes"), static_cast<double>(MemoryStats.AvailableVirtual));
            Resp->SetNumberField(TEXT("usedVirtualBytes"), static_cast<double>(MemoryStats.UsedVirtual));
            Resp->SetNumberField(TEXT("peakUsedVirtualBytes"), static_cast<double>(MemoryStats.PeakUsedVirtual));
            Resp->SetNumberField(TEXT("totalPhysicalMB"), static_cast<double>(MemoryConstants.TotalPhysical) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("totalVirtualMB"), static_cast<double>(MemoryConstants.TotalVirtual) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("availablePhysicalMB"), static_cast<double>(MemoryStats.AvailablePhysical) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("availableVirtualMB"), static_cast<double>(MemoryStats.AvailableVirtual) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("usedPhysicalMB"), static_cast<double>(MemoryStats.UsedPhysical) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("usedVirtualMB"), static_cast<double>(MemoryStats.UsedVirtual) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("peakUsedPhysicalMB"), static_cast<double>(MemoryStats.PeakUsedPhysical) / (1024.0 * 1024.0));
            Resp->SetNumberField(TEXT("peakUsedVirtualMB"), static_cast<double>(MemoryStats.PeakUsedVirtual) / (1024.0 * 1024.0));
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Memory stats retrieved"), Resp, FString());
            return true;
        }
    return false;
}

} // namespace McpEnvironmentHandlers
#endif
