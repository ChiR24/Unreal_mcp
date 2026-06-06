#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleSetOwner(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString ActorName = GetStringField(Payload, TEXT("actorName"));
    FString OwnerActorName = GetStringField(Payload, TEXT("ownerActorName"));

    if (ActorName.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing actorName"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = FindActorByName(World, ActorName);
    if (!Actor)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
        return true;
    }

    AActor* Owner = OwnerActorName.IsEmpty() ? nullptr : FindActorByName(World, OwnerActorName);
    Actor->SetOwner(Owner);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("message"), Owner ? FString::Printf(TEXT("Set owner of %s to %s"), *ActorName, *OwnerActorName) : FString::Printf(TEXT("Cleared owner of %s"), *ActorName));
    McpHandlerUtils::AddVerification(ResultJson, Actor);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Owner set"), ResultJson);
    return true;
}

bool HandleSetAutonomousProxy(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    bool bIsAutonomousProxy = GetBoolField(Payload, TEXT("isAutonomousProxy"), true);

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

    bool bAnyModified = false;
    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if ((VarDesc.PropertyFlags & CPF_Net) != 0)
        {
            VarDesc.ReplicationCondition = bIsAutonomousProxy ? COND_AutonomousOnly : COND_None;
            bAnyModified = true;
        }
    }

    if (bAnyModified)
    {
        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);
    }

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("isAutonomousProxy"), bIsAutonomousProxy);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Autonomous proxy configuration %s for replicated properties"), bIsAutonomousProxy ? TEXT("enabled") : TEXT("disabled")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Autonomous proxy configured"), ResultJson);
    return true;
}

bool HandleCheckHasAuthority(FNetworkingActionContext& Context)
{
    FString ActorName = GetStringField(Context.Payload, TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing actorName"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = FindActorByName(World, ActorName);
    if (!Actor)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
        return true;
    }

    Context.ResultJson->SetBoolField(TEXT("success"), true);
    Context.ResultJson->SetBoolField(TEXT("hasAuthority"), Actor->HasAuthority());
    Context.ResultJson->SetStringField(TEXT("role"), NetRoleToString(Actor->GetLocalRole()));
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Authority checked"), Context.ResultJson);
    return true;
}

bool HandleCheckIsLocallyControlled(FNetworkingActionContext& Context)
{
    FString ActorName = GetStringField(Context.Payload, TEXT("actorName"));
    if (ActorName.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing actorName"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("No world available"), TEXT("NO_WORLD"));
        return true;
    }

    AActor* Actor = FindActorByName(World, ActorName);
    if (!Actor)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
        return true;
    }

    bool bIsLocallyControlled = false;
    bool bIsLocalController = false;
    if (APawn* Pawn = Cast<APawn>(Actor))
    {
        bIsLocallyControlled = Pawn->IsLocallyControlled();
        APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
        bIsLocalController = PC ? PC->IsLocalController() : false;
    }

    Context.ResultJson->SetBoolField(TEXT("success"), true);
    Context.ResultJson->SetBoolField(TEXT("isLocallyControlled"), bIsLocallyControlled);
    Context.ResultJson->SetBoolField(TEXT("isLocalController"), bIsLocalController);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Local control checked"), Context.ResultJson);
    return true;
}
}
