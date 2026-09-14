#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {
namespace {

struct FResolvedDeleteTarget
{
    FString Target;
    FString ActorPath;
    AActor *Actor = nullptr;
};

void AddDeleteTargets(const TSharedPtr<FJsonObject> &Payload, TArray<FString> &Targets)
{
    const TArray<TSharedPtr<FJsonValue>> *NameValues = nullptr;
    if (Payload->TryGetArrayField(TEXT("names"), NameValues) && NameValues)
    {
        for (const TSharedPtr<FJsonValue> &Value : *NameValues)
        {
            FString Target;
            if (Value.IsValid() && McpHandlerUtils::TryGetJsonValueString(Value, Target))
            {
                Target = Target.TrimStartAndEnd();
                if (!Target.IsEmpty())
                {
                    Targets.AddUnique(Target);
                }
            }
        }
    }

    const FString SingleTarget = McpGetFirstStringField(
        Payload, {TEXT("actorPath"), TEXT("name"), TEXT("actorName"), TEXT("targetActor")});
    if (!SingleTarget.IsEmpty())
    {
        Targets.AddUnique(SingleTarget.TrimStartAndEnd());
    }
}

AActor *ResolveDeleteTarget(
    const FString &Target, const TArray<AActor *> &Actors, bool &bOutAmbiguous)
{
    bOutAmbiguous = false;
    for (AActor *Actor : Actors)
    {
        if (Actor && Actor->GetPathName().Equals(Target, ESearchCase::CaseSensitive))
        {
            return Actor;
        }
    }

    TArray<AActor *> NameMatches;
    for (AActor *Actor : Actors)
    {
        if (Actor &&
            (Actor->GetActorLabel().Equals(Target, ESearchCase::IgnoreCase) ||
             Actor->GetName().Equals(Target, ESearchCase::IgnoreCase)))
        {
            NameMatches.AddUnique(Actor);
        }
    }

    bOutAmbiguous = NameMatches.Num() > 1;
    return NameMatches.Num() == 1 ? NameMatches[0] : nullptr;
}

void SetDeleteResult(
    FEnvironmentBuildContext &Context, const TArray<FString> &DeletedTargets,
    const TArray<FString> &MissingTargets, const TArray<FString> &AmbiguousTargets,
    const TArray<FString> &FailedTargets)
{
    McpAddStringArrayField(Context.Resp, TEXT("deletedTargets"), DeletedTargets);
    McpAddStringArrayField(Context.Resp, TEXT("missingTargets"), MissingTargets);
    McpAddStringArrayField(Context.Resp, TEXT("ambiguousTargets"), AmbiguousTargets);
    McpAddStringArrayField(Context.Resp, TEXT("failedTargets"), FailedTargets);
    Context.Resp->SetNumberField(TEXT("requestedCount"),
        DeletedTargets.Num() + MissingTargets.Num() + AmbiguousTargets.Num() + FailedTargets.Num());
    Context.Resp->SetNumberField(TEXT("deletedCount"), DeletedTargets.Num());

    const int32 ErrorCategoryCount =
        (MissingTargets.IsEmpty() ? 0 : 1) +
        (AmbiguousTargets.IsEmpty() ? 0 : 1) +
        (FailedTargets.IsEmpty() ? 0 : 1);
    if (ErrorCategoryCount == 0)
    {
        Context.bSuccess = true;
        Context.Message = FString::Printf(TEXT("Deleted %d environment actor(s)"), DeletedTargets.Num());
        Context.ErrorCode.Empty();
        return;
    }

    Context.bSuccess = false;
    Context.Message = FString::Printf(
        TEXT("Deleted %d actor(s); %d missing, %d ambiguous, %d failed"),
        DeletedTargets.Num(), MissingTargets.Num(), AmbiguousTargets.Num(), FailedTargets.Num());
    if (!DeletedTargets.IsEmpty())
    {
        Context.ErrorCode = TEXT("DELETE_PARTIAL");
    }
    else if (!AmbiguousTargets.IsEmpty())
    {
        Context.ErrorCode = TEXT("AMBIGUOUS_ACTOR");
    }
    else if (!MissingTargets.IsEmpty())
    {
        Context.ErrorCode = TEXT("ACTOR_NOT_FOUND");
    }
    else
    {
        Context.ErrorCode = TEXT("DELETE_FAILED");
    }
}

}

