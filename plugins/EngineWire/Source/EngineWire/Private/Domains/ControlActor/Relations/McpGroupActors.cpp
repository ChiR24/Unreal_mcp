// control_actor group_actors: actors grouped as the editor's Group (Ctrl+G) does, so they select and move as one, or
// with op ungroup the outermost groups they belong to broken up again, through UActorGroupingUtils (the same in every
// 5.x). Each is the engine's own undo step (Regroup, Disband Group), as from the menu. The editor's grouping must be on
// (GROUPING_OFF names where), the actors must share a level (DIFFERENT_LEVELS), and every name is found first, so a
// refusal changes nothing.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "ActorGroupingUtils.h"
#include "Dom/JsonObject.h"
#include "Editor/GroupActor.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "GameFramework/Actor.h"

namespace McpRelations
{
bool HandleGroupActors(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge->SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was changed."), Code);
        return true;
    };
    const bool bUngroup = GetJsonStringField(Payload, TEXT("op"), TEXT("group")).Equals(TEXT("ungroup"), ESearchCase::IgnoreCase);
    TArray<AActor*> Actors;
    for (const FString& Name : McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames")))
    {
        AActor* Actor = Find(Name);
        if (!Actor)
        {
            return Refuse(FString::Printf(TEXT("No actor is labelled or named %s."), *Name), TEXT("ACTOR_NOT_FOUND"));
        }
        Actors.AddUnique(Actor);
    }
    if (!UActorGroupingUtils::IsGroupingActive())
    {
        return Refuse(TEXT("Actor grouping is turned off in this editor (the level editor's group mode toggle)."), TEXT("GROUPING_OFF"));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (bUngroup)
    {
        // The outermost group of each, locked or not (GetParentForActor, the engine's own fallback, is not exported).
        TSet<AGroupActor*> Groups;
        for (AActor* Actor : Actors)
        {
            if (AGroupActor* Group = AGroupActor::GetRootForActor(Actor))
            {
                Groups.Add(Group);
            }
        }
        if (Groups.Num() == 0)
        {
            return Refuse(TEXT("None of the actors is in a group."), TEXT("NOT_GROUPED"));
        }
        // The engine opens its own undo step (Disband Group).
        UActorGroupingUtils::Get()->UngroupActors(Actors);
        Result->SetNumberField(TEXT("ungrouped"), Groups.Num());
        Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Broke up %d group(s)."), Groups.Num()), Result);
        return true;
    }
    if (Actors.Num() < 2)
    {
        return Refuse(TEXT("A group needs two or more different actors."), TEXT("INVALID_ARGUMENT"));
    }
    for (const AActor* Actor : Actors)
    {
        if (Actor->GetLevel() != Actors[0]->GetLevel())
        {
            return Refuse(FString::Printf(TEXT("%s is in another level than %s; a group lives in one level."), *Actor->GetActorLabel(),
                *Actors[0]->GetActorLabel()), TEXT("DIFFERENT_LEVELS"));
        }
    }
    // The engine opens its own undo step (Regroup) and puts the group in the members' common outliner folder.
    AGroupActor* Group = UActorGroupingUtils::Get()->GroupActors(Actors);
    if (!Group)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("The editor did not group the actors (a project filter refused them)."), TEXT("GROUP_FAILED"));
        return true;
    }
    TArray<FString> Members;
    for (const AActor* Actor : Actors)
    {
        Members.Add(Actor->GetActorLabel());
    }
    Result->SetStringField(TEXT("group"), McpActorRef(Group));
    Result->SetArrayField(TEXT("members"), McpHandlerUtils::ToJsonStringArray(Members));
    Bridge->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Grouped %d actor(s) as %s."), Members.Num(), *Group->GetActorLabel()), Result);
    return true;
}
} // namespace McpRelations
