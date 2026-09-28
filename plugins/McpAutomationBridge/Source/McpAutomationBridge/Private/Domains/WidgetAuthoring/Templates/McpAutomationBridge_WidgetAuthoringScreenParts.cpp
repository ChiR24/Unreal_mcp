#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringScreens.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringValidation.h"
#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringScreens
{
using namespace WidgetAuthoringHelpers;

TSharedPtr<FJsonObject> Node(const TCHAR* Type, const FString& Name, const FString& Text)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("type"), Type);
    Result->SetStringField(TEXT("name"), Name);
    if (!Text.IsEmpty())
    {
        Result->SetStringField(TEXT("text"), Text);
    }
    return Result;
}

TSharedPtr<FJsonObject> Append(const TSharedPtr<FJsonObject>& Parent, const TSharedPtr<FJsonObject>& Child)
{
    TArray<TSharedPtr<FJsonValue>> Children;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Parent->TryGetArrayField(TEXT("children"), Existing))
    {
        Children = *Existing;
    }
    Children.Add(MakeShared<FJsonValueObject>(Child));
    Parent->SetArrayField(TEXT("children"), Children);
    return Child;
}

TSharedPtr<FJsonObject> SetSlot(const TSharedPtr<FJsonObject>& Target, const TCHAR* Json)
{
    Target->SetObjectField(TEXT("slot"), McpParseWidgetSpec(Json));
    return Target;
}

TSharedPtr<FJsonObject> Numbers(const TSharedPtr<FJsonObject>& Target, const TCHAR* Field, std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    for (const double Value : Values)
    {
        Array.Add(MakeShared<FJsonValueNumber>(Value));
    }
    Target->SetArrayField(Field, Array);
    return Target;
}

TSharedPtr<FJsonObject> Button(const FString& Name, const FString& Label, double FontSize)
{
    TSharedPtr<FJsonObject> Result = Numbers(Node(TEXT("Button"), Name), TEXT("color"), { 0.08, 0.08, 0.11, 0.92 });
    Result->SetNumberField(TEXT("radius"), 8);
    SetSlot(Result, TEXT(R"({"padding":[0,6,0,6],"hAlign":"fill"})"));
    TSharedPtr<FJsonObject> Text = Append(Result, Node(TEXT("TextBlock"), Name + TEXT("_Text"), Label));
    Text->SetNumberField(TEXT("fontSize"), FontSize);
    Text->SetStringField(TEXT("justify"), TEXT("center"));
    return Result;
}

FString NameStem(const FString& Label)
{
    FString Stem;
    bool bUpper = true;
    for (const TCHAR Char : Label)
    {
        if (!FChar::IsAlnum(Char))
        {
            bUpper = true;
            continue;
        }
        Stem.AppendChar(bUpper ? FChar::ToUpper(Char) : Char);
        bUpper = false;
    }
    return Stem.IsEmpty() ? TEXT("Item") : Stem;
}

TSharedPtr<FJsonObject> RootWithBackdrop(double Alpha)
{
    TSharedPtr<FJsonObject> Root = Node(TEXT("CanvasPanel"), TEXT("RootCanvas"));
    TSharedPtr<FJsonObject> Backdrop = Append(Root, Numbers(Node(TEXT("Image"), TEXT("Background")), TEXT("color"), { 0.02, 0.02, 0.04, Alpha }));
    SetSlot(Backdrop, TEXT(R"({"anchors":[0,0,1,1],"offsets":[0,0,0,0]})"));
    return Root;
}

}

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

// create_widget_template: a NEW Widget Blueprint holding a ready-made screen. The spec is
// built before the asset exists, so a bad knob refuses without leaving an empty asset.
bool HandleWidgetAuthoringScreens(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    using namespace WidgetAuthoringScreens;
    const FScreenTemplate* Screen = MenuScreens().FindByPredicate([&SubAction](const FScreenTemplate& Candidate)
        { return SubAction.Equals(Candidate.Action, ESearchCase::IgnoreCase); });
    if (!Screen)
    {
        Screen = PanelScreens().FindByPredicate([&SubAction](const FScreenTemplate& Candidate)
            { return SubAction.Equals(Candidate.Action, ESearchCase::IgnoreCase); });
    }
    if (!Screen)
    {
        return false;
    }
    FString Error;
    const TSharedPtr<FJsonObject> Spec = Screen->Build(Payload, Error);
    if (!Spec.IsValid())
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UWidgetBlueprint* WidgetBP = McpCreateTemplateWidgetBlueprint(Subsystem, RequestId, RequestingSocket, Payload, Screen->DefaultName);
    if (!WidgetBP)
    {
        return true;
    }
    TArray<UWidget*> Created;
    UWidget* Root = McpBuildWidgetSpec(WidgetBP, Spec, FString(), Created, Error);
    if (!Root)
    {
        McpRollbackWidgetSpec(WidgetBP, Created);
        MarkWidgetBlueprintModifiedAndSave(WidgetBP);
        Subsystem.SendAutomationError(RequestingSocket, RequestId, FString::Printf(
            TEXT("The %s template did not build (%s); '%s' was created empty."), Screen->Label, *Error, *WidgetBP->GetPathName()),
            TEXT("BUILD_FAILED"));
        return true;
    }
    WidgetBP->WidgetTree->RootWidget = Root;
    if (Screen->Finish)
    {
        Screen->Finish(WidgetBP, Payload, ResultJson);
    }
    const bool bCompiled = McpSafeCompileBlueprint(WidgetBP);
    const bool bSaved = MarkWidgetBlueprintModifiedAndSave(WidgetBP);
    if (!ValidateWidgetCreation(WidgetBP, Root->GetName(), Error))
    {
        Subsystem.SendAutomationError(RequestingSocket, RequestId, Error, TEXT("ENGINE_ERROR"));
        return true;
    }
    TArray<TSharedPtr<FJsonValue>> Buttons;
    for (const UWidget* Widget : Created)
    {
        if (Widget->IsA<UButton>())
        {
            Buttons.Add(MakeShared<FJsonValueString>(Widget->GetName()));
        }
    }
    const FString Message = FString::Printf(TEXT("Created %s '%s' (%d widgets)"), Screen->Label, *WidgetBP->GetPathName(), Created.Num());
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
    ResultJson->SetNumberField(TEXT("widgetCount"), Created.Num());
    ResultJson->SetArrayField(TEXT("buttons"), Buttons);
    ResultJson->SetBoolField(TEXT("compiled"), bCompiled);
    ResultJson->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(ResultJson, WidgetBP);
    Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, Message, ResultJson);
    return true;
}
}
