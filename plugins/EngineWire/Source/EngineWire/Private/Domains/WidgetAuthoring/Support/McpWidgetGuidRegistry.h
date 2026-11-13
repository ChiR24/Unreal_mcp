#pragma once

class UWidget;
class UWidgetAnimation;
class UWidgetBlueprint;

namespace WidgetAuthoringHelpers
{
void RegisterWidgetGuid(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget);
void UnregisterWidgetGuid(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget);
void RegisterAnimationGuid(UWidgetBlueprint* WidgetBlueprint, UWidgetAnimation* Animation);
// Takes Animation out of the Blueprint as the UMG editor's delete does: out of Animations, renamed into the transient
// package so its name is free at once, and (5.6 and later) its variable GUID entry removed.
void RemoveWidgetAnimation(UWidgetBlueprint* WidgetBlueprint, UWidgetAnimation* Animation);
void RegisterAllWidgetGuids(UWidgetBlueprint* WidgetBlueprint);
}
