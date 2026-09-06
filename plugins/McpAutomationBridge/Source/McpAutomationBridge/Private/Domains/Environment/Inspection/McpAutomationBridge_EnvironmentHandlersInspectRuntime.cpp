#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

bool HandleInspectRuntimeReportAction(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
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
            Report->SetBoolField(TEXT("isPIE"), World->WorldType == EWorldType::PIE);

            TArray<TSharedPtr<FJsonValue>> ActorsArray;
            int32 TotalActorCount = 0;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor *Actor = *It;
                if (!Actor)
                {
                    continue;
                }
                ++TotalActorCount;

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
                    ActorsArray.Add(MakeShared<FJsonValueObject>(McpDescribeRuntimeActor(Actor, ComponentNames, PropertyNames)));
                }
            }
            Report->SetArrayField(TEXT("actors"), ActorsArray);
            Report->SetNumberField(TEXT("count"), ActorsArray.Num());
            Report->SetNumberField(TEXT("totalActorCount"), TotalActorCount);

            APlayerController *PlayerController = World->GetFirstPlayerController();
            if (PlayerController)
            {
                // BB-036 pins these as canonical string identities; the record now matches (dogfood #139).
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
                    CameraJson->SetObjectField(TEXT("cameraLocation"), McpMakeVectorObject(CameraManager->GetCameraLocation()));
                    CameraJson->SetObjectField(TEXT("cameraRotation"), McpMakeRotatorObject(CameraManager->GetCameraRotation()));
                    CameraJson->SetNumberField(TEXT("fov"), CameraManager->GetFOVAngle());
                    Report->SetObjectField(TEXT("playerCameraManager"), CameraJson);
                    Report->SetObjectField(TEXT("cameraLocation"), McpMakeVectorObject(CameraManager->GetCameraLocation()));
                    Report->SetObjectField(TEXT("cameraRotation"), McpMakeRotatorObject(CameraManager->GetCameraRotation()));
                }
            }

            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Runtime inspection report generated"), Report, FString());
            return true;
        }
    return false;
}

} // namespace McpEnvironmentHandlers
#endif
