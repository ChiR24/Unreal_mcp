#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

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
                // Level summary (levelPath, streamingLevels, worldSettings,
                // lightingBuilt...) shared with get_level_details, which the
                // TypeScript surface aliases to this action.
                McpAppendLevelDetails(World, Resp);
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
        else if (LowerSubAction.Equals(TEXT("get_viewport_info")))
        {
            // Size, the level viewport camera/view mode/realtime flag, world
            // type and (in PIE) the view target + camera manager — see
            // McpAutomationBridge_EnvironmentHandlersInspectViewport.cpp.
            McpAppendViewportInfo(Resp);
            Resp->SetBoolField(TEXT("success"), true);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Viewport info retrieved"), Resp, FString());
            return true;
        }
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
        // get_scene_stats / get_performance_stats / get_memory_stats live in
        // McpAutomationBridge_EnvironmentHandlersInspectStats.cpp (split for the
        // 250-pure-line ceiling). Delegate to it; it returns false for anything
        // it does not own, which reaches this function's final return false.
        return HandleInspectStatsAction(Bridge, RequestId, LowerSubAction,
                                        RequestingSocket, Resp);
}

} // namespace McpEnvironmentHandlers
#endif
