#pragma once

#include "CoreMinimal.h"

class UWidgetBlueprint;

namespace WidgetAuthoringHelpers
{
bool ValidateWidgetCreation(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, FString& OutError);

// Why a widget of WidgetClass cannot be named Name, or empty when it can. A widget compiles to a
// member of the generated class, so a name held by a variable, a function or an inherited property
// (UWidget::DisplayLabel) broke the compile and left the widget in the tree; a widget of another
// class under that name would be replaced in place. The same class is an edit: re-adding a slotName
// re-configures that widget. A BindWidget property on the parent is meant to be filled by name.
FString McpWidgetNameConflict(UWidgetBlueprint* WidgetBlueprint, FName Name, const UClass* WidgetClass);

// Name with the first free numeric suffix (Name_1, Name_2...), for the refusal to suggest.
FString McpFreeWidgetName(UWidgetBlueprint* WidgetBlueprint, FName Name, const UClass* WidgetClass);
}
