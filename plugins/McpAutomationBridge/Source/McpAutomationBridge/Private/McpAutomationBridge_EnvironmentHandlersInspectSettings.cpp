#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool HandleInspectSettingsAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &SubAction, const FString &LowerSubAction,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> Resp)
{
        if (LowerSubAction.Equals(TEXT("get_project_settings")))
        {
            Resp->SetStringField(TEXT("action"), TEXT("inspect"));
            Resp->SetStringField(TEXT("subAction"), SubAction);
            Resp->SetStringField(TEXT("message"), TEXT("Project settings retrieved"));
            Resp->SetStringField(TEXT("projectName"), FApp::GetProjectName());
            Resp->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
            Resp->SetStringField(TEXT("buildConfig"), LexToString(FApp::GetBuildConfiguration()));
            Resp->SetStringField(TEXT("projectDir"), FPaths::ProjectDir());
            if (const UGeneralProjectSettings* ProjectSettings = GetDefault<UGeneralProjectSettings>())
            {
                Resp->SetStringField(TEXT("description"), ProjectSettings->Description);
                Resp->SetStringField(TEXT("homepage"), ProjectSettings->Homepage);
                Resp->SetStringField(TEXT("supportContact"), ProjectSettings->SupportContact);
                Resp->SetStringField(TEXT("projectVersion"), ProjectSettings->ProjectVersion);
                Resp->SetStringField(TEXT("companyName"), ProjectSettings->CompanyName);
                Resp->SetStringField(TEXT("copyrightNotice"), ProjectSettings->CopyrightNotice);
                Resp->SetStringField(TEXT("projectID"), ProjectSettings->ProjectID.ToString());
                Resp->SetBoolField(TEXT("startInVR"), ProjectSettings->bStartInVR);
            }
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Project settings retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_editor_settings
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_editor_settings")))
        {
            Resp->SetStringField(TEXT("action"), TEXT("inspect"));
            Resp->SetStringField(TEXT("subAction"), SubAction);
            Resp->SetStringField(TEXT("message"), TEXT("Editor settings retrieved"));
            if (const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>())
            {
                Resp->SetNumberField(TEXT("mouseSensitivity"), ViewportSettings->MouseSensitivty);
                Resp->SetNumberField(TEXT("mouseScrollCameraSpeed"), ViewportSettings->MouseScrollCameraSpeed);
                Resp->SetBoolField(TEXT("useDistanceScaledCamera"), ViewportSettings->bUseDistanceScaledCameraSpeed);
            }
            if (GEditor)
            {
                Resp->SetBoolField(TEXT("isSimulating"), GEditor->bIsSimulatingInEditor);
                Resp->SetBoolField(TEXT("isPIEActive"), GEditor->PlayWorld != nullptr);
                Resp->SetNumberField(TEXT("gameAgnosticSavedFPS"), GEngine ? GEngine->GetMaxFPS() : 0.0);
            }
            Resp->SetBoolField(TEXT("isEditor"), GIsEditor);
            Resp->SetNumberField(TEXT("gRunningCommandlet"), IsRunningCommandlet() ? 1 : 0);
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Editor settings retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_world_settings
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_world_settings")))
        {
            UWorld* World = McpGetRuntimeInspectionWorld();

            if (World)
            {
                Resp->SetStringField(TEXT("worldName"), World->GetName());
                if (ULevel* CurrentLevel = World->GetCurrentLevel())
                {
                    Resp->SetStringField(TEXT("levelName"), CurrentLevel->GetName());
                }
                Resp->SetStringField(TEXT("packageName"), World->GetOutermost()->GetName());
                Resp->SetNumberField(TEXT("timeSeconds"), World->GetTimeSeconds());
                Resp->SetNumberField(TEXT("realTimeSeconds"), World->GetRealTimeSeconds());
                Resp->SetNumberField(TEXT("deltaTimeSeconds"), World->GetDeltaSeconds());
                Resp->SetBoolField(TEXT("hasBegunPlay"), World->HasBegunPlay());
                Resp->SetBoolField(TEXT("isPlayInEditor"), World->IsPlayInEditor());
                if (AWorldSettings* WorldSettings = World->GetWorldSettings())
                {
                    Resp->SetNumberField(TEXT("killZ"), WorldSettings->KillZ);
                    Resp->SetNumberField(TEXT("worldGravityZ"), WorldSettings->GetGravityZ());
                    Resp->SetNumberField(TEXT("timeDilation"), WorldSettings->TimeDilation);
                    Resp->SetBoolField(TEXT("enableWorldBoundsChecks"), WorldSettings->bEnableWorldBoundsChecks);
                    if (UClass* GameModeClass = WorldSettings->DefaultGameMode.Get())
                    {
                        Resp->SetStringField(TEXT("defaultGameMode"), GameModeClass->GetPathName());
                    }
                }
                Resp->SetBoolField(TEXT("success"), true);
                Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                       TEXT("World settings retrieved"), Resp, FString());
            }
            else
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("No world available"),
                                    TEXT("WORLD_NOT_FOUND"));
            }
            return true;
        }
        // ---------------------------------------------------------------------
        // get_viewport_info
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_viewport_info")))
        {
            if (GEditor && GEditor->GetActiveViewport())
            {
                FViewport* Viewport = GEditor->GetActiveViewport();
                Resp->SetNumberField(TEXT("width"), Viewport->GetSizeXY().X);
                Resp->SetNumberField(TEXT("height"), Viewport->GetSizeXY().Y);
                Resp->SetBoolField(TEXT("success"), true);
                Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                       TEXT("Viewport info retrieved"), Resp, FString());
            }
            else
            {
                Resp->SetBoolField(TEXT("success"), true);
                Resp->SetStringField(TEXT("message"), TEXT("Viewport info not available in this context"));
                Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                       TEXT("Viewport info retrieved"), Resp, FString());
            }
            return true;
        }
        // ---------------------------------------------------------------------
        // get_selected_actors
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_selected_actors")))
        {
            TArray<TSharedPtr<FJsonValue>> ActorsArray;
            if (GEditor)
            {
                TArray<AActor*> SelectedActors;
                GEditor->GetSelectedActors()->GetSelectedObjects(SelectedActors);
                for (AActor* Actor : SelectedActors)
                {
                    if (Actor)
                    {
                        TSharedPtr<FJsonObject> ActorObj = McpHandlerUtils::CreateResultObject();
                        ActorObj->SetStringField(TEXT("name"), Actor->GetName());
                        ActorObj->SetStringField(TEXT("path"), Actor->GetPathName());
                        ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
                        ActorsArray.Add(MakeShared<FJsonValueObject>(ActorObj));
                    }
                }
            }
            Resp->SetArrayField(TEXT("actors"), ActorsArray);
            Resp->SetNumberField(TEXT("count"), ActorsArray.Num());
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Selected actors retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_scene_stats
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_scene_stats")))
        {
            int32 ActorCount = 0;
            if (GEditor && GEditor->GetEditorWorldContext().World())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    ActorCount++;
                }
            }
            Resp->SetNumberField(TEXT("actorCount"), ActorCount);
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Scene stats retrieved"), Resp, FString());
            return true;
        }
        // ---------------------------------------------------------------------
        // get_performance_stats
        // ---------------------------------------------------------------------
        else if (LowerSubAction.Equals(TEXT("get_performance_stats")))
        {
            const double DeltaSeconds = FApp::GetDeltaTime();
            const double FrameTimeMs = DeltaSeconds > 0.0 ? DeltaSeconds * 1000.0 : 0.0;
            const double EstimatedFps = DeltaSeconds > 0.0 ? 1.0 / DeltaSeconds : 0.0;
            const double GameThreadMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
            const double RenderThreadMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
            const double RHIThreadMs = FPlatformTime::ToMilliseconds(GRHIThreadTime);
            const double GPUFrameMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());

            int32 ActorCount = 0;
            if (GEditor && GEditor->GetEditorWorldContext().World())
            {
                UWorld* World = GEditor->GetEditorWorldContext().World();
                for (TActorIterator<AActor> It(World); It; ++It)
                {
                    ActorCount++;
                }
            }

            Resp->SetBoolField(TEXT("success"), true);
            Resp->SetNumberField(TEXT("deltaSeconds"), DeltaSeconds);
            Resp->SetNumberField(TEXT("frameTimeMs"), FrameTimeMs);
            Resp->SetNumberField(TEXT("estimatedFps"), EstimatedFps);
            Resp->SetNumberField(TEXT("fps"), EstimatedFps);
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
        // ---------------------------------------------------------------------
        // get_memory_stats
        // ---------------------------------------------------------------------
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
    else
    {
        return false;
    }

    return true;
}

} // namespace McpEnvironmentHandlers
#endif
