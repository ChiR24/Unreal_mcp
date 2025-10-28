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
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Environment control requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
