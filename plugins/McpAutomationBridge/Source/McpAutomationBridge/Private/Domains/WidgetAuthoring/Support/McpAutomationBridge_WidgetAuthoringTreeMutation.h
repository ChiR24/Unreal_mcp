#pragma once

#include "Blueprint/WidgetTree.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
void UnregisterWidgetAndChildren(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget);
bool SafeAddWidgetToTree(UWidgetBlueprint* WidgetBlueprint, UWidget* NewWidget, const FString& ParentSlot);
void ClearWidgetTreeForRebuild(UWidgetBlueprint* WidgetBlueprint);

template<typename T>
T* CreateAndRegisterWidget(UWidgetBlueprint* WidgetBlueprint, UWidgetTree* WidgetTree, FName WidgetName)
{
    static_assert(TIsDerivedFrom<T, UWidget>::Value, "T must derive from UWidget");
    if (!WidgetBlueprint || !WidgetTree)
    {
        return nullptr;
    }

    T* Widget = WidgetTree->ConstructWidget<T>(T::StaticClass(), WidgetName);
    if (Widget)
    {
        // Without this the compiler generates no member property, so a widget
        // authored here cannot be referenced from the graph at all — and the
        // only Blueprint-reachable alternative, UUserWidget::GetWidgetFromName,
        // is not a UFUNCTION. A widget created through this API is one the
        // caller intends to drive, so expose it.
        Widget->bIsVariable = true;
        RegisterWidgetGuid(WidgetBlueprint, Widget);
    }
    return Widget;
}
}
