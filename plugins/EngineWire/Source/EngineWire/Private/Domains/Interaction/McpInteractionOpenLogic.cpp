// The logic the interactables made by create_interactable run, written in through McpBlueprintBehaviour::Author from
// the Interaction recipes: a door or chest opens while a pawn is near (OpenOnApproach), a switch fires
// OnSwitchChanged (SwitchToggle, SwitchPlate; a lever also tilts its handle, LeverTilt), and a door can follow a
// switch (OpenFromSwitch). They were components and variables only, so nothing made over MCP opened or switched.
#include "Domains/Interaction/McpInteractionPrivate.h"
#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviour.h"

namespace McpInteractionHandlers
{
namespace
{
bool AuthorInteractionRecipe(UEngineWireSubsystem* Subsystem, const FString& RequestId,
                             TSharedPtr<FMcpBridgeWebSocket> Socket, UBlueprint* Blueprint, const TCHAR* Name,
                             const TMap<FString, TSharedPtr<FJsonValue>>& Values, const TSharedPtr<FJsonObject>& Result)
{
    return McpBlueprintBehaviour::AuthorRecipeFile(*Subsystem, RequestId, Socket, Blueprint, TEXT("Interaction"), Name,
                                                   Values, Result);
}
} // namespace

bool AuthorOpenOnApproach(UEngineWireSubsystem* Subsystem, const FString& RequestId,
                          TSharedPtr<FMcpBridgeWebSocket> Socket, UBlueprint* Blueprint, const TCHAR* Pivot,
                          const TCHAR* AngleVariable, const TCHAR* RotatorPin, const TSharedPtr<FJsonObject>& Result)
{
    TMap<FString, TSharedPtr<FJsonValue>> Values;
    Values.Add(TEXT("Pivot"), MakeShared<FJsonValueString>(Pivot));
    Values.Add(TEXT("AngleVariable"), MakeShared<FJsonValueString>(AngleVariable));
    Values.Add(TEXT("RotatorPin"), MakeShared<FJsonValueString>(RotatorPin));
    if (!AuthorInteractionRecipe(Subsystem, RequestId, Socket, Blueprint, TEXT("OpenOnApproach"), Values, Result))
    {
        return false;
    }
    Result->SetBoolField(TEXT("opensOnApproach"), true);
    return true;
}

bool AuthorSwitchLogic(UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                       UBlueprint* Blueprint, const FString& SwitchType, bool bTilt, const TSharedPtr<FJsonObject>& Result)
{
    const TCHAR* Logic = SwitchType.Equals(TEXT("pressure_plate"), ESearchCase::IgnoreCase) ? TEXT("SwitchPlate")
                                                                                             : TEXT("SwitchToggle");
    if (!AuthorInteractionRecipe(Subsystem, RequestId, Socket, Blueprint, Logic, {}, Result) ||
        (bTilt && !AuthorInteractionRecipe(Subsystem, RequestId, Socket, Blueprint, TEXT("LeverTilt"), {}, Result)))
    {
        return false;
    }
    Result->SetStringField(TEXT("dispatcher"), TEXT("OnSwitchChanged"));
    return true;
}

bool AuthorOpenFromSwitch(UEngineWireSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                          UBlueprint* Door, const FString& SwitchPath, const TSharedPtr<FJsonObject>& Result)
{
    FString ResolvedPath;
    FString LoadError;
    UBlueprint* Switch = LoadBlueprintAsset(SwitchPath, ResolvedPath, LoadError);
    UClass* SwitchClass = Switch ? Switch->GeneratedClass.Get() : nullptr;
    if (!SwitchClass || !FindFProperty<FMulticastDelegateProperty>(SwitchClass, TEXT("OnSwitchChanged")))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            Switch ? FString::Printf(TEXT("%s has no OnSwitchChanged dispatcher: make it with create_interactable kind "
                                          "switch, lever or trigger."), *ResolvedPath)
                   : LoadError,
            Switch ? TEXT("DISPATCHER_NOT_FOUND") : TEXT("BLUEPRINT_NOT_FOUND"));
        return false;
    }
    // The opening logic turns the door toward bIsOpen; a door made before it existed gets it first.
    if (!AuthorOpenOnApproach(Subsystem, RequestId, Socket, Door, TEXT("DoorPivot"), TEXT("OpenAngle"), TEXT("Yaw"), Result))
    {
        return false;
    }
    TMap<FString, TSharedPtr<FJsonValue>> Values;
    Values.Add(TEXT("SwitchClass"), MakeShared<FJsonValueString>(SwitchClass->GetPathName()));
    if (!AuthorInteractionRecipe(Subsystem, RequestId, Socket, Door, TEXT("OpenFromSwitch"), Values, Result))
    {
        return false;
    }
    Result->SetStringField(TEXT("followsSwitch"), Switch->GetPathName());
    return true;
}
}
