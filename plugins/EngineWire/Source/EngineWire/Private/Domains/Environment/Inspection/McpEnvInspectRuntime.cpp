#include "Domains/Environment/McpEnvShared.h"

namespace McpEnvironmentHandlers {

bool HandleInspectRuntimeReportAction(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &LowerSubAction, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
        if (LowerSubAction.Equals(TEXT("runtime_report")) || LowerSubAction.Equals(TEXT("pie_report")))
        {
            UWorld *World = McpGetRuntimeInspectionWorld();
            if (!World)
            {
                Bridge.SendAutomationError(RequestingSocket, RequestId,
                                    TEXT("No editor, PIE, or game world available for runtime inspection"),
                                    TEXT("WORLD_NOT_FOUND"));
                return true;
            }

            FString Filter;
            Payload->TryGetStringField(TEXT("filter"), Filter);
            FString ActorName;
            Payload->TryGetStringField(TEXT("actorName"), ActorName);
            if (ActorName.IsEmpty())
            {
                Payload->TryGetStringField(TEXT("name"), ActorName);
            }

            TArray<FString> ComponentNames;
            FString ComponentName;
            if (Payload->TryGetStringField(TEXT("componentName"), ComponentName) && !ComponentName.IsEmpty())
            {
                ComponentNames.Add(ComponentName);
            }
            const TArray<TSharedPtr<FJsonValue>> *ComponentNamesArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("componentNames"), ComponentNamesArray) && ComponentNamesArray)
            {
                for (const TSharedPtr<FJsonValue> &Value : *ComponentNamesArray)
                {
                    if (Value.IsValid() && Value->Type == EJson::String)
                    {
                        ComponentNames.Add(Value->AsString());
                    }
                }
            }

            TArray<FString> PropertyNames;
            FString PropertyName;
            if (Payload->TryGetStringField(TEXT("propertyName"), PropertyName) && !PropertyName.IsEmpty())
            {
                PropertyNames.Add(PropertyName);
            }
            else if (Payload->TryGetStringField(TEXT("propertyPath"), PropertyName) && !PropertyName.IsEmpty())
            {
                PropertyNames.Add(PropertyName);
            }
            const TArray<TSharedPtr<FJsonValue>> *PropertyNamesArray = nullptr;
            if (Payload->TryGetArrayField(TEXT("propertyNames"), PropertyNamesArray) && PropertyNamesArray)
            {
                for (const TSharedPtr<FJsonValue> &Value : *PropertyNamesArray)
                {
                    if (Value.IsValid() && Value->Type == EJson::String)
                    {
                        PropertyNames.Add(Value->AsString());
                    }
                }
            }

            TSharedPtr<FJsonObject> Report = McpHandlerUtils::CreateResultObject();
            Report->SetBoolField(TEXT("success"), true);
            Report->SetStringField(TEXT("worldName"), World->GetName());
            Report->SetStringField(TEXT("worldType"), McpGetWorldTypeName(World));
            Report->SetStringField(TEXT("worldPath"), World->GetPathName());
            const bool bIsPIE = World->WorldType == EWorldType::PIE;
            Report->SetBoolField(TEXT("isPIE"), bIsPIE);

            // A bare report described the first 25 actors in level order (world settings, brushes, lights) with every
            // component: kilobytes to learn the session and its pawn. Actors are listed only when something selects them.
            const bool bListActors = !ActorName.IsEmpty() || !Filter.IsEmpty() || ComponentNames.Num() > 0 || PropertyNames.Num() > 0;
            constexpr int32 MaxReportedActors = 25;
            TArray<TSharedPtr<FJsonValue>> ActorsArray;
            TSet<FString> FoundProperties;
            int32 TotalActorCount = 0;
            int32 OmittedActorCount = 0;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor *Actor = *It;
                if (!Actor)
                {
                    continue;
                }
                ++TotalActorCount;
                if (!bIsPIE || !bListActors)
                {
                    continue;
                }

