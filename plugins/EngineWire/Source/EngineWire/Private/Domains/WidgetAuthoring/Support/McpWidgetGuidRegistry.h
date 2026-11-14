#pragma once

#include "CoreMinimal.h"

class UWidget;
class UWidgetAnimation;
class UWidgetBlueprint;

namespace WidgetAuthoringHelpers
{
void RegisterWidgetGuid(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget);
void UnregisterWidgetGuid(UWidgetBlueprint* WidgetBlueprint, UWidget* Widget);
void RegisterAnimationGuid(UWidgetBlueprint* WidgetBlueprint, UWidgetAnimation* Animation);
// A new animation AnimationName on WidgetBlueprint as the UMG editor makes one: its MovieScene of the same name at a
// 20 fps display rate, a playback range from 0 to EndSeconds plus one tick, its GUID registered. Null with OutError,
// before anything is made, when EndSeconds is past what the scene's tick rate holds.
UWidgetAnimation* McpCreateWidgetAnimation(UWidgetBlueprint* WidgetBlueprint, const FString& AnimationName, double EndSeconds,
                                           FString& OutError);
// Takes Animation out of the Blueprint as the UMG editor's delete does: out of Animations, renamed into the transient
// package so its name is free at once, and (5.6 and later) its variable GUID entry removed.
void RemoveWidgetAnimation(UWidgetBlueprint* WidgetBlueprint, UWidgetAnimation* Animation);
void RegisterAllWidgetGuids(UWidgetBlueprint* WidgetBlueprint);
}
