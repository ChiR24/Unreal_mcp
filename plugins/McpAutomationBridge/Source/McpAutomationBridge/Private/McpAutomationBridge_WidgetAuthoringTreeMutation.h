#pragma once

#include "Blueprint/WidgetTree.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
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
        RegisterWidgetGuid(WidgetBlueprint, Widget);
    }
    return Widget;
}
}
