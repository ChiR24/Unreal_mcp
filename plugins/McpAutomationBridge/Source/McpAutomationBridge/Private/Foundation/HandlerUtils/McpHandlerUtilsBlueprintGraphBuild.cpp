#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"


#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsResponses.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeStruct.h"

namespace McpBlueprintUtils
{

void McpBuildStructMakeBreakNodes(UBlueprint* BP, UStruct* Struct, UEdGraph* Graph,
    const FVector2f& Pos, bool bMake, TSharedPtr<FJsonObject>& OutResult)
{
    OutResult = McpHandlerUtils::CreateResultObject();

    UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Struct);
    if (!BP || !Graph || !ScriptStruct)
    {
        OutResult->SetStringField(TEXT("error"),
            TEXT("Invalid arguments: BP, Graph or Struct is null / Struct is not a UScriptStruct"));
        OutResult->SetBoolField(TEXT("compiled"), false);
        return;
    }

    UK2Node_StructOperation* Node = bMake
        ? static_cast<UK2Node_StructOperation*>(NewObject<UK2Node_MakeStruct>(Graph))
        : NewObject<UK2Node_BreakStruct>(Graph);
    Node->StructType = ScriptStruct;
    Node->CreateNewGuid();
    Graph->AddNode(Node);
    Node->ReconstructNode();
    Node->NodePosX = FMath::RoundToInt(Pos.X);
    Node->NodePosY = FMath::RoundToInt(Pos.Y);

    // Legacy pin names + new structured pin types.
    TArray<TSharedPtr<FJsonValue>> PinNames;
    TArray<TSharedPtr<FJsonValue>> PinTypes;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->PinName.IsNone())
        {
            continue;
        }

        PinNames.Add(MakeShared<FJsonValueString>(Pin->PinName.ToString()));

        TSharedPtr<FJsonObject> PinInfo = MakeShared<FJsonObject>();
        PinInfo->SetStringField(TEXT("name"), Pin->PinName.ToString());
        PinInfo->SetStringField(TEXT("direction"),
            Pin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"));
        PinInfo->SetStringField(TEXT("type"), DescribePinType(Pin->PinType));
        PinInfo->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
        FString Sub = Pin->PinType.PinSubCategory.ToString();
        if (Pin->PinType.PinSubCategoryObject.IsValid())
        {
            Sub = Pin->PinType.PinSubCategoryObject->GetPathName();
        }
        PinInfo->SetStringField(TEXT("subCategory"), Sub);
        PinTypes.Add(MakeShared<FJsonValueObject>(PinInfo));
    }

    OutResult->SetStringField(TEXT("nodeGuid"), Node->NodeGuid.ToString());
    OutResult->SetStringField(TEXT("structPath"), ScriptStruct->GetPathName());
    OutResult->SetArrayField(TEXT("pinNames"), PinNames);
    OutResult->SetArrayField(TEXT("pinTypes"), PinTypes);
    // compiled / compilerStatus / errorCount / warningCount / diagnostics
    FString FirstError;
    McpCompileBlueprintWithDiagnostics(BP, OutResult, FirstError);
    McpHandlerUtils::AddVerification(OutResult, Node);
}

} // namespace McpBlueprintUtils

