#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Decorators/BTDecorator_Cooldown.h"
#include "BehaviorTree/Decorators/BTDecorator_Loop.h"
#include "BehaviorTree/Services/BTService_DefaultFocus.h"
#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

namespace
{
bool MatchesDecoratorTarget(const UBTNode* Node, const FString& Id)
{
    return Node && (Node->GetName().Equals(Id, ESearchCase::IgnoreCase) ||
                    Node->GetPathName().Equals(Id, ESearchCase::IgnoreCase) ||
                    Node->GetNodeName().Equals(Id, ESearchCase::IgnoreCase));
}

// The composite-child slot whose composite or task is Id: a decorator on a node lives in the
// entry its parent keeps for it, not on the node itself.
FBTCompositeChild* FindDecoratorSlot(UBTCompositeNode* Composite, const FString& Id)
{
    if (!Composite)
    {
        return nullptr;
    }
    for (FBTCompositeChild& Child : Composite->Children)
    {
        if (MatchesDecoratorTarget(Child.ChildComposite, Id) || MatchesDecoratorTarget(Child.ChildTask, Id))
        {
            return &Child;
        }
        if (FBTCompositeChild* Found = FindDecoratorSlot(Child.ChildComposite, Id))
        {
            return Found;
        }
    }
    return nullptr;
}
}

namespace McpAIHandlers
{
bool HandleAddDecorator(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    FString DecoratorType = GetJsonStringField(Payload, TEXT("decoratorType"));
    // The node the decorator guards; empty or "root" means a root decorator.
    const FString ParentNodeId = GetJsonStringField(Payload, TEXT("parentNodeId"));

    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
    if (!BT)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    UBTDecorator* NewDecorator = nullptr;
    // Support both short names and full class names
    if (DecoratorType.Equals(TEXT("Blackboard"), ESearchCase::IgnoreCase) ||
        DecoratorType.Equals(TEXT("BlackboardDecorator"), ESearchCase::IgnoreCase))
    {
        NewDecorator = NewObject<UBTDecorator_Blackboard>(BT);
    }
    else if (DecoratorType.Equals(TEXT("Cooldown"), ESearchCase::IgnoreCase) ||
             DecoratorType.Equals(TEXT("CooldownDecorator"), ESearchCase::IgnoreCase))
    {
        NewDecorator = NewObject<UBTDecorator_Cooldown>(BT);
    }
    else if (DecoratorType.Equals(TEXT("Loop"), ESearchCase::IgnoreCase) ||
             DecoratorType.Equals(TEXT("LoopDecorator"), ESearchCase::IgnoreCase))
    {
        NewDecorator = NewObject<UBTDecorator_Loop>(BT);
    }
    else
    {
        // The rest of the advertised types (TimeLimit, ForceSuccess, ConeCheck...) are AIModule
        // BTDecorator_<Type> classes; abstract bases such as BlackboardBase cannot be instanced.
        UClass* DecoratorClass = FindObject<UClass>(nullptr,
            *FString::Printf(TEXT("/Script/AIModule.BTDecorator_%s"), *DecoratorType));
        if (DecoratorClass && DecoratorClass->IsChildOf(UBTDecorator::StaticClass()) && !DecoratorClass->HasAnyClassFlags(CLASS_Abstract))
        {
            NewDecorator = NewObject<UBTDecorator>(BT, DecoratorClass);
        }
    }

    if (NewDecorator)
    {
        // Match add_service: a rootless tree cannot carry a root decorator, and the
        // asset-route add used to report success while dropping it.
        if (!BT->RootNode)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString(TEXT("Behavior tree has no root composite; add_composite first")),
                                TEXT("NO_ROOT"));
            return true;
        }
        const bool bRoot = ParentNodeId.IsEmpty() || ParentNodeId.Equals(TEXT("root"), ESearchCase::IgnoreCase) ||
                           MatchesDecoratorTarget(BT->RootNode, ParentNodeId);
        FBTCompositeChild* Slot = bRoot ? nullptr : FindDecoratorSlot(BT->RootNode, ParentNodeId);
        if (!bRoot && !Slot)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Behavior Tree node not found: %s (pass a composite or task id from add_composite or add_task, or omit parentNodeId for a root decorator)"), *ParentNodeId),
                                TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        if (Slot)
        {
            Slot->Decorators.Add(NewDecorator);
        }
        else
        {
            BT->RootDecorators.Add(NewDecorator);
        }
        Result->SetStringField(TEXT("attachedTo"), bRoot ? TEXT("root") : ParentNodeId);
        BT->MarkPackageDirty();
        McpSafeAssetSave(BT);
        Result->SetStringField(TEXT("nodeId"), NewDecorator->GetName());
        Result->SetStringField(TEXT("decoratorType"), DecoratorType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Added %s decorator"), *DecoratorType));
        McpHandlerUtils::AddVerification(Result, BT);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Decorator added"), Result);
    }
    else
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to create decorator: %s"), *DecoratorType),
                            TEXT("CREATION_FAILED"));
    }

    return true;
}

bool HandleAddService(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
    FString ServiceType = GetJsonStringField(Payload, TEXT("serviceType"));

    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
    if (!BT)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    UBTService* NewService = nullptr;
    if (ServiceType.Equals(TEXT("DefaultFocus"), ESearchCase::IgnoreCase))
    {
        NewService = NewObject<UBTService_DefaultFocus>(BT);
    }
    else
    {
        UClass* ServiceClass = FindObject<UClass>(nullptr,
            *FString::Printf(TEXT("/Script/AIModule.BTService_%s"), *ServiceType));
        if (ServiceClass && ServiceClass->IsChildOf(UBTService::StaticClass()))
        {
            NewService = NewObject<UBTService>(BT, ServiceClass);
        }
    }

    if (NewService)
    {
        // Guard the root before building the graph: creating a BehaviorTree graph
        // for a rootless asset dereferences an empty array in BehaviorTreeEditor
        // (dogfood #63). A null root used to fall through and report success
        // while the service was silently dropped.
        if (!BT->RootNode)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString(TEXT("Behavior tree has no root composite; add_composite first")),
                                TEXT("NO_ROOT"));
            return true;
        }
        BT->RootNode->Services.Add(NewService);
        BT->MarkPackageDirty();
        McpSafeAssetSave(BT);
        Result->SetStringField(TEXT("nodeId"), NewService->GetName());
        Result->SetStringField(TEXT("serviceType"), ServiceType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Service %s created"), *ServiceType));
        McpHandlerUtils::AddVerification(Result, BT);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Service added"), Result);
    }
    else
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Failed to create service: %s"), *ServiceType),
                            TEXT("CREATION_FAILED"));
    }
    return true;
}
}
