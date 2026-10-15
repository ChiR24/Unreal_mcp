#include "Domains/Interaction/McpInteractionPrivate.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"

#include "EditorAssetLibrary.h"

namespace McpInteractionHandlers
{
bool HandleDoorAction(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const double OpenAngle = GetJsonNumberField(Payload, TEXT("openAngle"), 90.0);
    const double OpenTime = GetJsonNumberField(Payload, TEXT("openTime"), 0.5);
    const bool Locked = GetJsonBoolField(Payload, TEXT("locked"), false);
    using EType = EInteractionVarType;
    if (SubAction == TEXT("create_door_actor"))
    {
        const bool AutoClose = GetJsonBoolField(Payload, TEXT("autoClose"), false);
        const double AutoCloseDelay = GetJsonNumberField(Payload, TEXT("autoCloseDelay"), 3.0);
        const bool RequiresKey = GetJsonBoolField(Payload, TEXT("requiresKey"), false);
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetNumberField(TEXT("openAngle"), OpenAngle);
        Result->SetNumberField(TEXT("openTime"), OpenTime);
        Result->SetBoolField(TEXT("autoClose"), AutoClose);
        Result->SetNumberField(TEXT("autoCloseDelay"), AutoCloseDelay);
        Result->SetBoolField(TEXT("requiresKey"), RequiresKey);

        // An existing door answers idempotently instead of being refused.
        const FString Name = GetJsonStringField(Payload, TEXT("name"));
        FString PackageName;
        FString PathError;
        UBlueprint* ExistingDoorBP =
            !Name.IsEmpty() &&
                    ValidateAssetCreationPath(GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/Interactables")),
                                              Name, PackageName, PathError) &&
                    McpAssetExists(PackageName)
                ? LoadObject<UBlueprint>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)))
                : nullptr;
        // An existing door gets the opening logic too (it is re-runnable), so one made before it existed opens.
        if (ExistingDoorBP)
        {
            Result->SetBoolField(TEXT("alreadyExisted"), true);
            if (!AuthorOpenOnApproach(Subsystem, RequestId, RequestingSocket, ExistingDoorBP, TEXT("DoorPivot"),
                                      TEXT("OpenAngle"), TEXT("Yaw"), Result))
            {
                return true;
            }
            McpHandlerUtils::AddVerification(Result, ExistingDoorBP);
            Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Door actor already exists"), Result);
            return true;
        }

        UBlueprint* DoorBP = CreateInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload, TEXT("/Game/Interactables"), TEXT("door"), {
            {USceneComponent::StaticClass(), TEXT("Root"), nullptr, 0.0f},
            {USceneComponent::StaticClass(), TEXT("DoorPivot"), nullptr, 0.0f},
            {UStaticMeshComponent::StaticClass(), TEXT("DoorMesh"), TEXT("DoorPivot"), 0.0f},
            {UBoxComponent::StaticClass(), TEXT("InteractionTrigger"), nullptr, 100.0f}});
        if (!DoorBP)
        {
            return true;
        }
        // create_door_actor used to echo these five settings and store none of them.
        const int32 NotApplied = ApplyInteractionVars(DoorBP, {
            {TEXT("OpenAngle"), EType::Float, MakeShared<FJsonValueNumber>(OpenAngle)},
            {TEXT("OpenTime"), EType::Float, MakeShared<FJsonValueNumber>(OpenTime)},
            {TEXT("AutoCloseDelay"), EType::Float, MakeShared<FJsonValueNumber>(AutoCloseDelay)},
            {TEXT("bAutoClose"), EType::Bool, MakeShared<FJsonValueBoolean>(AutoClose)},
            {TEXT("bRequiresKey"), EType::Bool, MakeShared<FJsonValueBoolean>(RequiresKey)},
            {TEXT("bIsLocked"), EType::Bool, MakeShared<FJsonValueBoolean>(Locked)}});
        Result->SetBoolField(TEXT("propertiesApplied"), NotApplied == 0);
        if (!AuthorOpenOnApproach(Subsystem, RequestId, RequestingSocket, DoorBP, TEXT("DoorPivot"), TEXT("OpenAngle"),
                                  TEXT("Yaw"), Result))
        {
            return true;
        }
        SendInteractableResult(Subsystem, RequestId, RequestingSocket, DoorBP, Result,
                               {TEXT("created door blueprint"), TEXT("added door components")}, TEXT("Door actor created"));
        return true;
    }

    if (SubAction != TEXT("configure_door_properties"))
    {
        return false;
    }
    UBlueprint* Blueprint = LoadInteractableBlueprint(Subsystem, RequestId, RequestingSocket, Payload,
                                                      TEXT("doorPath"), TEXT("door"), {TEXT("DoorPivot"), TEXT("DoorMesh")});
    if (!Blueprint)
    {
        return true;
    }
    // A door a switch opens stays shut when a pawn walks up (locked), unless locked is given.
    const FString SwitchPath = GetJsonStringField(Payload, TEXT("switchPath"));
    TSharedPtr<FJsonValue> LockedValue = Payload->TryGetField(TEXT("locked"));
    if (!SwitchPath.IsEmpty() && !LockedValue.IsValid())
    {
        LockedValue = MakeShared<FJsonValueBoolean>(true);
    }
    // The link first: a refused switchPath changes nothing.
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (!SwitchPath.IsEmpty() && !AuthorOpenFromSwitch(Subsystem, RequestId, RequestingSocket, Blueprint, SwitchPath, Result))
    {
        return true;
    }
    // Only the fields the caller passed are written (a null Value only ensures the variable): defaulting
    // the rest used to reset them, so changing openAngle alone unlocked a locked door.
    const int32 NotApplied = ApplyInteractionVars(Blueprint, {
        {TEXT("OpenAngle"), EType::Float, Payload->TryGetField(TEXT("openAngle"))},
        {TEXT("OpenTime"), EType::Float, Payload->TryGetField(TEXT("openTime"))},
        {TEXT("bIsLocked"), EType::Bool, LockedValue},
        {TEXT("bIsOpen"), EType::Bool, nullptr}});
    if (Payload->HasField(TEXT("openAngle"))) { Result->SetNumberField(TEXT("openAngle"), OpenAngle); }
    if (Payload->HasField(TEXT("openTime"))) { Result->SetNumberField(TEXT("openTime"), OpenTime); }
    if (LockedValue.IsValid()) { Result->SetBoolField(TEXT("locked"), LockedValue->AsBool()); }
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetBoolField(TEXT("propertiesApplied"), NotApplied == 0);
    Result->SetStringField(TEXT("doorPath"), GetJsonStringField(Payload, TEXT("doorPath")));
    SendInteractableResult(Subsystem, RequestId, RequestingSocket, Blueprint, Result,
                           {TEXT("configured door properties")}, TEXT("Door properties configured"));
    return true;
}
}
