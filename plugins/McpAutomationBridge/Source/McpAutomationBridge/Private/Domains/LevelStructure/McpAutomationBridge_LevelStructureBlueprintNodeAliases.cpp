// McpAutomationBridge_LevelStructureBlueprintNodeAliases.cpp — friendly node names for add_level_blueprint_node.
//
// Dogfood #160/#164: callers reach for "EventBeginPlay" or "PrintString"; only raw
// K2Node_* class names used to resolve, and the created event/function nodes carried
// no binding at all. The aliases below map to a node class plus the member to bind.
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"

#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Kismet/KismetSystemLibrary.h"

#if WITH_EDITOR
namespace McpLevelStructure
{
bool ResolveLevelBlueprintNodeAlias(FString& InOutNodeClass, FString& OutEventName, FString& OutFunctionName)
{
    FString Key = InOutNodeClass.ToLower();
    Key.ReplaceInline(TEXT("_"), TEXT(""));
    Key.ReplaceInline(TEXT(" "), TEXT(""));
    OutEventName.Reset();
    OutFunctionName.Reset();
    struct FAlias { const TCHAR* Key; const TCHAR* NodeClass; const TCHAR* Event; const TCHAR* Function; };
    static const FAlias Aliases[] = {
        {TEXT("eventbeginplay"), TEXT("K2Node_Event"), TEXT("ReceiveBeginPlay"), nullptr},
        {TEXT("beginplay"), TEXT("K2Node_Event"), TEXT("ReceiveBeginPlay"), nullptr},
        {TEXT("receivebeginplay"), TEXT("K2Node_Event"), TEXT("ReceiveBeginPlay"), nullptr},
        {TEXT("eventtick"), TEXT("K2Node_Event"), TEXT("ReceiveTick"), nullptr},
        {TEXT("tick"), TEXT("K2Node_Event"), TEXT("ReceiveTick"), nullptr},
        {TEXT("receivetick"), TEXT("K2Node_Event"), TEXT("ReceiveTick"), nullptr},
        {TEXT("eventendplay"), TEXT("K2Node_Event"), TEXT("ReceiveEndPlay"), nullptr},
        {TEXT("endplay"), TEXT("K2Node_Event"), TEXT("ReceiveEndPlay"), nullptr},
        {TEXT("printstring"), TEXT("K2Node_CallFunction"), nullptr, TEXT("PrintString")},
        {TEXT("print"), TEXT("K2Node_CallFunction"), nullptr, TEXT("PrintString")},
        {TEXT("printtext"), TEXT("K2Node_CallFunction"), nullptr, TEXT("PrintText")},
        {TEXT("delay"), TEXT("K2Node_CallFunction"), nullptr, TEXT("Delay")},
    };
    for (const FAlias& Alias : Aliases)
    {
        if (Key == Alias.Key)
        {
            InOutNodeClass = Alias.NodeClass;
            OutEventName = Alias.Event ? Alias.Event : TEXT("");
            OutFunctionName = Alias.Function ? Alias.Function : TEXT("");
            return true;
        }
    }
    return false;
}

bool ApplyLevelBlueprintNodeAlias(UK2Node* Node, const FString& EventName, const FString& FunctionName, FString& OutError)
{
    if (!EventName.IsEmpty())
    {
        UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
        if (!EventNode)
        {
            OutError = FString::Printf(TEXT("Alias event %s needs a K2Node_Event node"), *EventName);
            return false;
        }
        EventNode->EventReference.SetExternalMember(FName(*EventName), AActor::StaticClass());
        EventNode->bOverrideFunction = true;
        return true;
    }
    if (!FunctionName.IsEmpty())
    {
        UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);
        UFunction* Function = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(*FunctionName));
        if (!CallNode || !Function)
        {
            OutError = FString::Printf(TEXT("Alias function %s could not be bound (K2Node_CallFunction / KismetSystemLibrary)"), *FunctionName);
            return false;
        }
        CallNode->SetFromFunction(Function);
        return true;
    }
    return true;
}
}
#endif
