#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FAutomationTestBase;
class SButton;
class SDockTab;
class SUnrealAgentPanel;
class SWidget;

namespace UnrealAgent::AutomationTests
{
    struct FPanelTestFixture
    {
        FString ChatHistoryPath;
        TSharedPtr<SDockTab> Tab;
        TSharedPtr<SWidget> PanelContent;
        TSharedPtr<SUnrealAgentPanel> Panel;

        FPanelTestFixture();
        ~FPanelTestFixture();

        bool Open(FAutomationTestBase& Test);
        void Cleanup();
        TSharedRef<SWidget> Content() const;
        TSharedRef<SUnrealAgentPanel> PanelRef() const;
    };

    TSharedPtr<SWidget> FindHistoryContainerByTitle(const TSharedRef<SWidget>& RootWidget, const FString& ExpectedTitle);
    TSharedPtr<SButton> FindHistoryButtonByTitle(const TSharedRef<SWidget>& RootWidget, const FString& ExpectedTitle);
    bool VerifyPanelLayout(FAutomationTestBase& Test, FPanelTestFixture& Fixture);
    bool VerifyPanelTranscriptRendering(FAutomationTestBase& Test, FPanelTestFixture& Fixture);
    bool VerifyPanelHistoryInteractions(FAutomationTestBase& Test, FPanelTestFixture& Fixture);
}
