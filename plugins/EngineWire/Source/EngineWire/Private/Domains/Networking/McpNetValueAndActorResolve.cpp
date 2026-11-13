#include "Domains/Networking/McpNetPrivate.h"
#include "Foundation/Blueprint/McpBlueprintUtils.h"

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

AActor* LoadActorDefaultsOrReply(FNetworkingActionContext& Context, const FString& BlueprintPath, UBlueprint*& OutBlueprint)
{
    OutBlueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!OutBlueprint)
    {
        return nullptr;
    }
    // A Blueprint whose class never compiled has no defaults to write; the edit used to be skipped and reported.
    AActor* CDO = OutBlueprint->GeneratedClass ? Cast<AActor>(OutBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("The Blueprint has no compiled Actor class to configure; compile it first."), TEXT("NOT_SUPPORTED"));
    }
    return CDO;
}

bool SaveBlueprintAndReply(FNetworkingActionContext& Context, UBlueprint* Blueprint, const FString& Detail, const TCHAR* Message)
{
    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    // A Blueprint that could not be written answered success, with only saved false to tell.
    if (!McpSafeAssetSave(Blueprint))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("%s in this editor "
            "session, but %s could NOT be written to disk (file may be read-only or held by source control)."), *Detail,
            *Blueprint->GetPathName()), TEXT("SAVE_FAILED"));
        return true;
    }
    Context.ResultJson->SetBoolField(TEXT("saved"), true);
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
    return McpBlueprintUtils::FindGraphNamed(Blueprint->FunctionGraphs, Name.ToString());
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

}
