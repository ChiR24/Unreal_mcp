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

namespace
{
// The enum value spelled Name ("COND_OwnerOnly", "DORM_Awake", "ROLE_Authority"; case ignored), else Fallback.
template <typename TEnum>
TEnum EnumFromName(const FString& Name, TEnum Fallback)
{
    const int64 Value = StaticEnum<TEnum>()->GetValueByNameString(Name);
    return Value == INDEX_NONE ? Fallback : static_cast<TEnum>(Value);
}
}

ELifetimeCondition GetReplicationCondition(const FString& ConditionStr) { return EnumFromName(ConditionStr, COND_None); }
ENetDormancy GetNetDormancy(const FString& DormancyStr) { return EnumFromName(DormancyStr, DORM_Never); }
ENetRole GetNetRole(const FString& RoleStr) { return EnumFromName(RoleStr, ROLE_None); }
FString NetRoleToString(ENetRole Role) { return StaticEnum<ENetRole>()->GetNameStringByValue(Role); }
FString NetDormancyToString(ENetDormancy Dormancy) { return StaticEnum<ENetDormancy>()->GetNameStringByValue(Dormancy); }
}
