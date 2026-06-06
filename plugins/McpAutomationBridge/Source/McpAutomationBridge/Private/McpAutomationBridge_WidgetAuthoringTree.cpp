#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "McpVersionCompatibility.h"
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
    if (ParentSlot.IsEmpty())
    {
        if (!WidgetTree->RootWidget)
        {
            WidgetTree->RootWidget = NewWidget;
            UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Set '%s' as root widget"), *NewWidget->GetName());
        }
        else if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
        {
            RootPanel->AddChild(NewWidget);
            UE_LOG(LogTemp, Verbose, TEXT("SafeAddWidgetToTree: Added '%s' as child of root panel '%s'"),
                *NewWidget->GetName(), *RootPanel->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SafeAddWidgetToTree: Replacing non-panel root '%s' with '%s'"),
                *WidgetTree->RootWidget->GetName(), *NewWidget->GetName());
            UnregisterWidgetAndChildren(WidgetBP, WidgetTree->RootWidget);
            WidgetTree->RemoveWidget(WidgetTree->RootWidget);
            WidgetTree->RootWidget = NewWidget;
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
