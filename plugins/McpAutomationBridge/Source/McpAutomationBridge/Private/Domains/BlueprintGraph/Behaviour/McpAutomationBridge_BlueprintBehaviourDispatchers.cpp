// A recipe's dispatcher that already exists on the Blueprint: its signature against the declaration.
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "EdGraphSchema_K2.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"
#include "K2Node_FunctionEntry.h"

namespace McpBlueprintBehaviour::Detail
{
// Empty when the Blueprint's dispatcher Name has exactly the recipe's parameters (names, then
// types; Float and double count as the same, as for variables), else what differs.
FString DispatcherSignatureMismatch(UBlueprint* Blueprint, const FString& Name, const TSharedPtr<FJsonObject>& Declared)
{
    const TObjectPtr<UEdGraph>* Graph = Blueprint->DelegateSignatureGraphs.FindByPredicate(
        [&Name](const TObjectPtr<UEdGraph>& Candidate) { return Candidate && Candidate->GetFName() == FName(*Name); });
    const UK2Node_FunctionEntry* Entry = nullptr;
    for (int32 Node = 0; Graph && !Entry && Node < (*Graph)->Nodes.Num(); ++Node)
    {
        Entry = Cast<UK2Node_FunctionEntry>((*Graph)->Nodes[Node]);
    }
    const TArray<TSharedPtr<FJsonValue>>* Params = nullptr;
    const int32 Want = Declared->TryGetArrayField(TEXT("parameters"), Params) ? Params->Num() : 0;
    if (!Entry || Entry->UserDefinedPins.Num() != Want)
    {
        return FString::Printf(TEXT("dispatcher %s already exists with %d parameter(s); the recipe declares %d."),
                               *Name, Entry ? Entry->UserDefinedPins.Num() : 0, Want);
    }
    auto Loose = [](FEdGraphPinType Type)
    {
        Type.PinSubCategory = Type.PinCategory == UEdGraphSchema_K2::PC_Real ? FName() : Type.PinSubCategory;
        return Type;
    };
    for (int32 Index = 0; Index < Want; ++Index)
    {
        const TSharedPtr<FJsonObject> Param = (*Params)[Index]->AsObject();
        const FUserPinInfo& Have = *Entry->UserDefinedPins[Index];
        const McpBlueprintUtils::FTypeResolutionResult Type = McpBlueprintUtils::ResolvePinType(GetJsonStringField(Param, TEXT("type")));
        if (Have.PinName != FName(*GetJsonStringField(Param, TEXT("name"))) || Loose(Have.PinType) != Loose(Type.PinType))
        {
            return FString::Printf(TEXT("dispatcher %s already exists and its parameter %d is %s %s; the recipe declares %s %s."),
                                   *Name, Index, *McpBlueprintUtils::DescribePinType(Have.PinType), *Have.PinName.ToString(),
                                   *GetJsonStringField(Param, TEXT("type")), *GetJsonStringField(Param, TEXT("name")));
        }
    }
    return FString();
}
}
