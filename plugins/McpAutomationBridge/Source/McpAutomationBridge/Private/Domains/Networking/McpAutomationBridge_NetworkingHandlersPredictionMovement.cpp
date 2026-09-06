#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleConfigureClientPrediction(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    bool bEnablePrediction = GetJsonBoolField(Payload, TEXT("enablePrediction"), true);
    double PredictionThreshold = GetJsonNumberField(Payload, TEXT("predictionThreshold"), 0.1);

    if (BlueprintPath.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    ACharacter* CharacterCDO = Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CharacterCDO && CharacterCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* CMC = CharacterCDO->GetCharacterMovement();
        if (bEnablePrediction)
        {
            CMC->bNetworkAlwaysReplicateTransformUpdateTimestamp = true;
            CMC->NetworkSimulatedSmoothLocationTime = static_cast<float>(PredictionThreshold);
        }
        else
        {
            CMC->bNetworkAlwaysReplicateTransformUpdateTimestamp = false;
        }
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("enablePrediction"), bEnablePrediction);
    ResultJson->SetNumberField(TEXT("predictionThreshold"), PredictionThreshold);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Client prediction %s"), bEnablePrediction ? TEXT("enabled") : TEXT("disabled")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Client prediction configured"), ResultJson);
    return true;
}

bool HandleConfigureServerCorrection(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    double CorrectionThreshold = GetJsonNumberField(Payload, TEXT("correctionThreshold"), 1.0);
    double SmoothingRate = GetJsonNumberField(Payload, TEXT("smoothingRate"), 0.5);

    if (BlueprintPath.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    ACharacter* CharacterCDO = Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CharacterCDO && CharacterCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* CMC = CharacterCDO->GetCharacterMovement();
        CMC->NetworkSimulatedSmoothLocationTime = static_cast<float>(SmoothingRate);
        CMC->NetworkSimulatedSmoothRotationTime = static_cast<float>(SmoothingRate);
        CMC->ListenServerNetworkSimulatedSmoothLocationTime = static_cast<float>(SmoothingRate);
        CMC->ListenServerNetworkSimulatedSmoothRotationTime = static_cast<float>(SmoothingRate);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetNumberField(TEXT("correctionThreshold"), CorrectionThreshold);
    ResultJson->SetNumberField(TEXT("smoothingRate"), SmoothingRate);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Server correction configured (threshold=%.2f, smoothing=%.2f)"), CorrectionThreshold, SmoothingRate));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Server correction configured"), ResultJson);
    return true;
}

bool HandleConfigureMovementPrediction(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    double NetworkMaxSmoothUpdateDistance = GetJsonNumberField(Payload, TEXT("networkMaxSmoothUpdateDistance"), 256.0);
    double NetworkNoSmoothUpdateDistance = GetJsonNumberField(Payload, TEXT("networkNoSmoothUpdateDistance"), 384.0);

    if (BlueprintPath.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    ACharacter* CharacterCDO = Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CharacterCDO && CharacterCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* CMC = CharacterCDO->GetCharacterMovement();
        CMC->NetworkMaxSmoothUpdateDistance = static_cast<float>(NetworkMaxSmoothUpdateDistance);
        CMC->NetworkNoSmoothUpdateDistance = static_cast<float>(NetworkNoSmoothUpdateDistance);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("message"), TEXT("Movement prediction configured"));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Movement prediction configured"), ResultJson);
    return true;
}
}
