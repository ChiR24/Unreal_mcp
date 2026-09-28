#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "Components/CanvasPanel.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "UObject/Package.h"

namespace WidgetAuthoringHelpers
{
void UnregisterWidgetAndChildren(UWidgetBlueprint* WidgetBP, UWidget* Widget)
{
    if (!WidgetBP || !Widget)
    {
        return;
    }
    UnregisterWidgetGuid(WidgetBP, Widget);
    if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
    {
        for (UWidget* Child : PanelWidget->GetAllChildren())
        {
            if (Child)
            {
                UnregisterWidgetAndChildren(WidgetBP, Child);
            }
        }
    }
}

namespace
{
bool SeatWidgetInTree(UWidgetBlueprint* WidgetBP, UWidget* NewWidget, const FString& ParentSlot)
{
    if (!WidgetBP || !WidgetBP->WidgetTree || !NewWidget)
    {
        return false;
    }
    UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
    // Every add path funnels through here, so this is the one place that can
    // guarantee it. Without bIsVariable the compiler emits no member property,
    // leaving a widget authored through this API unreachable from its own graph
    // — and UUserWidget::GetWidgetFromName is not a UFUNCTION, so there is no
    // Blueprint-side workaround. A widget added by name is meant to be driven.
    NewWidget->bIsVariable = true;
    // Every widget that reaches the tree needs a GUID map entry before the next compile (dogfood c22).
    RegisterWidgetGuid(WidgetBP, NewWidget);
    if (ParentSlot.IsEmpty())
    {
        if (WidgetTree->RootWidget == NewWidget)
        {
            // The root's own slotName re-used: it is already seated. Adding it
            // under the root made it its own child, and UMG recursed through
            // that cycle until the editor died of a stack overflow.
            return true;
        }
        if (!WidgetTree->RootWidget)
        {
            // A leaf at the root can hold no siblings, so the *second* top-level
            // add used to hit the replace path below. Give leaves a canvas root
            // up front and every later add is an ordinary AddChild.
            if (Cast<UPanelWidget>(NewWidget))
            {
                WidgetTree->RootWidget = NewWidget;
                UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Set '%s' as root widget"), *NewWidget->GetName());
                return true;
            }
            UCanvasPanel* NewRoot = CreateAndRegisterWidget<UCanvasPanel>(
                WidgetBP, WidgetTree, TEXT("RootCanvas"));
            if (!NewRoot)
            {
                WidgetTree->RootWidget = NewWidget;
                return true;
            }
            WidgetTree->RootWidget = NewRoot;
            NewRoot->AddChild(NewWidget);
            UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Created canvas root for leaf '%s'"),
                *NewWidget->GetName());
        }
        else if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
        {
            RootPanel->AddChild(NewWidget);
            UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Added '%s' as child of root panel '%s'"),
                *NewWidget->GetName(), *RootPanel->GetName());
        }
        else
        {
            // Destroying the existing root to seat the newcomer silently threw
            // away everything already authored, and the reparent it left behind
            // tripped an engine ensure. Promote instead: wrap both in a canvas.
            UWidget* ExistingRoot = WidgetTree->RootWidget;
            UCanvasPanel* NewRoot = CreateAndRegisterWidget<UCanvasPanel>(
                WidgetBP, WidgetTree, TEXT("RootCanvas"));
            if (!NewRoot)
            {
                UE_LOG(LogTemp, Warning, TEXT("SafeAddWidgetToTree: Could not create canvas root for '%s'"),
                    *NewWidget->GetName());
                return false;
            }
            WidgetTree->RootWidget = NewRoot;
            NewRoot->AddChild(ExistingRoot);
            NewRoot->AddChild(NewWidget);
            UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Promoted root '%s' into a canvas to seat '%s'"),
                *ExistingRoot->GetName(), *NewWidget->GetName());
        }
        return true;
    }

    UWidget* ParentWidget = WidgetTree->FindWidget(FName(*ParentSlot));
    if (!ParentWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("SafeAddWidgetToTree: Parent widget '%s' not found"), *ParentSlot);
        return false;
    }
    UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
    if (!ParentPanel)
    {
        UE_LOG(LogTemp, Warning, TEXT("SafeAddWidgetToTree: Parent '%s' is not a panel widget"), *ParentSlot);
        return false;
    }
    // Same cycle one level down: a widget cannot be seated inside itself or its own subtree.
    for (const UWidget* Ancestor = ParentPanel; Ancestor; Ancestor = Ancestor->GetParent())
    {
        if (Ancestor == NewWidget)
        {
            UE_LOG(LogTemp, Warning, TEXT("SafeAddWidgetToTree: '%s' cannot be seated inside its own subtree ('%s')"),
                *NewWidget->GetName(), *ParentSlot);
            return false;
        }
    }
    ParentPanel->AddChild(NewWidget);
    UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Added '%s' as child of '%s'"),
        *NewWidget->GetName(), *ParentSlot);
    return true;
}
}