                const FString Label = Actor->GetActorLabel();
                const FString Name = Actor->GetName();
                const bool bMatchesActor = ActorName.IsEmpty() ||
                    Label.Equals(ActorName, ESearchCase::IgnoreCase) ||
                    Name.Equals(ActorName, ESearchCase::IgnoreCase) ||
                    Actor->GetPathName().Equals(ActorName, ESearchCase::IgnoreCase);
                const bool bMatchesFilter = Filter.IsEmpty() ||
                    Label.Contains(Filter) ||
                    Name.Contains(Filter) ||
                    Actor->GetClass()->GetName().Contains(Filter) ||
                    Actor->GetPathName().Contains(Filter);
                if (bMatchesActor && bMatchesFilter)
                {
                    if (ActorsArray.Num() < MaxReportedActors)
                    {
                        ActorsArray.Add(MakeShared<FJsonValueObject>(McpDescribeRuntimeActor(Actor, ComponentNames, PropertyNames, &FoundProperties)));
                    }
                    else
                    {
                        ++OmittedActorCount;
                    }
                }
            }
            if (bListActors)
            {
                Report->SetArrayField(TEXT("actors"), ActorsArray);
                Report->SetNumberField(TEXT("count"), ActorsArray.Num());
                // Once for the report: listed on every actor, a broad filter repeated each name for each unrelated actor.
                TArray<TSharedPtr<FJsonValue>> NotFound;
                for (const FString &Name : PropertyNames)
                {
                    if (!Name.IsEmpty() && ActorsArray.Num() > 0 && !FoundProperties.Contains(Name))
                    {
                        NotFound.Add(MakeShared<FJsonValueString>(Name));
                    }
                }
                if (NotFound.Num() > 0)
                {
                    Report->SetArrayField(TEXT("propertiesNotFound"), NotFound);
                }
            }
            Report->SetNumberField(TEXT("totalActorCount"), TotalActorCount);
            if (!bIsPIE || OmittedActorCount > 0)
            {
                TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
                if (!bIsPIE)
                {
                    Details->SetStringField(TEXT("reason"),
                        TEXT("No PIE session is active; start PIE for runtime state, or use inspect_object for editor-world actors."));
                }
                if (OmittedActorCount > 0)
                {
                    Details->SetBoolField(TEXT("truncated"), true);
                    Details->SetNumberField(TEXT("actorsOmitted"), OmittedActorCount);
                }
                Report->SetObjectField(TEXT("details"), Details);
            }

            APlayerController *PlayerController = World->GetFirstPlayerController();
            if (PlayerController)
            {
                // Pins these as canonical string identities; the record now matches (dogfood #139).
                Report->SetStringField(TEXT("playerController"), PlayerController->GetPathName());

                if (APawn *Pawn = PlayerController->GetPawn())
                {
                    Report->SetStringField(TEXT("pawn"), Pawn->GetPathName());
                }

                if (AActor *ViewTarget = PlayerController->GetViewTarget())
                {
                    Report->SetStringField(TEXT("viewTarget"), ViewTarget->GetPathName());
                }

                if (APlayerCameraManager *CameraManager = PlayerController->PlayerCameraManager)
                {
                    // The contract declares playerCameraManager as an object (dogfood #139): describe the manager
                    // as a runtime actor with its camera pose instead of a bare path string.
                    TSharedPtr<FJsonObject> CameraJson = MakeShared<FJsonObject>();
                    CameraJson->SetStringField(TEXT("name"), CameraManager->GetName());
                    CameraJson->SetStringField(TEXT("path"), CameraManager->GetPathName());
                    CameraJson->SetStringField(TEXT("class"), CameraManager->GetClass()->GetName());
                    CameraJson->SetObjectField(TEXT("cameraLocation"), McpHandlerUtils::VectorToJson(CameraManager->GetCameraLocation()));
                    CameraJson->SetObjectField(TEXT("cameraRotation"), McpHandlerUtils::RotatorToJson(CameraManager->GetCameraRotation()));
                    CameraJson->SetNumberField(TEXT("fov"), CameraManager->GetFOVAngle());
                    Report->SetObjectField(TEXT("playerCameraManager"), CameraJson);
                }
            }

            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   bListActors ? TEXT("Runtime inspection report generated")
                                               : TEXT("Runtime report of the session; pass filter (part of an actor's name, label or class) or actorName to describe actors"),
                                   Report, FString());
            return true;
        }
    return false;
}

} // namespace McpEnvironmentHandlers
