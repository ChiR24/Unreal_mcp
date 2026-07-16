#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"

#if WITH_EDITOR

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsResponses.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"

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

    UEdGraphNode* Node = nullptr;
    if (bMake)
    {
        UK2Node_MakeStruct* MakeNode = NewObject<UK2Node_MakeStruct>(Graph);
        MakeNode->StructType = ScriptStruct;
        MakeNode->CreateNewGuid();
        Graph->AddNode(MakeNode);
        MakeNode->ReconstructNode();
        Node = MakeNode;
    }
    else
    {
        UK2Node_BreakStruct* BreakNode = NewObject<UK2Node_BreakStruct>(Graph);
        BreakNode->StructType = ScriptStruct;
        BreakNode->CreateNewGuid();
        Graph->AddNode(BreakNode);
        BreakNode->ReconstructNode();
        Node = BreakNode;
    }

    if (Node)
    {
        Node->NodePosX = FMath::RoundToInt(Pos.X);
        Node->NodePosY = FMath::RoundToInt(Pos.Y);
    }

    // Legacy pin names + new structured pin types.
    TArray<TSharedPtr<FJsonValue>> PinNames;
    TArray<TSharedPtr<FJsonValue>> PinTypes;
    if (Node)
    {
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
    }

    // Compile the Blueprint and capture diagnostics from the compiler results.
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);

    TArray<TSharedPtr<FJsonValue>> Diagnostics;
    for (const TSharedRef<FTokenizedMessage>& Msg : Results.Messages)
    {
        const EMessageSeverity::Type Severity = Msg->GetSeverity();
        if (Severity != EMessageSeverity::Error && Severity != EMessageSeverity::Warning)
        {
            continue;
        }
        TSharedPtr<FJsonObject> Diag = MakeShared<FJsonObject>();
        Diag->SetStringField(TEXT("severity"),
            Severity == EMessageSeverity::Error ? TEXT("Error") : TEXT("Warning"));
        Diag->SetStringField(TEXT("message"), Msg->ToText().ToString());
        Diagnostics.Add(MakeShared<FJsonValueObject>(Diag));
    }

    const TCHAR* StatusName = TEXT("Unknown");
    switch (BP->Status)
    {
        case BS_UpToDate:             StatusName = TEXT("UpToDate"); break;
        case BS_UpToDateWithWarnings: StatusName = TEXT("UpToDateWithWarnings"); break;
        case BS_Error:                StatusName = TEXT("Error"); break;
        case BS_Dirty:                StatusName = TEXT("Dirty"); break;
        case BS_BeingCreated:         StatusName = TEXT("BeingCreated"); break;
        default: break;
    }

    OutResult->SetStringField(TEXT("nodeGuid"), Node ? Node->NodeGuid.ToString() : FString());
    OutResult->SetStringField(TEXT("structPath"), ScriptStruct->GetPathName());
    OutResult->SetArrayField(TEXT("pinNames"), PinNames);
    OutResult->SetArrayField(TEXT("pinTypes"), PinTypes);
    OutResult->SetArrayField(TEXT("diagnostics"), Diagnostics);
    OutResult->SetStringField(TEXT("compilerStatus"), StatusName);
    OutResult->SetBoolField(TEXT("compiled"),
        BP->Status == BS_UpToDate || BP->Status == BS_UpToDateWithWarnings);

    if (Node)
    {
        McpHandlerUtils::AddVerification(OutResult, Node);
    }
}

} // namespace McpBlueprintUtils

#endif // WITH_EDITOR
