#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class SButton;
class SWidget;

namespace UnrealAgent::AutomationTests
{
    TSharedPtr<SWidget> FindWidgetByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag);
    void FindWidgetsByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag, TArray<TSharedPtr<SWidget>>& OutWidgets);
    bool FindWidgetTraversalIndexByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag, int32& OutIndex);
    void ClickSlateButton(const TSharedPtr<SButton>& Button);
}
