#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Decorators/BTDecorator_Cooldown.h"
#include "BehaviorTree/Decorators/BTDecorator_Loop.h"
#include "BehaviorTree/Services/BTService_DefaultFocus.h"
#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

namespace McpAIHandlers
{
bool HandleAddDecorator(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_decorator");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_decorator"))
    {
        FString BTPath = GetJsonStringField(Payload, TEXT("behaviorTreePath"));
        FString DecoratorType = GetJsonStringField(Payload, TEXT("decoratorType"));

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

        if (NewDecorator)
        {
            UEdGraph* Graph = nullptr;
            McpBehaviorTreeHandlers::EnsureBehaviorTreeGraph(BT, Graph);
            BT->RootDecorators.Add(NewDecorator);
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

    return true;
}

bool HandleAddService(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("add_service");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("add_service"))
    {
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
            UEdGraph* Graph = nullptr;
            McpBehaviorTreeHandlers::EnsureBehaviorTreeGraph(BT, Graph);
            if (BT->RootNode)
            {
                BT->RootNode->Services.Add(NewService);
            }
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

    return true;
}
}
#endif
