#include "Domains/WidgetAuthoring/Support/McpWidgetGuidRegistry.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
void RegisterWidgetGuid(UWidgetBlueprint* WidgetBP, UWidget* Widget)
{
    if (!WidgetBP || !Widget)
    {
        return;
    }
#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
    const FName WidgetFName = Widget->GetFName();
    if (!MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Contains(WidgetFName))
    {
        MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Emplace(WidgetFName, MCP_NEW_DETERMINISTIC_GUID(Widget->GetPathName()));
    }
#else
    // Before UE 5.6 UWidgetBlueprint has no WidgetVariableNameToGuidMap, so there
    // is nothing to register: the engine keeps the widget variable's GUID in
    // UBlueprint::NewVariables[].VarGuid and assigns it itself. Writing our own
    // GUID in there would overwrite an engine-managed value that existing
    // bindings resolve through. This branch previously logged "registered",
    // claiming work it had not done.
#endif
}

void UnregisterWidgetGuid(UWidgetBlueprint* WidgetBP, UWidget* Widget)
{
    if (!WidgetBP || !Widget)
    {
        return;
    }
#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
    const FName WidgetFName = Widget->GetFName();
    if (MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Contains(WidgetFName))
    {
        MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Remove(WidgetFName);
        UE_LOG(LogTemp, Verbose, TEXT("UnregisterWidgetGuid: Unregistered widget '%s'"), *WidgetFName.ToString());
    }
#else
    // No GUID map before 5.6, so nothing was ever registered to remove. Logged
    // "unregistered" before, which read as confirmation of a removal.
#endif
}

void RegisterAnimationGuid(UWidgetBlueprint* WidgetBP, UWidgetAnimation* Animation)
{
    if (!WidgetBP || !Animation)
    {
        return;
    }
#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
    const FName AnimFName = Animation->GetFName();
    if (!MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Contains(AnimFName))
    {
        FGuid AnimGuid = MCP_NEW_DETERMINISTIC_GUID(Animation->GetPathName());
        MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Emplace(AnimFName, AnimGuid);
        UE_LOG(LogTemp, Verbose, TEXT("RegisterAnimationGuid: Registered animation '%s' with GUID %s"),
            *AnimFName.ToString(), *AnimGuid.ToString());
    }
#else
    // Same as RegisterWidgetGuid: no map to write to before 5.6. The animation
    // itself is still added to WidgetBP->Animations below, which is the part
    // that actually matters on these engines.
#endif
    if (!WidgetBP->Animations.Contains(Animation))
    {
        WidgetBP->Animations.Add(Animation);
    }
}

void RegisterAllWidgetGuids(UWidgetBlueprint* WidgetBP)
{
    if (!WidgetBP || !WidgetBP->WidgetTree)
    {
        return;
    }
    WidgetBP->WidgetTree->ForEachWidget([WidgetBP](UWidget* Widget) {
        if (Widget)
        {
            RegisterWidgetGuid(WidgetBP, Widget);
        }
    });
    for (UWidgetAnimation* Animation : WidgetBP->Animations)
    {
        if (Animation)
        {
            RegisterAnimationGuid(WidgetBP, Animation);
        }
    }
}
}
