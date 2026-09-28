#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
namespace
{
// Every prediction setting lives on a Character's CharacterMovementComponent. Any other Blueprint used
// to be saved and answered with success while nothing changed, so it is refused here instead.
UCharacterMovementComponent* LoadCharacterMovementOrReply(FNetworkingActionContext& Context, UBlueprint*& OutBlueprint)
{
    OutBlueprint = LoadBlueprintOrReply(Context, GetJsonStringField(Context.Payload, TEXT("blueprintPath")));
    if (!OutBlueprint)
    {
        return nullptr;
    }
    ACharacter* CharacterCDO = OutBlueprint->GeneratedClass ? Cast<ACharacter>(OutBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    UCharacterMovementComponent* Movement = CharacterCDO ? CharacterCDO->GetCharacterMovement() : nullptr;
    if (!Movement)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("Network prediction settings live on a Character's CharacterMovementComponent; this Blueprint is not a Character. Nothing was changed."),
            TEXT("NOT_SUPPORTED"));
    }
    return Movement;
}
}

bool HandleConfigureClientPrediction(FNetworkingActionContext& Context)
{
    if (!Context.Payload->HasField(TEXT("enablePrediction")))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing enablePrediction"), TEXT("INVALID_PARAMS"));
        return true;
    }
    UBlueprint* Blueprint = nullptr;
    UCharacterMovementComponent* CMC = LoadCharacterMovementOrReply(Context, Blueprint);
    if (!CMC)
    {
        return true;
    }
    const bool bEnablePrediction = GetJsonBoolField(Context.Payload, TEXT("enablePrediction"), true);
    CMC->bNetworkAlwaysReplicateTransformUpdateTimestamp = bEnablePrediction;

    Context.ResultJson->SetBoolField(TEXT("enablePrediction"), bEnablePrediction);
    return SaveBlueprintAndReply(Context, Blueprint,
        FString::Printf(TEXT("bNetworkAlwaysReplicateTransformUpdateTimestamp set to %s"), bEnablePrediction ? TEXT("true") : TEXT("false")),
        TEXT("Client prediction configured"));
}

bool HandleConfigureServerCorrection(FNetworkingActionContext& Context)
{
    if (!Context.Payload->HasField(TEXT("smoothingRate")))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing smoothingRate"), TEXT("INVALID_PARAMS"));
        return true;
    }
    UBlueprint* Blueprint = nullptr;
    UCharacterMovementComponent* CMC = LoadCharacterMovementOrReply(Context, Blueprint);
    if (!CMC)
    {
        return true;
    }
    const float SmoothingRate = static_cast<float>(GetJsonNumberField(Context.Payload, TEXT("smoothingRate"), 0.5));
    CMC->NetworkSimulatedSmoothLocationTime = SmoothingRate;
    CMC->NetworkSimulatedSmoothRotationTime = SmoothingRate;
    CMC->ListenServerNetworkSimulatedSmoothLocationTime = SmoothingRate;
    CMC->ListenServerNetworkSimulatedSmoothRotationTime = SmoothingRate;

    Context.ResultJson->SetNumberField(TEXT("smoothingRate"), SmoothingRate);
    return SaveBlueprintAndReply(Context, Blueprint,
        FString::Printf(TEXT("Simulated proxy correction smoothing time set to %.2f s"), SmoothingRate),
        TEXT("Server correction configured"));
}

bool HandleConfigureMovementPrediction(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString SmoothingModeName = GetJsonStringField(Payload, TEXT("networkSmoothingMode"));
    const bool bHasMaxSmooth = Payload->HasField(TEXT("networkMaxSmoothUpdateDistance"));
    const bool bHasNoSmooth = Payload->HasField(TEXT("networkNoSmoothUpdateDistance"));
    if (SmoothingModeName.IsEmpty() && !bHasMaxSmooth && !bHasNoSmooth)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("Nothing to configure: pass networkSmoothingMode, networkMaxSmoothUpdateDistance or networkNoSmoothUpdateDistance."),
            TEXT("INVALID_PARAMS"));
        return true;
    }
    ENetworkSmoothingMode SmoothingMode = ENetworkSmoothingMode::Exponential;
    FString ValidModes;
    if (!SmoothingModeName.IsEmpty() && !TryParseNetEnum(SmoothingModeName, SmoothingMode, ValidModes))
    {
        ReplyInvalidEnum(Context, TEXT("networkSmoothingMode"), SmoothingModeName, ValidModes);
        return true;
    }

    UBlueprint* Blueprint = nullptr;
    UCharacterMovementComponent* CMC = LoadCharacterMovementOrReply(Context, Blueprint);
    if (!CMC)
    {
        return true;
    }
    if (!SmoothingModeName.IsEmpty())
    {
        CMC->NetworkSmoothingMode = SmoothingMode;
    }
    if (bHasMaxSmooth)
    {
        CMC->NetworkMaxSmoothUpdateDistance = static_cast<float>(GetJsonNumberField(Payload, TEXT("networkMaxSmoothUpdateDistance"), 256.0));
    }
    if (bHasNoSmooth)
    {
        CMC->NetworkNoSmoothUpdateDistance = static_cast<float>(GetJsonNumberField(Payload, TEXT("networkNoSmoothUpdateDistance"), 384.0));
    }

    return SaveBlueprintAndReply(Context, Blueprint,
        FString::Printf(TEXT("NetworkSmoothingMode=%s, NetworkMaxSmoothUpdateDistance=%.1f, NetworkNoSmoothUpdateDistance=%.1f"),
            *StaticEnum<ENetworkSmoothingMode>()->GetNameStringByValue(static_cast<int64>(CMC->NetworkSmoothingMode)),
            CMC->NetworkMaxSmoothUpdateDistance, CMC->NetworkNoSmoothUpdateDistance),
        TEXT("Movement prediction configured"));
}
}
