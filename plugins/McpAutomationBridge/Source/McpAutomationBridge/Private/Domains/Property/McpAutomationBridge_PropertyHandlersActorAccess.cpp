#include "Domains/Property/McpAutomationBridge_PropertyHandlersActorAccess.h"

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/BridgeHelpers/Actors/McpAutomationBridgeHelpersActorMove.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node.h"

#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"

namespace McpPropertyActorAccess
{
namespace
{
// A level actor reaches disk only when its level is saved, never here; these
// shortcuts used to answer saved:true regardless.
// The reply every actor shortcut sends: the property, the value that landed,
// and saved:false - a level actor reaches disk only with its level.
bool SendActorWrite(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& PropertyName, AActor* Actor,
                    const TSharedPtr<FJsonValue>& Value, const TCHAR* Message)
{
    TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
    ResultPayload->SetStringField(TEXT("propertyName"), PropertyName);
    ResultPayload->SetBoolField(TEXT("saved"), false);
    const UWorld* World = Actor ? Actor->GetWorld() : nullptr;
    ResultPayload->SetStringField(TEXT("saveSkippedReason"), World && World->IsPlayInEditor()
        ? TEXT("a running-game actor has nothing to save; the change lasts until PIE stops")
        : TEXT("level content is saved with its level"));
    ResultPayload->SetField(TEXT("value"), Value);
    McpHandlerUtils::AddVerification(ResultPayload, Actor);
    Subsystem.SendAutomationResponse(Socket, RequestId, true, Message, ResultPayload);
    return true;
}
}

bool IsActorTransformProperty(const FString& PropertyName)
{
    return PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase);
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
    if (bIsClassDefaultObject && IsActorTransformProperty(PropertyName))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId,
            TEXT("Cannot modify runtime transform on a Blueprint CDO. Edit defaults on the root component or SCS template instead."),
            TEXT("CDO_TRANSFORM"));
        return true;
    }

    if (!bIsClassDefaultObject &&
        PropertyName.Equals(TEXT("ActorLocation"), ESearchCase::IgnoreCase))
    {
        const FVector NewLoc = ReadJsonVector(ValueField, FVector::ZeroVector);

        Actor->SetActorLocation(NewLoc);
        McpFinishEditorMove(Actor);

        return SendActorWrite(Subsystem, RequestId, RequestingSocket, PropertyName, Actor,
                              MakeShared<FJsonValueObject>(McpHandlerUtils::VectorToJson(NewLoc)), TEXT("Actor location updated."));
    }

    if (PropertyName.Equals(TEXT("ActorRotation"), ESearchCase::IgnoreCase))
    {
        const FRotator NewRot = ReadJsonRotator(ValueField, FRotator::ZeroRotator);

        Actor->SetActorRotation(NewRot);
        McpFinishEditorMove(Actor);

        return SendActorWrite(Subsystem, RequestId, RequestingSocket, PropertyName, Actor,
                              MakeShared<FJsonValueObject>(McpHandlerUtils::RotatorToJson(NewRot)), TEXT("Actor rotation updated."));
    }

    if (PropertyName.Equals(TEXT("ActorScale"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("ActorScale3D"), ESearchCase::IgnoreCase))
    {
        const FVector NewScale = ReadJsonVector(ValueField, FVector::OneVector);

        Actor->SetActorScale3D(NewScale);
        McpFinishEditorMove(Actor);

        return SendActorWrite(Subsystem, RequestId, RequestingSocket, PropertyName, Actor,
                              MakeShared<FJsonValueObject>(McpHandlerUtils::VectorToJson(NewScale)), TEXT("Actor scale updated."));
    }

    if (!bIsClassDefaultObject && PropertyName.Equals(TEXT("bHidden"), ESearchCase::IgnoreCase))
    {
        bool bHidden = GetJsonBoolField(Payload, TEXT("value"), false);
        if (ValueField->Type == EJson::Boolean)
        {
            bHidden = ValueField->AsBool();
        }
        else if (ValueField->Type == EJson::Number)
        {
            bHidden = ValueField->AsNumber() != 0;
        }

        Actor->SetActorHiddenInGame(bHidden);

        return SendActorWrite(Subsystem, RequestId, RequestingSocket, PropertyName, Actor,
                              MakeShared<FJsonValueBoolean>(bHidden), TEXT("Actor visibility updated."));
    }

    return false;
}

void RefreshK2NodeTitleCacheIfNeeded(UObject* RootObject)
{
    if (UK2Node* K2Node = Cast<UK2Node>(RootObject))
    {
        // Its cached title names the input action, so it goes stale on edit.
        if (K2Node->GetClass()->GetName() == TEXT("K2Node_EnhancedInputAction"))
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
}

bool RefreshMaterialHostAfterEdit(UObject* Edited)
{
    // An expression's own PostEditChange never reaches its material, and the parameter lists instances read
    // come from the material's cached expression data, which only the material's PostEditChange rebuilds (and
    // recompiles): a renamed parameter stayed invisible to every instance until some other call did this.
    UObject* Host = Edited ? Edited->GetTypedOuter<UMaterial>() : nullptr;
    if (!Host && Edited)
    {
        Host = Edited->GetTypedOuter<UMaterialFunction>();
    }
    if (!Host)
    {
        return false;
    }
    Host->PreEditChange(nullptr);
    Host->PostEditChange();
    return true;
}
}