bool HandleBuildSnapshotAndDeletionAction(const FString &LowerSub, FEnvironmentBuildContext &Context)
{
    if (LowerSub == TEXT("export_snapshot") || LowerSub == TEXT("import_snapshot"))
    {
        return HandleBuildSnapshotAction(LowerSub, Context);
    }
    if (LowerSub != TEXT("delete"))
    {
        return false;
    }

    if (!GEditor)
    {
        Context.bSuccess = false;
        Context.Message = TEXT("Editor is unavailable");
        Context.ErrorCode = TEXT("EDITOR_NOT_AVAILABLE");
        return true;
    }
    UEditorActorSubsystem *ActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
    if (!ActorSubsystem)
    {
        Context.bSuccess = false;
        Context.Message = TEXT("Editor actor subsystem is unavailable");
        Context.ErrorCode = TEXT("ACTOR_SUBSYSTEM_NOT_AVAILABLE");
        return true;
    }

    TArray<FString> Targets;
    AddDeleteTargets(Context.Payload, Targets);
    if (Targets.IsEmpty())
    {
        Context.bSuccess = false;
        Context.Message = TEXT("delete requires names, actorPath, name, actorName, or targetActor");
        Context.ErrorCode = TEXT("INVALID_ARGUMENT");
        return true;
    }

    const TArray<AActor *> Actors = ActorSubsystem->GetAllLevelActors();
    TArray<FString> DeletedTargets;
    TArray<FString> MissingTargets;
    TArray<FString> AmbiguousTargets;
    TArray<FString> FailedTargets;
    TArray<FResolvedDeleteTarget> ResolvedTargets;
    TSet<AActor *> ResolvedActors;
    for (const FString &Target : Targets)
    {
        bool bAmbiguous = false;
        AActor *Actor = ResolveDeleteTarget(Target, Actors, bAmbiguous);
        if (bAmbiguous)
        {
            AmbiguousTargets.Add(Target);
            continue;
        }
        if (!Actor)
        {
            MissingTargets.Add(Target);
            continue;
        }
        if (ResolvedActors.Contains(Actor))
        {
            continue;
        }
        ResolvedActors.Add(Actor);
        FResolvedDeleteTarget ResolvedTarget;
        ResolvedTarget.Target = Target;
        ResolvedTarget.ActorPath = Actor->GetPathName();
        ResolvedTarget.Actor = Actor;
        ResolvedTargets.Add(MoveTemp(ResolvedTarget));
    }

    if (!AmbiguousTargets.IsEmpty() || !MissingTargets.IsEmpty())
    {
        SetDeleteResult(Context, DeletedTargets, MissingTargets, AmbiguousTargets, FailedTargets);
        return true;
    }

    TArray<AActor *> ActorsToDestroy;
    for (const FResolvedDeleteTarget &ResolvedTarget : ResolvedTargets)
    {
        AActor *Actor = ResolvedTarget.Actor;
        if (!IsValid(Actor))
        {
            FailedTargets.Add(ResolvedTarget.Target);
            continue;
        }
        ActorsToDestroy.Add(Actor);
    }
    if (!FailedTargets.IsEmpty())
    {
        SetDeleteResult(Context, DeletedTargets, MissingTargets, AmbiguousTargets, FailedTargets);
        return true;
    }

    const bool bDestroyReportedSuccess = ActorSubsystem->DestroyActors(ActorsToDestroy);
    const TArray<AActor *> RemainingActors = ActorSubsystem->GetAllLevelActors();
    for (const FResolvedDeleteTarget &ResolvedTarget : ResolvedTargets)
    {
        if (!RemainingActors.Contains(ResolvedTarget.Actor))
        {
            DeletedTargets.Add(ResolvedTarget.Target);
        }
        else
        {
            FailedTargets.Add(ResolvedTarget.Target);
        }
    }

    if (!bDestroyReportedSuccess || !FailedTargets.IsEmpty())
    {
        const bool bMutationOccurred = !DeletedTargets.IsEmpty();
        bool bRolledBack = !bMutationOccurred || GEditor->UndoTransaction(false);
        if (bRolledBack && bMutationOccurred)
        {
            const TArray<AActor *> RestoredActors = ActorSubsystem->GetAllLevelActors();
            for (const FResolvedDeleteTarget &ResolvedTarget : ResolvedTargets)
            {
                bool bAmbiguous = false;
                bRolledBack = ResolveDeleteTarget(
                    ResolvedTarget.ActorPath, RestoredActors, bAmbiguous) != nullptr;
                if (!bRolledBack)
                {
                    break;
                }
            }
        }
        Context.Resp->SetBoolField(TEXT("rolledBack"), bRolledBack);
        DeletedTargets.Empty();
        FailedTargets = Targets;
        SetDeleteResult(Context, DeletedTargets, MissingTargets, AmbiguousTargets, FailedTargets);
        if (!bRolledBack)
        {
            Context.ErrorCode = TEXT("DELETE_ROLLBACK_FAILED");
            Context.Message = TEXT("Environment actor deletion failed and rollback did not complete");
        }
        return true;
    }

    SetDeleteResult(Context, DeletedTargets, MissingTargets, AmbiguousTargets, FailedTargets);
    return true;
}

}
#endif
