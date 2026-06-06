#pragma once

#include "CoreMinimal.h"

class UWidgetBlueprint;

namespace WidgetAuthoringHelpers
{
bool ValidateWidgetCreation(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, FString& OutError);
bool CheckWidgetExists(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName, FString& OutError);
}
