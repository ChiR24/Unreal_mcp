#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleConfigureNetDriver(FNetworkingActionContext& Context)
{
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    double MaxClientRate = GetNumberField(Context.Payload, TEXT("maxClientRate"), 15000.0);
    double MaxInternetClientRate = GetNumberField(Context.Payload, TEXT("maxInternetClientRate"), 10000.0);
    double NetServerMaxTickRate = GetNumberField(Context.Payload, TEXT("netServerMaxTickRate"), 30.0);
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    bool bConfigApplied = false;

    if (World && World->GetNetDriver())
    {
        UNetDriver* NetDriver = World->GetNetDriver();
        NetDriver->MaxClientRate = static_cast<int32>(MaxClientRate);
        NetDriver->MaxInternetClientRate = static_cast<int32>(MaxInternetClientRate);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        NetDriver->SetNetServerMaxTickRate(static_cast<int32>(NetServerMaxTickRate));
#else
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        NetDriver->NetServerMaxTickRate = static_cast<int32>(NetServerMaxTickRate);
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
        bConfigApplied = true;
    }

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("appliedToActiveDriver"), bConfigApplied);
    ResultJson->SetNumberField(TEXT("maxClientRate"), MaxClientRate);
    ResultJson->SetNumberField(TEXT("maxInternetClientRate"), MaxInternetClientRate);
    ResultJson->SetNumberField(TEXT("netServerMaxTickRate"), NetServerMaxTickRate);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net driver configured (maxClientRate=%.0f, maxInternetClientRate=%.0f, tickRate=%.0f)"),
        MaxClientRate, MaxInternetClientRate, NetServerMaxTickRate));
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Net driver configured"), ResultJson);
    return true;
}

bool HandleSetNetRole(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    FString Role = GetStringField(Payload, TEXT("role"));

    if (BlueprintPath.IsEmpty() || Role.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    ENetRole NetRole = GetNetRole(Role);
    if (CDO)
    {
        CDO->SetReplicates(NetRole != ROLE_None);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("role"), Role);
    ResultJson->SetBoolField(TEXT("replicates"), CDO ? CDO->GetIsReplicated() : false);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net role configured to %s (replicates=%s)"), *Role, CDO && CDO->GetIsReplicated() ? TEXT("true") : TEXT("false")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Net role configured"), ResultJson);
    return true;
}

bool HandleConfigureReplicatedMovement(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    bool bReplicateMovement = GetBoolField(Payload, TEXT("replicateMovement"), true);

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

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
        CDO->SetReplicatingMovement(bReplicateMovement);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Replicate movement set to %s"), bReplicateMovement ? TEXT("true") : TEXT("false")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Replicated movement configured"), ResultJson);
    return true;
}
}
