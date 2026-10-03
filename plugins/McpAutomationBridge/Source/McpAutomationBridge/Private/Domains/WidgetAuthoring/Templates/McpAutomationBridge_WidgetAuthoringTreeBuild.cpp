#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringValidation.h"
#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"

#include "Blueprint/WidgetTree.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

// add_widget_tree: a whole layout (panels, borders, text, images and their slots) built from one
// nested description. A HUD used to cost an add_* call per widget plus a set_style call per look.
namespace WidgetAuthoringHelpers
{
namespace
{
constexpr int32 MaxWidgetTreeNodes = 200;

const TArray<FString>& WidgetTreeNodeKeys()
{
    static const TArray<FString> Keys = {
        TEXT("type"), TEXT("name"), TEXT("children"), TEXT("slot"), TEXT("text"), TEXT("fontSize"), TEXT("color"),
        TEXT("justify"), TEXT("autoWrap"), TEXT("percent"), TEXT("visibility"), TEXT("opacity"), TEXT("padding"),
        TEXT("radius"), TEXT("imageSize"), TEXT("value"), TEXT("checked"), TEXT("width"), TEXT("height"),
        TEXT("maxHeight"), TEXT("options"), TEXT("selected"), TEXT("foreground"), TEXT("slotPadding"),
        TEXT("minSlotSize"), TEXT("texture"), TEXT("typeface"), TEXT("fontFamily"), TEXT("letterSpacing"),
        TEXT("copyStyleFrom"), TEXT("outline"), TEXT("outlineColor"), TEXT("outlineWidth"), TEXT("shadowOffset"),
        TEXT("shadowColor"), TEXT("material")};
    return Keys;
}

const TArray<FString>& WidgetTreeSlotKeys()
{
    static const TArray<FString> Keys = {
        TEXT("anchors"), TEXT("alignment"), TEXT("position"), TEXT("size"), TEXT("offsets"), TEXT("autoSize"),
        TEXT("z"), TEXT("padding"), TEXT("hAlign"), TEXT("vAlign"), TEXT("fill"), TEXT("row"), TEXT("column")};
    return Keys;
}

FString WidgetTreeUnknownKey(const TSharedPtr<FJsonObject>& Object, const TArray<FString>& Known)
{
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
    {
        if (!Known.Contains(Pair.Key))
        {
            return Pair.Key;
        }
    }
    return FString();
}
}

FString McpValidateWidgetTreeSpec(const TSharedPtr<FJsonObject>& Node, const FString& Where, int32& InOutCount)
{
    if (!Node.IsValid())
    {
        return FString::Printf(TEXT("%s is not an object {type, name, children[], slot{}, ...}"), *Where);
    }
    if (++InOutCount > MaxWidgetTreeNodes)
    {
        return FString::Printf(TEXT("the tree holds more than %d widgets; build it in parts (parentSlot)"), MaxWidgetTreeNodes);
    }
    const FString Unknown = WidgetTreeUnknownKey(Node, WidgetTreeNodeKeys());
    if (!Unknown.IsEmpty())
    {
        return FString::Printf(TEXT("%s has an unknown field '%s'; a node takes %s"), *Where, *Unknown,
                               *FString::Join(WidgetTreeNodeKeys(), TEXT(", ")));
    }
    if (GetJsonStringField(Node, TEXT("type")).IsEmpty() || GetJsonStringField(Node, TEXT("name")).IsEmpty())
    {
        return FString::Printf(TEXT("%s needs a type (a UMG class short name: CanvasPanel, Border, VerticalBox, ")
                               TEXT("HorizontalBox, Overlay, TextBlock, Image, ProgressBar ...) and a name"), *Where);
    }
    const TSharedPtr<FJsonObject>* SlotSpec = nullptr;
    const FString SlotUnknown = Node->TryGetObjectField(TEXT("slot"), SlotSpec) ? WidgetTreeUnknownKey(*SlotSpec, WidgetTreeSlotKeys()) : FString();
    if (!SlotUnknown.IsEmpty())
    {
        return FString::Printf(TEXT("%s slot has an unknown field '%s'; a slot takes %s"), *Where, *SlotUnknown,
                               *FString::Join(WidgetTreeSlotKeys(), TEXT(", ")));
    }
    const FString Texture = GetJsonStringField(Node, TEXT("texture"));
    if (!Texture.IsEmpty() && !McpLoadSpecTexture(Texture))
    {
        return FString::Printf(TEXT("%s texture '%s' does not load as a Texture2D"), *Where, *Texture);
    }
    const FString Material = GetJsonStringField(Node, TEXT("material"));
    const FString Type = GetJsonStringField(Node, TEXT("type"));
    if (!Material.IsEmpty() && Type != TEXT("Image") && Type != TEXT("Border"))
    {
        return FString::Printf(TEXT("%s material is the brush of an Image or a Border, not of a %s"), *Where, *Type);
    }
    if (!Material.IsEmpty() && !McpLoadSpecMaterial(Material))
    {
        return FString::Printf(TEXT("%s material '%s' does not load as a material or material instance"), *Where, *Material);
    }
    const TArray<TSharedPtr<FJsonValue>>* Children = nullptr;
    if (!Node->TryGetArrayField(TEXT("children"), Children))
    {
        return Node->HasField(TEXT("children"))
            ? FString::Printf(TEXT("%s children must be an array of nodes"), *Where) : FString();
    }
    for (int32 Index = 0; Index < Children->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Child = nullptr;
        const bool bObject = (*Children)[Index].IsValid() && (*Children)[Index]->TryGetObject(Child);
        const FString ChildWhere = FString::Printf(TEXT("%s.children[%d]"), *Where, Index);
        const FString Error = McpValidateWidgetTreeSpec(bObject ? *Child : nullptr, ChildWhere, InOutCount);
        if (!Error.IsEmpty())
        {
            return Error;
        }
    }
    return FString();
}
}

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringTreeBuild(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                                    const FString& SubAction, const TSharedPtr<FJsonObject>& Payload,
                                    TSharedPtr<FMcpBridgeWebSocket> Socket, TSharedPtr<FJsonObject> ResultJson)
{
    if (!SubAction.Equals(TEXT("add_widget_tree"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    const TSharedPtr<FJsonObject>* TreeObject = nullptr;
    if (!Payload->TryGetObjectField(TEXT("tree"), TreeObject) || !TreeObject->IsValid())
    {
        Subsystem.SendAutomationError(Socket, RequestId,
            TEXT("tree is required: the root node {type, name, children[], slot{}, ...widget props}."), TEXT("MISSING_PARAMETER"));
        return true;
    }
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    UWidgetBlueprint* WidgetBP = WidgetPath.IsEmpty() ? nullptr : LoadWidgetBlueprint(WidgetPath);
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        Subsystem.SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("No Widget Blueprint at '%s'."), *WidgetPath), TEXT("NOT_FOUND"));
        return true;
    }
    int32 Count = 0;
    const FString SpecError = McpValidateWidgetTreeSpec(*TreeObject, TEXT("tree"), Count);
    if (!SpecError.IsEmpty())
    {
        Subsystem.SendAutomationError(Socket, RequestId, SpecError + TEXT(". Nothing was added."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const FString RootName = GetJsonStringField(*TreeObject, TEXT("name"));
    TArray<UWidget*> Created;
    if (!McpAddSpecToWidget(Subsystem, RequestId, Socket, Payload, WidgetBP, *TreeObject, RootName, Created))
    {
        return true;
    }
    const bool bSaved = MarkWidgetBlueprintModifiedAndSave(WidgetBP);
    FString ValidationError;
    if (!ValidateWidgetCreation(WidgetBP, RootName, ValidationError))
    {
        McpRollbackWidgetSpec(WidgetBP, Created);
        MarkWidgetBlueprintModifiedAndSave(WidgetBP);
        Subsystem.SendAutomationError(Socket, RequestId, ValidationError + TEXT(" The tree was removed again."), TEXT("ENGINE_ERROR"));
        return true;
    }
    RefreshWidgetBlueprintClass(WidgetBP); // every widget is a variable: give the generated class their properties
    TArray<TSharedPtr<FJsonValue>> Names;
    for (const UWidget* Widget : Created)
    {
        Names.Add(MakeShared<FJsonValueString>(Widget->GetName()));
    }
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));
    ResultJson->SetStringField(TEXT("slotName"), RootName);
    ResultJson->SetArrayField(TEXT("widgets"), Names);
    ResultJson->SetNumberField(TEXT("widgetCount"), Created.Num());
    ResultJson->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
    Subsystem.SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("Added widget tree '%s' (%d widgets)"), *RootName, Created.Num()), ResultJson);
    return true;
}
}
