#pragma once

#include "Blueprint/WidgetTree.h"
#include "Dom/JsonObject.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
void UnregisterWidgetAndChildren(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget);

// Reads parentSlot, falling back to the parentName spelling the contract also advertises.
FString ResolveParentSlotName(const TSharedPtr<FJsonObject>& Payload);

// Applies positionX/positionY/sizeX/sizeY to a widget already seated in a canvas slot.
void ApplyCanvasSlotGeometry(const TSharedPtr<FJsonObject>& Payload, UWidget* Widget);

// Passing Payload lets the one funnel every add path shares apply that geometry,
// so a caller never has to follow an add with a separate layout call.
bool SafeAddWidgetToTree(UWidgetBlueprint* WidgetBlueprint, UWidget* NewWidget, const FString& ParentSlot,
    const TSharedPtr<FJsonObject>& Payload = TSharedPtr<FJsonObject>());
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
