#include "Domains/WidgetAuthoring/Support/McpWidgetTreeMutation.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "MovieScene.h"
#include "WidgetBlueprint.h"
#if __has_include("WidgetBlueprintEditorUtils.h") // private to UMGEditor on 5.0
#include "WidgetBlueprintEditorUtils.h"
#endif

namespace WidgetAuthoringHelpers
{
// What the editor's rename fixes beyond the widget itself (FWidgetBlueprintEditorUtils::RenameWidget in every 5.x): property
// bindings, animations bound to the widget (binding and possessable name), navigation rules naming it and, from 5.6, the
// desired focus. A rename here changed only the widget, so its animations stopped finding it. Returns the animation
// bindings renamed.
int32 McpRenameWidgetReferences(UWidgetBlueprint* WidgetBP, FName OldName, FName NewName)
{
    const FString OldText = OldName.ToString();
    for (FDelegateEditorBinding& Binding : WidgetBP->Bindings)
    {
        if (Binding.ObjectName == OldText)
        {
            Binding.ObjectName = NewName.ToString();
        }
    }
    int32 AnimationBindings = 0;
    for (UWidgetAnimation* Animation : WidgetBP->Animations)
    {
        if (!Animation)
        {
            continue;
        }
        UMovieScene* MovieScene = Animation->GetMovieScene();
        for (FWidgetAnimationBinding& AnimBinding : Animation->AnimationBindings)
        {
            if (AnimBinding.WidgetName != OldName)
            {
                continue;
            }
            AnimBinding.WidgetName = NewName;
            ++AnimationBindings;
            FMovieScenePossessable* Possessable = MovieScene && AnimBinding.SlotWidgetName == NAME_None
                ? MovieScene->FindPossessable(AnimBinding.AnimationGuid) : nullptr;
            if (Possessable)
            {
                MovieScene->Modify();
                Possessable->SetName(NewName.ToString());
            }
        }
    }
    WidgetBP->WidgetTree->ForEachWidget([OldName, NewName](UWidget* Widget)
    {
        if (Widget && Widget->Navigation)
        {
            Widget->Navigation->SetFlags(RF_Transactional);
            Widget->Navigation->Modify();
            Widget->Navigation->TryToRenameBinding(OldName, NewName);
        }
    });
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
    FWidgetBlueprintEditorUtils::ReplaceDesiredFocus(WidgetBP, OldName, NewName);
#endif
    return AnimationBindings;
}
}
