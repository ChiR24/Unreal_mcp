#pragma once

class UWidget;
class UWidgetAnimation;
class UWidgetBlueprint;

namespace WidgetAuthoringHelpers
{
void RegisterWidgetGuid(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget);
void UnregisterWidgetGuid(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget);
void RegisterAnimationGuid(UWidgetBlueprint* WidgetBlueprint, UWidgetAnimation* Animation);
void RegisterAllWidgetGuids(UWidgetBlueprint* WidgetBlueprint);
}
