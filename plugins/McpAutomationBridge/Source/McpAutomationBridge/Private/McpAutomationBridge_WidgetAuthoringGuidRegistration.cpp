#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"

#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "McpVersionCompatibility.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHelpers
{
void RegisterWidgetGuid(UWidgetBlueprint* WidgetBP, UWidget* Widget)
{
    if (!WidgetBP || !Widget)
    {
        return;
    }
    const FName WidgetFName = Widget->GetFName();
#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
    if (!MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Contains(WidgetFName))
    {
        FGuid WidgetGuid = MCP_NEW_DETERMINISTIC_GUID(Widget->GetPathName());
        MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Emplace(WidgetFName, WidgetGuid);
        UE_LOG(LogTemp, Verbose, TEXT("RegisterWidgetGuid: Registered widget '%s' with GUID %s"),
            *WidgetFName.ToString(), *WidgetGuid.ToString());
    }
#else
    UE_LOG(LogTemp, Verbose, TEXT("RegisterWidgetGuid: Widget '%s' registered (UE 5.0 mode)"),
        *WidgetFName.ToString());
#endif
}

void UnregisterWidgetGuid(UWidgetBlueprint* WidgetBP, UWidget* Widget)
{
    if (!WidgetBP || !Widget)
    {
        return;
    }
    const FName WidgetFName = Widget->GetFName();
#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
    if (MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Contains(WidgetFName))
    {
        MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Remove(WidgetFName);
        UE_LOG(LogTemp, Verbose, TEXT("UnregisterWidgetGuid: Unregistered widget '%s'"), *WidgetFName.ToString());
    }
#else
    UE_LOG(LogTemp, Verbose, TEXT("UnregisterWidgetGuid: Widget '%s' unregistered (UE 5.0 mode)"),
        *WidgetFName.ToString());
#endif
}

void RegisterAnimationGuid(UWidgetBlueprint* WidgetBP, UWidgetAnimation* Animation)
{
    if (!WidgetBP || !Animation)
    {
        return;
    }
    const FName AnimFName = Animation->GetFName();
#if MCP_HAS_WIDGET_VARIABLE_GUID_MAP
    if (!MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Contains(AnimFName))
    {
        FGuid AnimGuid = MCP_NEW_DETERMINISTIC_GUID(Animation->GetPathName());
        MCP_WIDGET_BP_GET_GUID_MAP(WidgetBP).Emplace(AnimFName, AnimGuid);
        UE_LOG(LogTemp, Verbose, TEXT("RegisterAnimationGuid: Registered animation '%s' with GUID %s"),
            *AnimFName.ToString(), *AnimGuid.ToString());
    }
#else
    UE_LOG(LogTemp, Verbose, TEXT("RegisterAnimationGuid: Animation '%s' registered (UE 5.0 mode)"),
        *AnimFName.ToString());
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
