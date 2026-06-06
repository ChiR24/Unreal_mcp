#if WITH_DEV_AUTOMATION_TESTS

#include "UnrealAgentAutomationWidgetHelpers.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/Children.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWidget.h"

namespace UnrealAgent::AutomationTests
{
    TSharedPtr<SWidget> FindWidgetByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag)
    {
        if (RootWidget->GetTag() == Tag)
        {
            return RootWidget;
        }

        FChildren* Children = RootWidget->GetAllChildren();
        if (Children == nullptr)
        {
            return nullptr;
        }

        for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
        {
            TSharedPtr<SWidget> Match = FindWidgetByTag(Children->GetChildAt(ChildIndex), Tag);
            if (Match.IsValid())
            {
                return Match;
            }
        }

        return nullptr;
    }

    void FindWidgetsByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag, TArray<TSharedPtr<SWidget>>& OutWidgets)
    {
        if (RootWidget->GetTag() == Tag)
        {
            OutWidgets.Add(RootWidget);
        }

        FChildren* Children = RootWidget->GetAllChildren();
        if (Children == nullptr)
        {
            return;
        }

        for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
        {
            FindWidgetsByTag(Children->GetChildAt(ChildIndex), Tag, OutWidgets);
        }
    }

    namespace
    {
        bool FindTraversalIndex(const TSharedRef<SWidget>& RootWidget, const FName& Tag, int32& OutIndex, int32& Cursor)
        {
            const int32 CurrentIndex = Cursor++;
            if (RootWidget->GetTag() == Tag)
            {
                OutIndex = CurrentIndex;
                return true;
            }

            FChildren* Children = RootWidget->GetAllChildren();
            if (Children == nullptr)
            {
                return false;
            }

            for (int32 ChildIndex = 0; ChildIndex < Children->Num(); ++ChildIndex)
            {
                if (FindTraversalIndex(Children->GetChildAt(ChildIndex), Tag, OutIndex, Cursor))
                {
                    return true;
                }
            }

            return false;
        }
    }

    bool FindWidgetTraversalIndexByTag(const TSharedRef<SWidget>& RootWidget, const FName& Tag, int32& OutIndex)
    {
        int32 Cursor = 0;
        OutIndex = INDEX_NONE;
        return FindTraversalIndex(RootWidget, Tag, OutIndex, Cursor);
    }

    void ClickSlateButton(const TSharedPtr<SButton>& Button)
    {
        if (!Button.IsValid())
        {
            return;
        }

        TSet<FKey> PressedButtons;
        PressedButtons.Add(EKeys::LeftMouseButton);
        const FPointerEvent PointerEvent(
            0,
            FVector2D::ZeroVector,
            FVector2D::ZeroVector,
            PressedButtons,
            EKeys::LeftMouseButton,
            0.0f,
            FModifierKeysState());

        Button->SetClickMethod(EButtonClickMethod::MouseDown);
        Button->OnMouseButtonDown(FGeometry(), PointerEvent);
    }
}

#endif
