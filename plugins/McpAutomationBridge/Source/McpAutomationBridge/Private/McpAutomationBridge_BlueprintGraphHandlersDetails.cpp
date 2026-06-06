#include "McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
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

static bool GetNodeDetails(FActionContext& Context)
{
    if (Context.SubAction != TEXT("get_node_details"))
    {
        return false;
    }

    FString NodeId;
    Context.Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendError(TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
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

    FString NodeId;
    FString PinName;
    Context.Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Context.Payload->TryGetStringField(TEXT("pinName"), PinName);
    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendError(TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    TArray<UEdGraphPin*> PinsToReport;
    if (!PinName.IsEmpty())
    {
        UEdGraphPin* Pin = Context.FindPin(TargetNode, PinName);
        if (!Pin)
        {
            Context.SendError(TEXT("Pin not found."), TEXT("PIN_NOT_FOUND"));
            return true;
        }
        PinsToReport.Add(Pin);
    }
    else
    {
        PinsToReport = TargetNode->Pins;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NodeId);
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
#else
namespace McpBlueprintGraphHandlers
{
bool HandleNodeDetailAction(FActionContext&)
{
    return false;
}
}
#endif
