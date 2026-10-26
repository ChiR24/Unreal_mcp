// manage_blueprint arrange_widgets (see McpWidgetArrange.h): the widgets the relations name, children of one Canvas
// Panel, solved at a reference screen size, written to their slots as one undo step, and checked again at each size.
#include "Domains/WidgetAuthoring/Arrange/McpWidgetArrange.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Domains/WidgetAuthoring/Audit/McpWidgetAudit.h"
#include "Domains/WidgetAuthoring/McpWidgetActions.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetBPLoading.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

namespace
{
constexpr int32 ArrangeMaxRelations = 64;
constexpr double ArrangeTolerance = 1.0; // Slate units: rounding at a size

// The boxes of Names in their panel, and the panel's size, in Slate units, as the widget lays out at Size; MinSize is
// what a text needs (its desired size), 0 for the rest.
bool ArrangeMeasure(UWidgetBlueprint* WidgetBP, FIntPoint Size, const FString& PanelName, const TArray<FString>& Names,
                    TArray<McpWidgetArrange::FArrangeBox>& OutBoxes, TArray<FVector2D>& OutMinSize, FVector2D& OutPanel, FString& OutError)
{
    TArray<McpWidgetAudit::FItem> Items;
    float Scale = 1.f;
    TArray<FColor> Pixels;
    if (!McpDrawWidget(WidgetBP, Size, Pixels, OutError, [&Items, &Scale](UUserWidget& Widget, float InScale)
                       { Items = McpWidgetAudit::Collect(Widget); Scale = InScale; }))
    {
        return false;
    }
    const auto Named = [&Items](const FString& Name) { return Items.FindByPredicate([&Name](const McpWidgetAudit::FItem& Item) { return Item.Name == Name; }); };
    const McpWidgetAudit::FItem* Panel = Named(PanelName);
    if (!Panel || Scale <= 0.f)
    {
        OutError = FString::Printf(TEXT("The Canvas Panel '%s' is not shown at %dx%d: a hidden panel cannot be arranged."), *PanelName, Size.X, Size.Y);
        return false;
    }
    OutPanel = Panel->Local;
    OutBoxes.Reset();
    OutMinSize.Reset();
    for (const FString& Name : Names)
    {
        const McpWidgetAudit::FItem* Item = Named(Name);
        McpWidgetArrange::FArrangeBox Box; // a hidden widget starts at the panel's corner
        if (Item)
        {
            Box.Left = (Item->Rect.Min.X - Panel->Rect.Min.X) / Scale;
            Box.Top = (Item->Rect.Min.Y - Panel->Rect.Min.Y) / Scale;
            Box.Width = Item->Local.X;
            Box.Height = Item->Local.Y;
        }
        OutBoxes.Add(Box);
        // A text needs its desired size, and 2 units to spare: it measures a unit wider at another DPI scale.
        OutMinSize.Add(Item && Item->Kind == McpWidgetAudit::EKind::Text ? Item->Desired + FVector2D(2.0, 2.0) : FVector2D::ZeroVector);
    }
    return true;
}

const TCHAR* ArrangeAnchorName(McpWidgetArrange::EAnchor Anchor, bool bX)
{
    switch (Anchor)
    {
    case McpWidgetArrange::EAnchor::Start: return bX ? TEXT("left") : TEXT("top");
    case McpWidgetArrange::EAnchor::Middle: return bX ? TEXT("centre") : TEXT("middle");
    case McpWidgetArrange::EAnchor::End: return bX ? TEXT("right") : TEXT("bottom");
    default: return TEXT("stretch");
    }
}

TSharedPtr<FJsonObject> ArrangeAuditCall(const FString& WidgetPath, const TArray<FIntPoint>& Sizes)
{
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("widgetPath"), WidgetPath);
    TArray<TSharedPtr<FJsonValue>> SizeJson;
    for (const FIntPoint& Size : Sizes)
    {
        SizeJson.Add(MakeShared<FJsonValueString>(FString::Printf(TEXT("%dx%d"), Size.X, Size.Y)));
    }
    Params->SetArrayField(TEXT("resolutions"), SizeJson);
    TSharedPtr<FJsonObject> Call = MakeShared<FJsonObject>();
    Call->SetStringField(TEXT("operation"), TEXT("execute"));
    Call->SetStringField(TEXT("tool"), TEXT("manage_blueprint"));
    Call->SetStringField(TEXT("action"), TEXT("audit_widget"));
    Call->SetObjectField(TEXT("params"), Params);
    return Call;
}
} // namespace

