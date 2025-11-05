#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "Editor.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Math/UnrealMathUtility.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBuildEnvironmentAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("build_environment"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("build_environment"))) return false;

    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("build_environment payload missing."), TEXT("INVALID_PAYLOAD")); return true; }

    FString SubAction; Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

    // Fast-path foliage sub-actions to dedicated native handlers to avoid double responses
    if (LowerSub == TEXT("add_foliage_instances"))
    {
        // Transform from build_environment schema to foliage handler schema
        FString FoliageTypePath; Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        const TArray<TSharedPtr<FJsonValue>>* Transforms = nullptr; Payload->TryGetArrayField(TEXT("transforms"), Transforms);
        TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>();
        if (!FoliageTypePath.IsEmpty()) { FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath); }
        TArray<TSharedPtr<FJsonValue>> Locations;
        if (Transforms)
        {
            for (const TSharedPtr<FJsonValue>& V : *Transforms)
            {
                if (!V.IsValid() || V->Type != EJson::Object) continue;
                const TSharedPtr<FJsonObject>* TObj = nullptr; if (!V->TryGetObject(TObj) || !TObj) continue;
                const TSharedPtr<FJsonObject>* LocObj = nullptr; if (!(*TObj)->TryGetObjectField(TEXT("location"), LocObj) || !LocObj) continue;
                double X=0,Y=0,Z=0; (*LocObj)->TryGetNumberField(TEXT("x"), X); (*LocObj)->TryGetNumberField(TEXT("y"), Y); (*LocObj)->TryGetNumberField(TEXT("z"), Z);
                TSharedPtr<FJsonObject> L = MakeShared<FJsonObject>(); L->SetNumberField(TEXT("x"), X); L->SetNumberField(TEXT("y"), Y); L->SetNumberField(TEXT("z"), Z);
                Locations.Add(MakeShared<FJsonValueObject>(L));
            }
        }
        FoliagePayload->SetArrayField(TEXT("locations"), Locations);
        return HandlePaintFoliage(RequestId, TEXT("paint_foliage"), FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("get_foliage_instances"))
    {
        FString FoliageTypePath; Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>();
        if (!FoliageTypePath.IsEmpty()) { FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath); }
        return HandleGetFoliageInstances(RequestId, TEXT("get_foliage_instances"), FoliagePayload, RequestingSocket);
    }
    else if (LowerSub == TEXT("remove_foliage"))
    {
        FString FoliageTypePath; Payload->TryGetStringField(TEXT("foliageType"), FoliageTypePath);
        bool bRemoveAll = false; Payload->TryGetBoolField(TEXT("removeAll"), bRemoveAll);
        TSharedPtr<FJsonObject> FoliagePayload = MakeShared<FJsonObject>();
        if (!FoliageTypePath.IsEmpty()) { FoliagePayload->SetStringField(TEXT("foliageTypePath"), FoliageTypePath); }
        FoliagePayload->SetBoolField(TEXT("removeAll"), bRemoveAll);
        return HandleRemoveFoliage(RequestId, TEXT("remove_foliage"), FoliagePayload, RequestingSocket);
    }

