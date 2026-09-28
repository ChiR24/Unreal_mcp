#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

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
    McpSafeAssetSave(Blueprint);
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

void ReplyInvalidEnum(FNetworkingActionContext& Context, const TCHAR* Field, const FString& Value, const FString& ValidNames)
{
    Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("Unknown %s '%s'; nothing was changed. Use one of: %s."), Field, *Value, *ValidNames),
        TEXT("INVALID_ARGUMENT"));
}

FString NetRoleToString(ENetRole Role) { return StaticEnum<ENetRole>()->GetNameStringByValue(Role); }
FString NetDormancyToString(ENetDormancy Dormancy) { return StaticEnum<ENetDormancy>()->GetNameStringByValue(Dormancy); }
}
