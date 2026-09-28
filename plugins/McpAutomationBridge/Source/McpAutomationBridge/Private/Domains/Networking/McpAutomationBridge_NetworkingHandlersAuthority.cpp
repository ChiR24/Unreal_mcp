#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleSetOwner(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"));
    FString OwnerActorName = GetJsonStringField(Payload, TEXT("ownerActorName"));

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

    AActor* Actor = FindActorByNameInWorldForMcp(World, ActorName, true);
    if (!Actor)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Actor not found"), TEXT("NOT_FOUND"));
        return true;
    }

    // An empty ownerActorName clears the owner; a name that matches nothing is an error, not a clear.
    AActor* Owner = OwnerActorName.IsEmpty() ? nullptr : FindActorByNameInWorldForMcp(World, OwnerActorName, true);
    if (!OwnerActorName.IsEmpty() && !Owner)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("Owner actor '%s' not found; the owner of %s was left unchanged."), *OwnerActorName, *ActorName),
            TEXT("NOT_FOUND"));
        return true;
    }
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
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    bool bIsAutonomousProxy = GetJsonBoolField(Payload, TEXT("isAutonomousProxy"), true);

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    // Enabling makes every replicated variable replicate to the autonomous proxy only; disabling undoes
    // exactly that (COND_AutonomousOnly back to COND_None) and leaves every other condition alone.
    int32 ReplicatedCount = 0;
    int32 ChangedCount = 0;
    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if ((VarDesc.PropertyFlags & CPF_Net) == 0)
        {
            continue;
        }
        ++ReplicatedCount;
        const ELifetimeCondition Current = VarDesc.ReplicationCondition;
        const ELifetimeCondition Wanted = bIsAutonomousProxy ? COND_AutonomousOnly : (Current == COND_AutonomousOnly ? COND_None : Current);
        if (Current != Wanted)
        {
            VarDesc.ReplicationCondition = Wanted;
            ++ChangedCount;
        }
    }
    if (ReplicatedCount == 0)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("The Blueprint has no replicated variables to configure; replicate one first (set_property_replicated)."),
            TEXT("NOT_FOUND"));
        return true;
    }

    if (ChangedCount > 0)
    {
        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);
    }

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("isAutonomousProxy"), bIsAutonomousProxy);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("%d of %d replicated variable(s) changed; they %s"), ChangedCount, ReplicatedCount,
        bIsAutonomousProxy ? TEXT("now replicate to the autonomous proxy only (COND_AutonomousOnly)") : TEXT("no longer use COND_AutonomousOnly")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Autonomous proxy configured"), ResultJson);
    return true;
}

bool HandleCheckHasAuthority(FNetworkingActionContext& Context)
{
    FString ActorName = GetJsonStringField(Context.Payload, TEXT("actorName"));
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

    AActor* Actor = FindActorByNameInWorldForMcp(World, ActorName, true);
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
    FString ActorName = GetJsonStringField(Context.Payload, TEXT("actorName"));
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

    AActor* Actor = FindActorByNameInWorldForMcp(World, ActorName, true);
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