namespace
{
/**
 * Take a widget out of whichever panel currently lists it, keeping its layout.
 *
 * Re-using a slotName is how a caller edits an existing widget -- "the text
 * block called Txt_Health now reads 0". ConstructWidget re-initialises the
 * object that already holds that name rather than making a second one, and that
 * re-initialisation CLEARS the widget's Slot pointer. So GetParent() answers
 * null while the panel's own Slots array still points at the widget, and the
 * add that follows appends a second slot for the same widget: one widget listed
 * twice under one parent, with the graph's variable binding to whichever the
 * compiler reaches first.
 *
 * Asking the panels instead of the widget is what makes this reliable -- the
 * parent's slot list survives the re-initialisation that erases the widget's
 * own back-pointer.
 *
 * The layout has to come with it. Re-adding produces a fresh slot at the panel's
 * default position, so editing a HUD label's text would silently move it to the
 * corner. The old slot is handed back so the re-seat can copy it; the payload
 * still wins when the caller actually asked for new geometry. So are the old
 * parent and index, to put the widget back if the re-seat is refused.
 */
bool DetachFromOwningPanel(UWidgetBlueprint* WidgetBP, UWidget* NewWidget,
    UPanelSlot*& OutOldSlot, UPanelWidget*& OutOldParent, int32& OutOldIndex)
{
    OutOldSlot = nullptr;
    OutOldParent = nullptr;
    OutOldIndex = INDEX_NONE;
    if (!WidgetBP || !WidgetBP->WidgetTree || !NewWidget)
    {
        return false;
    }
    TArray<UWidget*> AllWidgets;
    WidgetBP->WidgetTree->GetAllWidgets(AllWidgets);
    bool bDetached = false;
    for (UWidget* Candidate : AllWidgets)
    {
        UPanelWidget* Panel = Cast<UPanelWidget>(Candidate);
        if (!Panel || Panel == NewWidget)
        {
            continue;
        }
        while (Panel->GetChildIndex(NewWidget) != INDEX_NONE)
        {
            if (!OutOldSlot)
            {
                OutOldIndex = Panel->GetChildIndex(NewWidget);
                OutOldSlot = Panel->GetSlots()[OutOldIndex];
                OutOldParent = Panel;
            }
            Panel->RemoveChild(NewWidget);
            bDetached = true;
        }
    }
    if (bDetached)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("SafeAddWidgetToTree: '%s' was already in the tree; it was detached and re-seated "
                 "rather than duplicated. Use a fresh slotName to add a second widget."),
            *NewWidget->GetName());
    }
    return bDetached;
}

// Copies every layout field the old slot shares with the new one by name and type: padding,
// alignment and size of a box slot, a canvas slot's layout, auto-size and z-order. Only a canvas
// layout used to survive, so a box child that was moved or re-added lost its padding silently.
void CarrySlotLayout(const UPanelSlot* From, UPanelSlot* To)
{
    if (!From || !To || From == To)
    {
        return;
    }
    for (TFieldIterator<FProperty> It(To->GetClass()); It; ++It)
    {
        // Parent and Content, declared on UPanelSlot itself, are the slot's wiring, not its layout.
        if (It->GetOwnerClass() == UPanelSlot::StaticClass() || It->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
        {
            continue;
        }
        const FProperty* Source = From->GetClass()->FindPropertyByName(It->GetFName());
        if (Source && Source->SameType(*It))
        {
            It->CopyCompleteValue(It->ContainerPtrToValuePtr<void>(To), Source->ContainerPtrToValuePtr<void>(From));
        }
    }
    To->SynchronizeProperties();
}
}

bool SafeAddWidgetToTree(UWidgetBlueprint* WidgetBP, UWidget* NewWidget, const FString& ParentSlot,
    const TSharedPtr<FJsonObject>& Payload)
{
    UPanelSlot* OldSlot = nullptr;
    UPanelWidget* OldParent = nullptr;
    int32 OldIndex = INDEX_NONE;
    DetachFromOwningPanel(WidgetBP, NewWidget, OldSlot, OldParent, OldIndex);

    if (!SeatWidgetInTree(WidgetBP, NewWidget, ParentSlot))
    {
        // A widget that already sat somewhere goes back where it was instead of dropping out of the tree.
        if (OldParent)
        {
            OldParent->InsertChildAt(OldIndex, NewWidget);
            CarrySlotLayout(OldSlot, NewWidget->Slot);
        }
        return false;
    }
    CarrySlotLayout(OldSlot, NewWidget->Slot);
    // The slot only exists once the widget is seated, so geometry has to land here
    // rather than in each caller — every add path funnels through this function.
    ApplyCanvasSlotGeometry(Payload, NewWidget);
    return true;
}

void ClearWidgetTreeForRebuild(UWidgetBlueprint* WidgetBP)
{
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        return;
    }
    UWidgetTree* WidgetTree = WidgetBP->WidgetTree;
    TArray<UWidget*> WidgetsToRemove;
    WidgetTree->ForEachWidget([&WidgetsToRemove](UWidget* Widget) {
        if (Widget)
        {
            WidgetsToRemove.Add(Widget);
        }
    });
    for (UWidget* Widget : WidgetsToRemove)
    {
        if (Widget)
        {
            WidgetTree->RemoveWidget(Widget);
        }
    }
    for (UWidget* Widget : WidgetsToRemove)
    {
        if (Widget)
        {
            Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        }
    }
    WidgetTree->RootWidget = nullptr;
#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
    WidgetBP->WidgetVariableNameToGuidMap.Empty();
#endif
    UE_LOG(LogTemp, Verbose, TEXT("ClearWidgetTreeForRebuild: Cleared %d widgets from tree"), WidgetsToRemove.Num());
}
}