#if WITH_EDITOR
    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetStringField(TEXT("action"), LowerSub);
    bool bSuccess = true;
    FString Message = FString::Printf(TEXT("Environment action '%s' completed"), *LowerSub);
    FString ErrorCode;

    if (LowerSub == TEXT("export_snapshot"))
    {
        FString Path; Payload->TryGetStringField(TEXT("path"), Path);
        if (Path.IsEmpty())
        {
            bSuccess = false;
            Message = TEXT("path required for export_snapshot");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            TSharedPtr<FJsonObject> Snapshot = MakeShared<FJsonObject>();
            Snapshot->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToString());
            Snapshot->SetStringField(TEXT("type"), TEXT("environment_snapshot"));

            FString JsonString;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
            if (FJsonSerializer::Serialize(Snapshot.ToSharedRef(), Writer))
            {
                if (FFileHelper::SaveStringToFile(JsonString, *Path))
                {
                    Resp->SetStringField(TEXT("exportPath"), Path);
                    Resp->SetStringField(TEXT("message"), TEXT("Snapshot exported"));
                }
                else
                {
                    bSuccess = false;
                    Message = TEXT("Failed to write snapshot file");
                    ErrorCode = TEXT("WRITE_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                }
            }
            else
            {
                bSuccess = false;
                Message = TEXT("Failed to serialize snapshot");
                ErrorCode = TEXT("SERIALIZE_FAILED");
                Resp->SetStringField(TEXT("error"), Message);
            }
        }
    }
    else if (LowerSub == TEXT("import_snapshot"))
    {
        FString Path; Payload->TryGetStringField(TEXT("path"), Path);
        if (Path.IsEmpty())
        {
            bSuccess = false;
            Message = TEXT("path required for import_snapshot");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            FString JsonString;
            if (!FFileHelper::LoadFileToString(JsonString, *Path))
            {
                bSuccess = false;
                Message = TEXT("Failed to read snapshot file");
                ErrorCode = TEXT("LOAD_FAILED");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                TSharedPtr<FJsonObject> SnapshotObj;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
                if (!FJsonSerializer::Deserialize(Reader, SnapshotObj) || !SnapshotObj.IsValid())
                {
                    bSuccess = false;
                    Message = TEXT("Failed to parse snapshot");
                    ErrorCode = TEXT("PARSE_FAILED");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    Resp->SetObjectField(TEXT("snapshot"), SnapshotObj.ToSharedRef());
                    Resp->SetStringField(TEXT("message"), TEXT("Snapshot imported"));
                }
            }
        }
    }
    else if (LowerSub == TEXT("delete"))
    {
        const TArray<TSharedPtr<FJsonValue>>* NamesArray = nullptr;
        if (!Payload->TryGetArrayField(TEXT("names"), NamesArray) || !NamesArray)
        {
            bSuccess = false;
            Message = TEXT("names array required for delete");
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else if (!GEditor)
        {
            bSuccess = false;
            Message = TEXT("Editor not available");
            ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
            Resp->SetStringField(TEXT("error"), Message);
        }
        else
        {
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
            if (!ActorSS)
            {
                bSuccess = false;
                Message = TEXT("EditorActorSubsystem not available");
                ErrorCode = TEXT("EDITOR_ACTOR_SUBSYSTEM_MISSING");
                Resp->SetStringField(TEXT("error"), Message);
            }
            else
            {
                TArray<FString> Deleted;
                TArray<FString> Missing;
                for (const TSharedPtr<FJsonValue>& Val : *NamesArray)
                {
                    if (Val.IsValid() && Val->Type == EJson::String)
                    {
                        FString Name = Val->AsString();
                        TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
                        bool bRemoved = false;
                        for (AActor* A : AllActors)
                        {
                            if (A && A->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase))
                            {
                                if (ActorSS->DestroyActor(A))
                                {
                                    Deleted.Add(Name);
                                    bRemoved = true;
                                }
                                break;
                            }
                        }
                        if (!bRemoved)
                        {
                            Missing.Add(Name);
                        }
                    }
                }

                TArray<TSharedPtr<FJsonValue>> DeletedArray;
                for (const FString& Name : Deleted)
                {
                    DeletedArray.Add(MakeShared<FJsonValueString>(Name));
                }
                Resp->SetArrayField(TEXT("deleted"), DeletedArray);
                Resp->SetNumberField(TEXT("deletedCount"), Deleted.Num());

                if (Missing.Num() > 0)
                {
                    TArray<TSharedPtr<FJsonValue>> MissingArray;
                    for (const FString& Name : Missing)
                    {
                        MissingArray.Add(MakeShared<FJsonValueString>(Name));
                    }
                    Resp->SetArrayField(TEXT("missing"), MissingArray);
                    bSuccess = false;
                    Message = TEXT("Some environment actors could not be removed");
                    ErrorCode = TEXT("DELETE_PARTIAL");
                    Resp->SetStringField(TEXT("error"), Message);
                }
                else
                {
                    Message = TEXT("Environment actors deleted");
                }
            }
        }
    }
    else if (LowerSub == TEXT("create_sky_sphere"))
    {
        if (GEditor)
        {
            UClass* SkySphereClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.Blueprint'/Engine/Maps/Templates/SkySphere.SkySphere_C'"));
            if (SkySphereClass)
            {
                UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
                if (ActorSS)
                {
                    AActor* SkySphere = ActorSS->SpawnActorFromClass(SkySphereClass, FVector::ZeroVector, FRotator::ZeroRotator);
                    if (SkySphere)
                    {
                        bSuccess = true;
                        Message = TEXT("Sky sphere created");
                        Resp->SetStringField(TEXT("actorName"), SkySphere->GetActorLabel());
                    }
                }
            }
        }
        if (!bSuccess)
        {
            bSuccess = false;
            Message = TEXT("Failed to create sky sphere");
            ErrorCode = TEXT("CREATION_FAILED");
        }
    }
    else if (LowerSub == TEXT("set_time_of_day"))
    {
        float TimeOfDay = 12.0f;
        Payload->TryGetNumberField(TEXT("time"), TimeOfDay);
        
        if (GEditor)
        {
            UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
            if (ActorSS)
            {
                for (AActor* Actor : ActorSS->GetAllLevelActors())
                {
                    if (Actor->GetClass()->GetName().Contains(TEXT("SkySphere")))
                    {
                        UFunction* SetTimeFunction = Actor->FindFunction(TEXT("SetTimeOfDay"));
                        if (SetTimeFunction)
                        {
                            float TimeParam = TimeOfDay;
                            Actor->ProcessEvent(SetTimeFunction, &TimeParam);
                            bSuccess = true;
                            Message = FString::Printf(TEXT("Time of day set to %.2f"), TimeOfDay);
                            break;
                        }
                    }
                }
            }
        }
        if (!bSuccess)
        {
            bSuccess = false;
            Message = TEXT("Sky sphere not found or time function not available");
            ErrorCode = TEXT("SET_TIME_FAILED");
        }
    }
    else if (LowerSub == TEXT("create_fog_volume"))
    {
        FVector Location(0, 0, 0);
        Payload->TryGetNumberField(TEXT("x"), Location.X);
        Payload->TryGetNumberField(TEXT("y"), Location.Y);
        Payload->TryGetNumberField(TEXT("z"), Location.Z);
        
        if (GEditor)
        {
            UClass* FogClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.ExponentialHeightFog"));
            if (FogClass)
            {
                UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
                if (ActorSS)
                {
                    AActor* FogVolume = ActorSS->SpawnActorFromClass(FogClass, Location, FRotator::ZeroRotator);
                    if (FogVolume)
                    {
                        bSuccess = true;
                        Message = TEXT("Fog volume created");
                        Resp->SetStringField(TEXT("actorName"), FogVolume->GetActorLabel());
                    }
                }
            }
        }
        if (!bSuccess)
        {
            bSuccess = false;
            Message = TEXT("Failed to create fog volume");
            ErrorCode = TEXT("CREATION_FAILED");
        }
    }
    else
    {
        bSuccess = false;
        Message = FString::Printf(TEXT("Environment action '%s' not implemented"), *LowerSub);
        ErrorCode = TEXT("NOT_IMPLEMENTED");
        Resp->SetStringField(TEXT("error"), Message);
    }

    Resp->SetBoolField(TEXT("success"), bSuccess);
    SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp, ErrorCode);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Environment building actions require editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEnvironmentAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("control_environment"), ESearchCase::IgnoreCase) && !Lower.StartsWith(TEXT("control_environment")))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("control_environment payload missing."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction;
    Payload->TryGetStringField(TEXT("action"), SubAction);
    const FString LowerSub = SubAction.ToLower();

#if WITH_EDITOR
    auto SendResult = [&](bool bSuccess, const TCHAR* Message, const FString& ErrorCode, const TSharedPtr<FJsonObject>& Result)
    {
        if (bSuccess)
        {
            SendAutomationResponse(RequestingSocket, RequestId, true, Message ? Message : TEXT("Environment control succeeded."), Result, FString());
        }
        else
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, Message ? Message : TEXT("Environment control failed."), Result, ErrorCode);
        }
    };

    UWorld* World = nullptr;
    if (GEditor)
    {
        World = GEditor->GetEditorWorldContext().World();
    }

    if (!World)
    {
        SendResult(false, TEXT("Editor world is unavailable"), TEXT("WORLD_NOT_AVAILABLE"), nullptr);
        return true;
    }

    auto FindFirstDirectionalLight = [&]() -> ADirectionalLight*
    {
        for (TActorIterator<ADirectionalLight> It(World); It; ++It)
        {
            if (ADirectionalLight* Light = *It)
            {
                if (IsValid(Light))
                {
                    return Light;
                }
            }
        }
        return nullptr;
    };

    auto FindFirstSkyLight = [&]() -> ASkyLight*
    {
        for (TActorIterator<ASkyLight> It(World); It; ++It)
        {
            if (ASkyLight* Sky = *It)
            {
                if (IsValid(Sky))
                {
                    return Sky;
                }
            }
        }
        return nullptr;
    };

    if (LowerSub == TEXT("set_time_of_day"))
    {
        double Hour = 0.0;
        const bool bHasHour = Payload->TryGetNumberField(TEXT("hour"), Hour);
        if (!bHasHour)
        {
            SendResult(false, TEXT("Missing hour parameter"), TEXT("INVALID_ARGUMENT"), nullptr);
            return true;
        }

        ADirectionalLight* SunLight = FindFirstDirectionalLight();
        if (!SunLight)
        {
            SendResult(false, TEXT("No directional light found"), TEXT("SUN_NOT_FOUND"), nullptr);
            return true;
        }

        const float ClampedHour = FMath::Clamp(static_cast<float>(Hour), 0.0f, 24.0f);
        const float SolarPitch = (ClampedHour / 24.0f) * 360.0f - 90.0f;

        SunLight->Modify();
        FRotator NewRotation = SunLight->GetActorRotation();
        NewRotation.Pitch = SolarPitch;
        SunLight->SetActorRotation(NewRotation);

        if (UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
        {
            LightComp->MarkRenderStateDirty();
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("hour"), ClampedHour);
        Result->SetNumberField(TEXT("pitch"), SolarPitch);
        Result->SetStringField(TEXT("actor"), SunLight->GetPathName());
        SendResult(true, TEXT("Time of day updated"), FString(), Result);
        return true;
    }

    if (LowerSub == TEXT("set_sun_intensity"))
    {
        double Intensity = 0.0;
        if (!Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            SendResult(false, TEXT("Missing intensity parameter"), TEXT("INVALID_ARGUMENT"), nullptr);
            return true;
        }

        ADirectionalLight* SunLight = FindFirstDirectionalLight();
        if (!SunLight)
        {
            SendResult(false, TEXT("No directional light found"), TEXT("SUN_NOT_FOUND"), nullptr);
            return true;
        }

        if (UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
        {
            LightComp->SetIntensity(static_cast<float>(Intensity));
            LightComp->MarkRenderStateDirty();
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("intensity"), Intensity);
        Result->SetStringField(TEXT("actor"), SunLight->GetPathName());
        SendResult(true, TEXT("Sun intensity updated"), FString(), Result);
        return true;
    }

    if (LowerSub == TEXT("set_skylight_intensity"))
    {
        double Intensity = 0.0;
        if (!Payload->TryGetNumberField(TEXT("intensity"), Intensity))
        {
            SendResult(false, TEXT("Missing intensity parameter"), TEXT("INVALID_ARGUMENT"), nullptr);
            return true;
        }

        ASkyLight* SkyActor = FindFirstSkyLight();
        if (!SkyActor)
        {
            SendResult(false, TEXT("No skylight found"), TEXT("SKYLIGHT_NOT_FOUND"), nullptr);
            return true;
        }

        if (USkyLightComponent* SkyComp = SkyActor->GetLightComponent())
        {
            SkyComp->SetIntensity(static_cast<float>(Intensity));
            SkyComp->MarkRenderStateDirty();
            SkyActor->MarkComponentsRenderStateDirty();
        }

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("intensity"), Intensity);
        Result->SetStringField(TEXT("actor"), SkyActor->GetPathName());
        SendResult(true, TEXT("Skylight intensity updated"), FString(), Result);
        return true;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("action"), LowerSub);
    SendResult(false, TEXT("Unsupported environment control action"), TEXT("UNSUPPORTED_ACTION"), Result);
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Environment control requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSystemControlAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!Payload.IsValid())
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("System control requires valid payload"), nullptr, TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction;
    if (!Payload->TryGetStringField(TEXT("action"), SubAction))
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("System control requires action parameter"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString LowerSub = SubAction.ToLower();
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // Profile commands
    if (LowerSub == TEXT("profile"))
    {
        FString ProfileType;
        bool bEnabled = true;
        Payload->TryGetStringField(TEXT("profileType"), ProfileType);
        Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

        FString Command;
        if (ProfileType.ToLower() == TEXT("cpu"))
        {
            Command = bEnabled ? TEXT("stat cpu") : TEXT("stat cpu");
        }
        else if (ProfileType.ToLower() == TEXT("gpu"))
        {
            Command = bEnabled ? TEXT("stat gpu") : TEXT("stat gpu");
        }
        else if (ProfileType.ToLower() == TEXT("memory"))
        {
            Command = bEnabled ? TEXT("stat memory") : TEXT("stat memory");
        }
        else if (ProfileType.ToLower() == TEXT("fps"))
        {
            Command = bEnabled ? TEXT("stat fps") : TEXT("stat fps");
        }

        if (!Command.IsEmpty())
        {
            GEngine->Exec(nullptr, *Command);
            Result->SetStringField(TEXT("command"), Command);
            Result->SetBoolField(TEXT("enabled"), bEnabled);
            SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Executed profile command: %s"), *Command), Result, FString());
            return true;
        }
    }

    // Show FPS
    if (LowerSub == TEXT("show_fps"))
    {
        bool bEnabled = true;
        Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

        FString Command = bEnabled ? TEXT("stat fps") : TEXT("stat fps");
        GEngine->Exec(nullptr, *Command);
        Result->SetStringField(TEXT("command"), Command);
        Result->SetBoolField(TEXT("enabled"), bEnabled);
        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("FPS display %s"), bEnabled ? TEXT("enabled") : TEXT("disabled")), Result, FString());
        return true;
    }

    // Set quality
    if (LowerSub == TEXT("set_quality"))
    {
        FString Category;
        int32 Level = 1;
        Payload->TryGetStringField(TEXT("category"), Category);
        Payload->TryGetNumberField(TEXT("level"), Level);

        if (!Category.IsEmpty())
        {
            FString Command = FString::Printf(TEXT("sg.%s %d"), *Category, Level);
            GEngine->Exec(nullptr, *Command);
            Result->SetStringField(TEXT("command"), Command);
            Result->SetStringField(TEXT("category"), Category);
            Result->SetNumberField(TEXT("level"), Level);
            SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Set quality %s to %d"), *Category, Level), Result, FString());
            return true;
        }
    }

    // Screenshot
    if (LowerSub == TEXT("screenshot"))
    {
        FString Filename = TEXT("screenshot");
        Payload->TryGetStringField(TEXT("filename"), Filename);

        FString Command = FString::Printf(TEXT("screenshot %s"), *Filename);
        GEngine->Exec(nullptr, *Command);
        Result->SetStringField(TEXT("command"), Command);
        Result->SetStringField(TEXT("filename"), Filename);
        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Screenshot captured: %s"), *Filename), Result, FString());
        return true;
    }

    // Engine quit (disabled for safety)
    if (LowerSub == TEXT("engine_quit"))
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Engine quit command is disabled for safety"), nullptr, TEXT("NOT_ALLOWED"));
        return true;
    }

    SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Unknown system control action: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleConsoleCommandAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!Payload.IsValid())
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Console command requires valid payload"), nullptr, TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString Command;
    if (!Payload->TryGetStringField(TEXT("command"), Command))
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Console command requires command parameter"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Block dangerous commands
    FString LowerCommand = Command.ToLower();
    TArray<FString> BlockedCommands = { TEXT("quit"), TEXT("exit"), TEXT("crash"), TEXT("shutdown"), TEXT("restart") };
    
    for (const FString& Blocked : BlockedCommands)
    {
        if (LowerCommand.Contains(Blocked))
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Command '%s' is blocked for safety"), *Command), nullptr, TEXT("COMMAND_BLOCKED"));
            return true;
        }
    }

    // Execute the command
    try
    {
        GEngine->Exec(nullptr, *Command);
        
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("command"), Command);
        Result->SetBoolField(TEXT("executed"), true);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Executed console command: %s"), *Command), Result, FString());
        return true;
    }
    catch (...)
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Failed to execute command: %s"), *Command), nullptr, TEXT("EXECUTION_FAILED"));
        return true;
    }
}

