#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_PropertyHandlersActorAccess.h"

#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"
#include "McpPropertyReflection.h"

#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node.h"
#endif

namespace McpPropertyActorAccess
{
void AddObjectVerification(TSharedPtr<FJsonObject>& Result, UObject* Object)
{
#if WITH_EDITOR
    if (AActor* AsActor = Cast<AActor>(Object))
    {
        McpHandlerUtils::AddVerification(Result, AsActor);
    }
    else
    {
        McpHandlerUtils::AddVerification(Result, Object);
    }
#endif
}

bool TryHandleSetActorProperty(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& PropertyName,
    const TSharedPtr<FJsonObject>& Payload,
    const TSharedPtr<FJsonValue>& ValueField,
    AActor* Actor,
    bool bIsClassDefaultObject,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (bIsClassDefaultObject &&
        (PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase) ||
         PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase) ||
         PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
         PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase)))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Cannot modify runtime transform on a Blueprint CDO. Edit defaults on the root component or SCS template instead."),
            TEXT("CDO_TRANSFORM"));
        return true;
    }

    if (!bIsClassDefaultObject &&
        PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase))
    {
        FVector NewLoc = FVector::ZeroVector;
        if (ValueField->Type == EJson::Object)
        {
            McpPropertyReflection::JsonToVector(ValueField->AsObject(), NewLoc);
        }
        else if (ValueField->Type == EJson::Array)
        {
            McpPropertyReflection::JsonArrayToVector(ValueField->AsArray(), NewLoc);
        }

        Actor->SetActorLocation(NewLoc);

        TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
        ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
        ResultPayload->SetBoolField(TEXT("saved"), true);
        ResultPayload->SetObjectField(TEXT("value"), McpPropertyReflection::VectorToJson(NewLoc));
        AddObjectVerification(ResultPayload, Actor);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor location updated."), ResultPayload);
        return true;
    }

    if (PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase))
    {
        FRotator NewRot = FRotator::ZeroRotator;
        if (ValueField->Type == EJson::Object)
        {
            McpPropertyReflection::JsonToRotator(ValueField->AsObject(), NewRot);
        }
        else if (ValueField->Type == EJson::Array)
        {
            McpPropertyReflection::JsonArrayToRotator(ValueField->AsArray(), NewRot);
        }

        Actor->SetActorRotation(NewRot);

        TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
        ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
        ResultPayload->SetBoolField(TEXT("saved"), true);
        ResultPayload->SetObjectField(TEXT("value"), McpPropertyReflection::RotatorToJson(NewRot));
        AddObjectVerification(ResultPayload, Actor);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor rotation updated."), ResultPayload);
        return true;
    }

    if (PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))
    {
        FVector NewScale = FVector::OneVector;
        if (ValueField->Type == EJson::Object)
        {
            McpPropertyReflection::JsonToVector(ValueField->AsObject(), NewScale);
        }
        else if (ValueField->Type == EJson::Array)
        {
            McpPropertyReflection::JsonArrayToVector(ValueField->AsArray(), NewScale);
        }

        Actor->SetActorScale3D(NewScale);

        TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
        ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
        ResultPayload->SetBoolField(TEXT("saved"), true);
        ResultPayload->SetObjectField(TEXT("value"), McpPropertyReflection::VectorToJson(NewScale));
        AddObjectVerification(ResultPayload, Actor);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor scale updated."), ResultPayload);
        return true;
    }

    if (!bIsClassDefaultObject && PropertyName.Equals(TEXT("bHidden"), ESearchCase::IgnoreCase))
    {
        bool bHidden = McpHandlerUtils::GetOptionalBool(Payload, TEXT("value"), false);
        if (ValueField->Type == EJson::Boolean)
        {
            bHidden = ValueField->AsBool();
        }
        else if (ValueField->Type == EJson::Number)
        {
            bHidden = ValueField->AsNumber() != 0;
        }

        Actor->SetActorHiddenInGame(bHidden);

        TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
        ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
        ResultPayload->SetBoolField(TEXT("saved"), true);
        ResultPayload->SetBoolField(TEXT("value"), bHidden);
        AddObjectVerification(ResultPayload, Actor);
        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Actor visibility updated."), ResultPayload);
        return true;
    }

    return false;
}

void RefreshK2NodeTitleCacheIfNeeded(UObject* RootObject)
{
#if WITH_EDITOR
    if (UK2Node* K2Node = Cast<UK2Node>(RootObject))
    {
        static const TSet<FString> RefreshableTitleNodeClassNames = {
            TEXT("K2Node_EnhancedInputAction"),
        };
        if (RefreshableTitleNodeClassNames.Contains(K2Node->GetClass()->GetName()))
        {
            K2Node->ReconstructNode();
            if (UEdGraph* Graph = K2Node->GetGraph())
            {
                if (const UEdGraphSchema* Schema = Graph->GetSchema())
                {
                    Schema->ForceVisualizationCacheClear();
                }
                Graph->NotifyGraphChanged();
            }
        }
    }
#endif
}
}
