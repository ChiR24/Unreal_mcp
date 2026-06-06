#include "McpAutomationBridge_EnvironmentHandlersShared.h"

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
                TSharedPtr<FJsonObject> ControllerObj = McpDescribeRuntimeActor(PlayerController, ComponentNames, PropertyNames);
                Report->SetObjectField(TEXT("playerController"), ControllerObj);

                if (APawn *Pawn = PlayerController->GetPawn())
                {
                    Report->SetObjectField(TEXT("pawn"), McpDescribeRuntimeActor(Pawn, ComponentNames, PropertyNames));
                }

                if (AActor *ViewTarget = PlayerController->GetViewTarget())
                {
                    Report->SetObjectField(TEXT("viewTarget"), McpDescribeRuntimeActor(ViewTarget, ComponentNames, PropertyNames));
                }

                if (APlayerCameraManager *CameraManager = PlayerController->PlayerCameraManager)
                {
                    TSharedPtr<FJsonObject> CameraManagerObj = McpDescribeRuntimeActor(CameraManager, ComponentNames, PropertyNames);
                    CameraManagerObj->SetObjectField(TEXT("cameraLocation"), McpMakeVectorObject(CameraManager->GetCameraLocation()));
                    CameraManagerObj->SetObjectField(TEXT("cameraRotation"), McpMakeRotatorObject(CameraManager->GetCameraRotation()));
                    Report->SetObjectField(TEXT("playerCameraManager"), CameraManagerObj);
                }
            }

            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                   TEXT("Runtime inspection report generated"), Report, FString());
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
