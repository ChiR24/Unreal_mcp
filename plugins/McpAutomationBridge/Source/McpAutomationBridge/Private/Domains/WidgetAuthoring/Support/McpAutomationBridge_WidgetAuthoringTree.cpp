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

bool SafeAddWidgetToTree(UWidgetBlueprint* WidgetBP, UWidget* NewWidget, const FString& ParentSlot)
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
    if (ParentSlot.IsEmpty())
    {
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
    ParentPanel->AddChild(NewWidget);
    UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Added '%s' as child of '%s'"),
        *NewWidget->GetName(), *ParentSlot);
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