bool UMcpAutomationBridgeSubsystem::HandleInspectAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!Payload.IsValid())
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Inspect action requires valid payload"), nullptr, TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString SubAction;
    if (!Payload->TryGetStringField(TEXT("action"), SubAction))
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Inspect action requires action parameter"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString LowerSub = SubAction.ToLower();
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

    // Inspect object
    if (LowerSub == TEXT("inspect_object"))
    {
        FString ObjectPath;
        if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath))
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("inspect_object requires objectPath parameter"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UObject* TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
        if (!TargetObject)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Object not found: %s"), *ObjectPath), nullptr, TEXT("OBJECT_NOT_FOUND"));
            return true;
        }

        Result->SetStringField(TEXT("objectPath"), ObjectPath);
        Result->SetStringField(TEXT("objectName"), TargetObject->GetName());
        Result->SetStringField(TEXT("objectClass"), TargetObject->GetClass()->GetName());
        Result->SetStringField(TEXT("objectType"), TargetObject->GetClass()->GetFName().ToString());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Inspected object: %s"), *ObjectPath), Result, FString());
        return true;
    }

    // Get property
    if (LowerSub == TEXT("get_property"))
    {
        FString ObjectPath;
        FString PropertyName;
        
        if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) || 
            !Payload->TryGetStringField(TEXT("propertyName"), PropertyName))
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("get_property requires objectPath and propertyName parameters"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UObject* TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
        if (!TargetObject)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Object not found: %s"), *ObjectPath), nullptr, TEXT("OBJECT_NOT_FOUND"));
            return true;
        }

        UClass* ObjectClass = TargetObject->GetClass();
        FProperty* Property = ObjectClass->FindPropertyByName(*PropertyName);
        
        if (!Property)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Property not found: %s"), *PropertyName), nullptr, TEXT("PROPERTY_NOT_FOUND"));
            return true;
        }

        Result->SetStringField(TEXT("objectPath"), ObjectPath);
        Result->SetStringField(TEXT("propertyName"), PropertyName);
        Result->SetStringField(TEXT("propertyType"), Property->GetClass()->GetName());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("Retrieved property: %s.%s"), *ObjectPath, *PropertyName), Result, FString());
        return true;
    }

    // Set property (simplified implementation)
    if (LowerSub == TEXT("set_property"))
    {
        FString ObjectPath;
        FString PropertyName;
        
        if (!Payload->TryGetStringField(TEXT("objectPath"), ObjectPath) || 
            !Payload->TryGetStringField(TEXT("propertyName"), PropertyName))
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("set_property requires objectPath and propertyName parameters"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UObject* TargetObject = FindObject<UObject>(nullptr, *ObjectPath);
        if (!TargetObject)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Object not found: %s"), *ObjectPath), nullptr, TEXT("OBJECT_NOT_FOUND"));
            return true;
        }

        // Get the property value from payload
        FString PropertyValue;
        if (!Payload->TryGetStringField(TEXT("value"), PropertyValue))
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("set_property requires 'value' field"), nullptr, TEXT("INVALID_ARGUMENT"));
            return true;
        }

        // Find the property using Unreal's reflection system
        FProperty* FoundProperty = TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
        if (!FoundProperty)
        {
            SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Property '%s' not found on object '%s'"), *PropertyName, *ObjectPath), nullptr, TEXT("PROPERTY_NOT_FOUND"));
            return true;
        }

        // Set the property value based on type
        bool bSuccess = false;
        FString ErrorMessage;

        if (FStrProperty* StrProp = CastField<FStrProperty>(FoundProperty))
        {
            void* PropAddr = StrProp->ContainerPtrToValuePtr<void>(TargetObject);
            StrProp->SetPropertyValue(PropAddr, PropertyValue);
            bSuccess = true;
        }
        else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(FoundProperty))
        {
            void* PropAddr = FloatProp->ContainerPtrToValuePtr<void>(TargetObject);
            float Value = FCString::Atof(*PropertyValue);
            FloatProp->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(FoundProperty))
        {
            void* PropAddr = DoubleProp->ContainerPtrToValuePtr<void>(TargetObject);
            double Value = FCString::Atod(*PropertyValue);
            DoubleProp->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FIntProperty* IntProp = CastField<FIntProperty>(FoundProperty))
        {
            void* PropAddr = IntProp->ContainerPtrToValuePtr<void>(TargetObject);
            int32 Value = FCString::Atoi(*PropertyValue);
            IntProp->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FInt64Property* Int64Prop = CastField<FInt64Property>(FoundProperty))
        {
            void* PropAddr = Int64Prop->ContainerPtrToValuePtr<void>(TargetObject);
            int64 Value = FCString::Atoi64(*PropertyValue);
            Int64Prop->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(FoundProperty))
        {
            void* PropAddr = BoolProp->ContainerPtrToValuePtr<void>(TargetObject);
            bool Value = PropertyValue.ToBool();
            BoolProp->SetPropertyValue(PropAddr, Value);
            bSuccess = true;
        }
        else if (FObjectProperty* ObjProp = CastField<FObjectProperty>(FoundProperty))
        {
            // Try to find the object by path
            UObject* ObjValue = FindObject<UObject>(nullptr, *PropertyValue);
            if (ObjValue || PropertyValue.IsEmpty())
            {
                void* PropAddr = ObjProp->ContainerPtrToValuePtr<void>(TargetObject);
                ObjProp->SetPropertyValue(PropAddr, ObjValue);
                bSuccess = true;
            }
            else
            {
                ErrorMessage = FString::Printf(TEXT("Object property requires valid object path, got: %s"), *PropertyValue);
            }
        }
        else
        {
            ErrorMessage = FString::Printf(TEXT("Property type '%s' not supported for setting"), *FoundProperty->GetClass()->GetName());
        }

        if (bSuccess)
        {
            Result->SetStringField(TEXT("objectPath"), ObjectPath);
            Result->SetStringField(TEXT("propertyName"), PropertyName);
            Result->SetStringField(TEXT("value"), PropertyValue);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property set successfully"), Result, FString());
        }
        else
        {
            Result->SetStringField(TEXT("objectPath"), ObjectPath);
            Result->SetStringField(TEXT("propertyName"), PropertyName);
            Result->SetStringField(TEXT("error"), ErrorMessage);
            SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Failed to set property"), Result, TEXT("PROPERTY_SET_FAILED"));
        }
        return true;
    }

    SendAutomationResponse(RequestingSocket, RequestId, false, FString::Printf(TEXT("Unknown inspect action: %s"), *SubAction), nullptr, TEXT("UNKNOWN_ACTION"));
    return true;
}