bool HandleWidgetAuthoringArrange(UEngineWireSubsystem& Subsystem, const FString& RequestId, const FString& SubAction,
                                  const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                  TSharedPtr<FJsonObject> ResultJson)
{
    if (!SubAction.Equals(TEXT("arrange_widgets"), ESearchCase::IgnoreCase))
    {
        return false;
    }
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Subsystem.SendAutomationError(Socket, RequestId, Message, Code);
        return true;
    };
    const FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
    UWidgetBlueprint* WidgetBP = WidgetPath.IsEmpty() ? nullptr : LoadWidgetBlueprint(WidgetPath);
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        return Refuse(TEXT("No Widget Blueprint at widgetPath."), TEXT("NOT_FOUND"));
    }
    const TArray<TSharedPtr<FJsonValue>>* RelationJson = nullptr;
    TArray<FString> Names;
    TArray<McpWidgetArrange::FRelation> Relations;
    TArray<FIntPoint> Reference;
    TArray<FIntPoint> Sizes;
    FString Error = FString::Printf(TEXT("relations takes 1 to %d relations."), ArrangeMaxRelations);
    const TArray<TSharedPtr<FJsonValue>> ReferenceJson = {MakeShared<FJsonValueString>(GetJsonStringField(Payload, TEXT("resolution"), TEXT("1920x1080")))};
    const TArray<TSharedPtr<FJsonValue>>* SizeJson = nullptr;
    Payload->TryGetArrayField(TEXT("resolutions"), SizeJson);
    if (!Payload->TryGetArrayField(TEXT("relations"), RelationJson) || !RelationJson || RelationJson->Num() == 0 ||
        RelationJson->Num() > ArrangeMaxRelations || !McpWidgetArrange::ParseRelations(*RelationJson, Names, Relations, Error) ||
        !McpWidgetAudit::ParseSizes(&ReferenceJson, TEXT("resolution"), Reference, Error) ||
        !McpWidgetAudit::ParseSizes(SizeJson, TEXT("resolutions"), Sizes, Error))
    {
        return Refuse(Error, TEXT("INVALID_ARGUMENT"));
    }
    // Every widget named sits in the same Canvas Panel.
    TArray<UCanvasPanelSlot*> Slots;
    UCanvasPanel* Canvas = nullptr;
    for (const FString& Name : Names)
    {
        UWidget* Widget = FindWidgetByName(WidgetBP->WidgetTree, Name);
        UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
        if (!Widget)
        {
            return Refuse(FString::Printf(TEXT("'%s' is not a widget in %s."), *Name, *WidgetBP->GetName()), TEXT("WIDGET_NOT_FOUND"));
        }
        if (!Slot || (Canvas && Slot->Parent != Canvas))
        {
            return Refuse(Slot ? FString::Printf(TEXT("'%s' is in another Canvas Panel than '%s': the widgets of one call share a panel."), *Name, *Canvas->GetName())
                               : FString::Printf(TEXT("'%s' sits in %s: arrange_widgets places the children of a Canvas Panel."), *Name,
                                                 Widget->Slot ? *Widget->Slot->GetClass()->GetName() : TEXT("no panel (it is the root)")),
                          TEXT("NOT_A_CANVAS"));
        }
        Canvas = Cast<UCanvasPanel>(Slot->Parent);
        Slots.Add(Slot);
    }
    TArray<McpWidgetArrange::FArrangeBox> Current;
    TArray<FVector2D> MinSize;
    FVector2D Panel;
    TArray<McpWidgetArrange::FArrangeBox> Boxes;
    int32 Failed = INDEX_NONE;
    if (!Canvas || !ArrangeMeasure(WidgetBP, Reference[0], Canvas->GetName(), Names, Current, MinSize, Panel, Error))
    {
        return Refuse(Canvas ? Error : TEXT("The widgets are in no Canvas Panel."), TEXT("ARRANGE_FAILED"));
    }
    if (!McpWidgetArrange::Solve(Relations, Current, MinSize, Panel, Boxes, Failed))
    {
        return Refuse(FString::Printf(TEXT("relations[%d] (%s) cannot hold with the required relations before it; nothing was changed."),
                                      Failed, *Relations[Failed].Text), TEXT("UNSATISFIABLE"));
    }
    const TArray<TPair<McpWidgetArrange::EAnchor, McpWidgetArrange::EAnchor>> Anchors = McpWidgetArrange::InferAnchors(Relations, Boxes, Panel);
    const bool bDryRun = GetJsonBoolField(Payload, TEXT("dryRun"), false);
    if (!bDryRun)
    {
        TArray<UObject*> Touched = {WidgetBP};
        Touched.Append(Slots);
        {
            FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Arrange Widgets")), EMcpMutationDurability::EditorStateOnly, Touched);
            for (int32 Index = 0; Index < Slots.Num(); ++Index)
            {
                McpWidgetArrange::WriteSlot(*Slots[Index], Boxes[Index], Anchors[Index], Panel);
            }
            Transaction.DescribeInto(ResultJson);
        }
        RefreshWidgetBlueprintClass(WidgetBP); // the drawings below lay out the compiled class
        ResultJson->SetBoolField(TEXT("saved"), MarkWidgetBlueprintModifiedAndSave(WidgetBP));
    }
    // Each relation measured again at each size, as the game lays the widget out there.
    TArray<TArray<FString>> MetAt;
    MetAt.SetNum(Relations.Num());
    TArray<double> Worst;
    Worst.Init(0.0, Relations.Num());
    for (const FIntPoint& Size : bDryRun ? TArray<FIntPoint>() : Sizes)
    {
        TArray<McpWidgetArrange::FArrangeBox> At;
        FVector2D AtPanel;
        if (ArrangeMeasure(WidgetBP, Size, Canvas->GetName(), Names, At, MinSize, AtPanel, Error))
        {
            for (int32 Index = 0; Index < Relations.Num(); ++Index)
            {
                const double Miss = McpWidgetArrange::Residual(Relations[Index], At, AtPanel);
                Worst[Index] = FMath::Max(Worst[Index], Miss);
                if (Miss <= ArrangeTolerance)
                {
                    MetAt[Index].Add(FString::Printf(TEXT("%dx%d"), Size.X, Size.Y));
                }
            }
        }
    }
    TArray<TSharedPtr<FJsonValue>> WidgetRows;
    for (int32 Index = 0; Index < Names.Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Names[Index]);
        Row->SetArrayField(TEXT("box"), {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Boxes[Index].Left)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Boxes[Index].Top)),
                                         MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Boxes[Index].Width)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Boxes[Index].Height))});
        Row->SetStringField(TEXT("anchors"), FString::Printf(TEXT("%s %s"), ArrangeAnchorName(Anchors[Index].Key, true), ArrangeAnchorName(Anchors[Index].Value, false)));
        WidgetRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    TArray<TSharedPtr<FJsonValue>> RelationRows;
    int32 Holding = 0;
    for (int32 Index = 0; Index < Relations.Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("relation"), Relations[Index].Text);
        TArray<TSharedPtr<FJsonValue>> Met;
        for (const FString& Size : MetAt[Index])
        {
            Met.Add(MakeShared<FJsonValueString>(Size));
        }
        Row->SetArrayField(TEXT("metAt"), Met);
        Row->SetNumberField(TEXT("worstMiss"), FMath::RoundToDouble(Worst[Index] * 10.0) / 10.0);
        RelationRows.Add(MakeShared<FJsonValueObject>(Row));
        Holding += MetAt[Index].Num() == Sizes.Num() ? 1 : 0;
    }
    ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));
    ResultJson->SetArrayField(TEXT("widgets"), WidgetRows);
    ResultJson->SetArrayField(TEXT("relations"), RelationRows);
    ResultJson->SetBoolField(TEXT("dryRun"), bDryRun);
    ResultJson->SetObjectField(TEXT("auditCall"), ArrangeAuditCall(WidgetPath, Sizes));
    Subsystem.SendAutomationResponse(Socket, RequestId, true, bDryRun
        ? FString::Printf(TEXT("Would arrange %d widget(s) at %dx%d (dryRun: nothing changed)."), Names.Num(), Reference[0].X, Reference[0].Y)
        : FString::Printf(TEXT("Arranged %d widget(s); %d of %d relation(s) hold at all %d size(s)."), Names.Num(), Holding, Relations.Num(), Sizes.Num()),
        ResultJson);
    return true;
}
} // namespace WidgetAuthoringHandlers
