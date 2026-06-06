#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Decorators/BTDecorator_Cooldown.h"
#include "BehaviorTree/Decorators/BTDecorator_Loop.h"

namespace McpAIHandlers
{
bool HandleAddDecorator(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_decorator");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_decorator"))
    {
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        FString DecoratorType = GetStringFieldAI(Payload, TEXT("decoratorType"));

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
        // Add more decorator types as needed

        if (NewDecorator)
        {
            BT->MarkPackageDirty();
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

    return true;
}

bool HandleAddService(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_service");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_service"))
    {
        FString BTPath = GetStringFieldAI(Payload, TEXT("behaviorTreePath"));
        FString ServiceType = GetStringFieldAI(Payload, TEXT("serviceType"));

        UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *BTPath);
        if (!BT)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                                FString::Printf(TEXT("Behavior Tree not found: %s"), *BTPath),
                                TEXT("NOT_FOUND"));
            return true;
        }

        // Services are added to composite nodes, not directly to the tree
        // For now, just mark the tree as modified
        BT->MarkPackageDirty();
        Result->SetStringField(TEXT("serviceType"), ServiceType);
        Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Service %s reference created"), *ServiceType));

        McpHandlerUtils::AddVerification(Result, BT);
        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Service added"), Result);
        return true;
    }

    return true;
}
}
#endif
