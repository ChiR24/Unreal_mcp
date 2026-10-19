#include "Domains/Networking/McpNetPrivate.h"

#include "Kismet2/Kismet2NameValidators.h"

namespace McpNetworkingHandlers
{
UBlueprint* LoadBlueprintOrReply(FNetworkingActionContext& Context, const FString& BlueprintPath)
{
    if (BlueprintPath.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
        return nullptr;
    }
    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
    }
    return Blueprint;
}

bool SaveBlueprintAndReply(FNetworkingActionContext& Context, UBlueprint* Blueprint, const FString& Detail, const TCHAR* Message)
{
    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    Context.ResultJson->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
    Context.ResultJson->SetBoolField(TEXT("success"), true);
    Context.ResultJson->SetStringField(TEXT("message"), Detail);
    McpHandlerUtils::AddVerification(Context.ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, Message, Context.ResultJson);
    return true;
}

UBlueprint* LoadBlueprintFromPath(const FString& BlueprintPath)
{
    FString Normalized;
    FString Error;
    return LoadBlueprintAsset(BlueprintPath, Normalized, Error);
}

UEdGraph* FindFunctionGraph(UBlueprint* Blueprint, const FName Name)
{
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetFName() == Name)
        {
            return Graph;
        }
    }
    return nullptr;
}

bool CheckFreeMemberNameOrReply(FNetworkingActionContext& Context, UBlueprint* Blueprint, const FString& Name, const TCHAR* Field)
{
    const EValidatorResult Check = FKismetNameValidator(Blueprint).IsValid(Name);
    if (Check == EValidatorResult::Ok)
    {
        return true;
    }
    const bool bTaken = Check == EValidatorResult::AlreadyInUse || Check == EValidatorResult::ExistingName || Check == EValidatorResult::LocallyInUse;
    if (bTaken)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("%s '%s' is already a function, variable, event or other member of %s (or of its parent class); pick another name. Nothing was changed."), Field, *Name, *Blueprint->GetName()),
            TEXT("NAME_TAKEN"));
        return false;
    }
    Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("%s '%s' is not a valid member name (empty, too long, or holding characters a Blueprint name cannot). Nothing was changed."), Field, *Name),
        TEXT("INVALID_ARGUMENT"));
    return false;
}

void ReplyInvalidEnum(FNetworkingActionContext& Context, const TCHAR* Field, const FString& Value, const FString& ValidNames)
{
    Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("Unknown %s '%s'; nothing was changed. Use one of: %s."), Field, *Value, *ValidNames),
        TEXT("INVALID_ARGUMENT"));
}

FString NetRoleToString(ENetRole Role) { return StaticEnum<ENetRole>()->GetNameStringByValue(Role); }
FString NetDormancyToString(ENetDormancy Dormancy) { return StaticEnum<ENetDormancy>()->GetNameStringByValue(Dormancy); }
}
