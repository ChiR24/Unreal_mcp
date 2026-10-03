#include "Domains/BlueprintGraph/PinMutations/McpAutomationBridge_BlueprintGraphPinLinkReport.h"

#include "Dom/JsonValue.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

void McpReportPinLinkChanges(const TSharedPtr<FJsonObject>& Result, const UEdGraphPin* FromPin, const UEdGraphPin* ToPin,
                             const TArray<UEdGraphPin*>& FromBefore, const TArray<UEdGraphPin*>& ToBefore)
{
    TArray<TSharedPtr<FJsonValue>> Replaced;
    auto NoteDropped = [&Replaced](const UEdGraphPin* Pin, const TArray<UEdGraphPin*>& Before)
    {
        for (UEdGraphPin* Old : Before)
        {
            UEdGraphNode* OldNode = Old ? Old->GetOwningNodeUnchecked() : nullptr;
            if (OldNode && !Pin->LinkedTo.Contains(Old))
            {
                TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
                Link->SetStringField(TEXT("nodeId"), OldNode->NodeGuid.ToString());
                Link->SetStringField(TEXT("nodeTitle"), OldNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
                Link->SetStringField(TEXT("pinName"), Old->GetName());
                Replaced.Add(MakeShared<FJsonValueObject>(Link));
            }
        }
    };
    NoteDropped(FromPin, FromBefore);
    NoteDropped(ToPin, ToBefore);
    if (Replaced.Num() > 0)
    {
        Result->SetArrayField(TEXT("replacedLinks"), Replaced);
    }
    // A Target (self) pin keeps every object wired into it: wiring in a new one kept the old one without a word,
    // and two components then took the same transform. Several wires into an exec input are ordinary.
    const UEdGraphPin* InputPin = ToPin->Direction == EGPD_Input ? ToPin : FromPin;
    if (InputPin->LinkedTo.Num() > 1 && InputPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
    {
        TArray<TSharedPtr<FJsonValue>> Warnings;
        Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
            TEXT("%s on '%s' now holds %d links and the call runs on each; to replace the old one, break this pin's links first (delete_node deleteScope pin_links), then connect."),
            *InputPin->GetName(), *InputPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString(),
            InputPin->LinkedTo.Num())));
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
}
