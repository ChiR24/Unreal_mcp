#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "EdGraphSchema_K2.h"

#include "Animation/AnimationAsset.h"
#include "AnimGraphNode_Base.h"

namespace McpBlueprintGraphHandlers
{
static TSharedPtr<FJsonObject> MakeDetailedPin(UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject =
        McpHandlerUtils::CreateResultObject();
    PinObject->SetStringField(
        TEXT("pinName"),
        Pin->PinName.ToString());
    PinObject->SetStringField(
        TEXT("direction"),
        Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
    PinObject->SetStringField(
        TEXT("pinType"),
        Pin->PinType.PinCategory.ToString());

    if ((Pin->PinType.PinCategory == TEXT("object") ||
         Pin->PinType.PinCategory == TEXT("class") ||
         Pin->PinType.PinCategory == TEXT("struct")) &&
        Pin->PinType.PinSubCategoryObject.IsValid())
    {
        PinObject->SetStringField(
            TEXT("pinSubType"),
            Pin->PinType.PinSubCategoryObject->GetName());
    }

    TArray<TSharedPtr<FJsonValue>> LinkedTo;
    for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
    {
        if (LinkedPin && LinkedPin->GetOwningNode())
        {
            TSharedPtr<FJsonObject> Link =
                McpHandlerUtils::CreateResultObject();
            Link->SetStringField(
                TEXT("nodeId"),
                LinkedPin->GetOwningNode()->NodeGuid.ToString());
            Link->SetStringField(
                TEXT("pinName"),
                LinkedPin->PinName.ToString());
            // The far node's title, so a chain reads without a call per hop.
            Link->SetStringField(
                TEXT("nodeTitle"),
                LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString());
            LinkedTo.Add(MakeShared<FJsonValueObject>(Link));
        }
    }
    PinObject->SetArrayField(TEXT("linkedTo"), LinkedTo);

    if (!Pin->DefaultValue.IsEmpty())
    {
        PinObject->SetStringField(
            TEXT("defaultValue"),
            Pin->DefaultValue);
    }
    else if (!Pin->DefaultTextValue.IsEmptyOrWhitespace())
    {
        PinObject->SetStringField(
            TEXT("defaultTextValue"),
            Pin->DefaultTextValue.ToString());
    }
    else if (Pin->DefaultObject)
    {
        PinObject->SetStringField(
            TEXT("defaultObjectPath"),
            Pin->DefaultObject->GetPathName());
    }
    return PinObject;
}

// followExec: the nodes the exec wires leaving Start reach, depth-first (a Branch's
// then side before its else), each with its inputs as a value or "<- Title.Pin".
// Reading what an event does took one pin call per hop before.
static TArray<TSharedPtr<FJsonValue>> McpExecChain(UEdGraphNode* Start, int32 Limit)
{
    TArray<TSharedPtr<FJsonValue>> Chain;
    TSet<UEdGraphNode*> Seen = {Start};
    TFunction<void(UEdGraphNode*)> Walk = [&](UEdGraphNode* From)
    {
        for (UEdGraphPin* Out : From->Pins)
        {
            if (!Out || Out->Direction != EGPD_Output || Out->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                continue;
            }
            for (UEdGraphPin* Link : Out->LinkedTo)
            {
                UEdGraphNode* Next = Link ? Link->GetOwningNode() : nullptr;
                if (!Next || Chain.Num() >= Limit || Seen.Contains(Next))
                {
                    continue;
                }
                Seen.Add(Next);
                TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
                Entry->SetStringField(TEXT("nodeId"), Next->NodeGuid.ToString());
                Entry->SetStringField(TEXT("nodeTitle"), Next->GetNodeTitle(ENodeTitleType::ListView).ToString());
                Entry->SetStringField(TEXT("via"), From->GetNodeTitle(ENodeTitleType::ListView).ToString() + TEXT(".") + Out->PinName.ToString());
                TSharedPtr<FJsonObject> Inputs = McpHandlerUtils::CreateResultObject();
                for (UEdGraphPin* In : Next->Pins)
                {
                    // Hidden pins (a library call's self, LatentInfo, WorldContextObject) are plumbing, not inputs.
                    if (!In || In->bHidden || In->Direction != EGPD_Input || In->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                    {
                        continue;
                    }
                    const UEdGraphPin* Source = In->LinkedTo.Num() > 0 ? In->LinkedTo[0] : nullptr;
                    const FString Value = Source && Source->GetOwningNode()
                        ? TEXT("<- ") + Source->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString() + TEXT(".") + Source->PinName.ToString()
                        : !In->DefaultValue.IsEmpty() ? In->DefaultValue
                        : In->DefaultObject ? In->DefaultObject->GetPathName() : In->DefaultTextValue.ToString();
                    if (!Value.IsEmpty())
                    {
                        Inputs->SetStringField(In->PinName.ToString(), Value);
                    }
                }
                Entry->SetObjectField(TEXT("inputs"), Inputs);
                Chain.Add(MakeShared<FJsonValueObject>(Entry));
                Walk(Next);
            }
        }
    };
    Walk(Start);
    return Chain;
}

// An animation node's own settings, which its pins do not show: the asset an asset player plays (the Sequence of a
// Sequence Player) and what its embedded `Node` struct holds for it (play rate, loop, start position ...), as typed values.
static void AddAnimNodeSettings(UEdGraphNode* Node, const TSharedPtr<FJsonObject>& Result)
{
    const UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node);
    FStructProperty* NodeStruct = AnimNode ? CastField<FStructProperty>(Node->GetClass()->FindPropertyByName(TEXT("Node"))) : nullptr;
    if (!NodeStruct)
    {
        return;
    }
    if (const UAnimationAsset* Asset = AnimNode->GetAnimationAsset())
    {
        Result->SetStringField(TEXT("animationAsset"), Asset->GetPathName());
        Result->SetStringField(TEXT("animationAssetClass"), Asset->GetClass()->GetName());
    }
    void* Settings = NodeStruct->ContainerPtrToValuePtr<void>(Node);
    TSharedPtr<FJsonObject> Values = McpHandlerUtils::CreateResultObject();
    for (TFieldIterator<FProperty> It(NodeStruct->Struct); It && Values->Values.Num() < 24; ++It)
    {
        // The settings a caller can edit; a pose link or a runtime counter is not one, and a list stays out of a summary.
        if (It->HasAnyPropertyFlags(CPF_Edit) && !It->IsA<FArrayProperty>() && !It->IsA<FMapProperty>() && !It->IsA<FSetProperty>())
        {
            if (const TSharedPtr<FJsonValue> Value = McpPropertyReflection::ExportPropertyToJsonValue(Settings, *It))
            {
                Values->SetField(It->GetName(), Value);
            }
        }
    }
    if (Values->Values.Num() > 0)
    {
        Result->SetObjectField(TEXT("settings"), Values);
    }
}

static bool GetNodeDetails(FActionContext& Context)
{
    if (Context.SubAction != TEXT("get_node_details"))
    {
        return false;
    }

    // The contract takes nodeGuid in place of nodeId; reading only nodeId sent
    // a caller who followed it to "Could not find node ''".
    const FString NodeId = McpGetFirstStringField(Context.Payload, {TEXT("nodeId"), TEXT("nodeGuid")});
    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendNodeNotFound(NodeId);
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeName"), TargetNode->GetName());
    Result->SetStringField(
        TEXT("nodeTitle"),
        TargetNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
    Result->SetStringField(
        TEXT("nodeComment"),
        TargetNode->NodeComment);
    Result->SetNumberField(TEXT("x"), TargetNode->NodePosX);
    Result->SetNumberField(TEXT("y"), TargetNode->NodePosY);

    TArray<TSharedPtr<FJsonValue>> Pins;
    for (UEdGraphPin* Pin : TargetNode->Pins)
    {
        if (Pin)
        {
            Pins.Add(MakeShared<FJsonValueObject>(MakeDetailedPin(Pin)));
        }
    }
    Result->SetArrayField(TEXT("pins"), Pins);
    Result->SetStringField(TEXT("nodeId"), TargetNode->NodeGuid.ToString());
    AddAnimNodeSettings(TargetNode, Result);
    double FollowExec = 0.0;
    if (Context.Payload->TryGetNumberField(TEXT("followExec"), FollowExec) && FollowExec >= 1.0)
    {
        Result->SetArrayField(TEXT("chain"), McpExecChain(TargetNode, FMath::Min(static_cast<int32>(FollowExec), 50)));
    }
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Node details retrieved."), Result);
    return true;
}

static bool GetPinDetails(FActionContext& Context)
{
    if (Context.SubAction != TEXT("get_pin_details"))
    {
        return false;
    }

    const FString NodeId = McpGetFirstStringField(Context.Payload, {TEXT("nodeId"), TEXT("nodeGuid")});
    FString PinName;
    Context.Payload->TryGetStringField(TEXT("pinName"), PinName);
    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendNodeNotFound(NodeId);
        return true;
    }

    TArray<UEdGraphPin*> PinsToReport;
    if (!PinName.IsEmpty())
    {
        UEdGraphPin* Pin = Context.FindPin(TargetNode, PinName);
        if (!Pin)
        {
            Context.SendError(
            FString::Printf(TEXT("No pin named '%s'. Pins on this node: %s."),
                *PinName, *DescribeNodePins(TargetNode)),
            TEXT("PIN_NOT_FOUND"));
            return true;
        }
        PinsToReport.Add(Pin);
    }
    else
    {
        PinsToReport = TargetNode->Pins;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), TargetNode->NodeGuid.ToString());
    TArray<TSharedPtr<FJsonValue>> Pins;
    for (UEdGraphPin* Pin : PinsToReport)
    {
        if (Pin)
        {
            Pins.Add(MakeShared<FJsonValueObject>(MakeDetailedPin(Pin)));
        }
    }
    Result->SetArrayField(TEXT("pins"), Pins);
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Pin details retrieved."), Result);
    return true;
}

bool HandleNodeDetailAction(FActionContext& Context)
{
    return GetNodeDetails(Context) || GetPinDetails(Context);
}
}
