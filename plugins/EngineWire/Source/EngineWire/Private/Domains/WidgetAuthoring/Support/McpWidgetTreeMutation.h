#pragma once

#include "Blueprint/WidgetTree.h"
#include "Dom/JsonObject.h"
#include "Domains/WidgetAuthoring/Support/McpWidgetGuidRegistry.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
void UnregisterWidgetAndChildren(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget);

// The parent panel a call names (parentSlot); empty when none.
FString ResolveParentSlotName(const TSharedPtr<FJsonObject>& Payload);

// Applies positionX/positionY/sizeX/sizeY to a widget already seated in a canvas slot.
void ApplyCanvasSlotGeometry(const TSharedPtr<FJsonObject>& Payload, UWidget* Widget);

// Copies the layout fields From shares with To by name and type (padding, alignment, a canvas layout).
void CarrySlotLayout(const UPanelSlot* From, UPanelSlot* To);

// Passing Payload lets the one funnel every add path shares apply that geometry,
// so a caller never has to follow an add with a separate layout call. A widget already
// seated (reparent_widget) is detached and seated under the new parent, keeping its layout.
bool SafeAddWidgetToTree(UWidgetBlueprint* WidgetBlueprint, UWidget* NewWidget, const FString& ParentSlot,
    const TSharedPtr<FJsonObject>& Payload = TSharedPtr<FJsonObject>());
void ClearWidgetTreeForRebuild(UWidgetBlueprint* WidgetBlueprint);

// After renaming a widget: property bindings, animation bindings, navigation rules and the desired focus follow the new
// name, as the editor's rename does. Returns the animation bindings renamed.
int32 McpRenameWidgetReferences(UWidgetBlueprint* WidgetBP, FName OldName, FName NewName);

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
