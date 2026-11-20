// manage_ai get_live_ai_state: what an AI is doing in Play right now, in one read: the Behavior Tree node it runs, the
// tasks active, and every blackboard key's value. The calls are exported and the same in every 5.x.

#include "Domains/AI/McpAIContext.h"

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BTNode.h"
#include "Editor.h"
#include "GameFramework/Pawn.h"

namespace McpAIHandlers
{
bool HandleGetLiveAIState(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                          TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (!GEditor || !GEditor->PlayWorld)
    {
        Self->SendAutomationError(Socket, RequestId, TEXT("No Play session is running: an AI's live state exists only in Play (control_editor play)."), TEXT("PIE_NOT_RUNNING"));
        return true;
    }
    const FString Name = GetJsonStringField(Payload, TEXT("actorName"));
    AActor* Actor = Name.IsEmpty() ? nullptr : McpHandlerUtils::FindActorByName(Name);
    AAIController* Controller = Cast<AAIController>(Actor);
    if (const APawn* Pawn = Cast<APawn>(Actor))
    {
        Controller = Cast<AAIController>(Pawn->GetController());
    }
    if (!Controller)
    {
        Self->SendAutomationError(Socket, RequestId, Actor
            ? FString::Printf(TEXT("%s is neither an AI controller nor a pawn one possesses."), *Actor->GetActorLabel())
            : FString::Printf(TEXT("No actor named '%s' in the Play world (control_actor list names them)."), *Name),
            Actor ? TEXT("NOT_AI_CONTROLLED") : TEXT("ACTOR_NOT_FOUND"));
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("controller"), Controller->GetName());
    Result->SetStringField(TEXT("pawn"), Controller->GetPawn() ? Controller->GetPawn()->GetActorLabel() : FString());
    const UBehaviorTreeComponent* Tree = Cast<UBehaviorTreeComponent>(Controller->GetBrainComponent());
    Result->SetBoolField(TEXT("treeRunning"), Tree && Tree->IsRunning());
    if (Tree)
    {
        const UBehaviorTree* Asset = Tree->GetCurrentTree();
        const UBTNode* Active = Tree->GetActiveNode();
        Result->SetStringField(TEXT("behaviorTree"), Asset ? Asset->GetPathName() : FString());
        Result->SetStringField(TEXT("activeNode"), Active ? Active->GetNodeName() : FString());
        Result->SetStringField(TEXT("activeNodeClass"), Active ? Active->GetClass()->GetName() : FString());
        Result->SetStringField(TEXT("activeTasks"), Tree->DescribeActiveTasks());
    }
    const UBlackboardComponent* Board = Controller->GetBlackboardComponent();
    if (Board && Board->GetBlackboardAsset())
    {
        TSharedPtr<FJsonObject> Keys = MakeShared<FJsonObject>();
        for (int32 Index = 0; Index < Board->GetNumKeys(); ++Index)
        {
            const FBlackboard::FKey Key = static_cast<FBlackboard::FKey>(Index);
            Keys->SetStringField(Board->GetKeyName(Key).ToString(), Board->DescribeKeyValue(Key, EBlackboardDescription::OnlyValue));
        }
        Result->SetStringField(TEXT("blackboard"), Board->GetBlackboardAsset()->GetPathName());
        Result->SetObjectField(TEXT("keys"), Keys);
    }
    Self->SendAutomationResponse(Socket, RequestId, true, Tree && Tree->IsRunning()
        ? FString::Printf(TEXT("%s runs %s"), *Controller->GetName(), Tree->GetActiveNode() ? *Tree->GetActiveNode()->GetNodeName() : TEXT("no node"))
        : FString::Printf(TEXT("%s runs no Behavior Tree"), *Controller->GetName()), Result);
    return true;
}
} // namespace McpAIHandlers
