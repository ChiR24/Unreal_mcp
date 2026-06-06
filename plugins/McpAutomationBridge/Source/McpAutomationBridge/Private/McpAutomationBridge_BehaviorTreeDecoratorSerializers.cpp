#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BehaviorTreeSerializers.h"

#include "BehaviorTree/BTCompositeNode.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace McpBehaviorTreeSerializers
{

TArray<TSharedPtr<FJsonValue>> SerializeDecoratorOpsRaw(const TArray<FBTDecoratorLogic>& Ops)
{
    TArray<TSharedPtr<FJsonValue>> Out;
    Out.Reserve(Ops.Num());
    for (const FBTDecoratorLogic& Op : Ops)
    {
        TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        FString OpName;
        switch (Op.Operation.GetValue())
        {
            case EBTDecoratorLogic::Test: OpName = TEXT("Test"); break;
            case EBTDecoratorLogic::And:  OpName = TEXT("And");  break;
            case EBTDecoratorLogic::Or:   OpName = TEXT("Or");   break;
            case EBTDecoratorLogic::Not:  OpName = TEXT("Not");  break;
            default:                      OpName = TEXT("Invalid"); break;
        }
        Entry->SetStringField(TEXT("op"), OpName);
        // Test.number indexes the SOURCE decorator array (Child.Decorators / RootDecorators),
        // NOT the emitted decorators array — SerializeBTNode skips null decorator slots, so a
        // consumer correlating ops to emitted decorators by position must account for dropped
        // nulls (PR1b will parse these ops; this raw form preserves the engine's source indices).
        Entry->SetNumberField(TEXT("number"), Op.Number);   // Test: decorator index; And/Or/Not: operand count
        Out.Add(MakeShared<FJsonValueObject>(Entry));
    }
    return Out;
}


}
