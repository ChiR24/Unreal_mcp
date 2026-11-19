// McpModalDialogTests.cpp — what a dialog says, read from a window built like one (never shown, never modal).

#include "Core/Requests/McpModalDialog.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpModalDialogTest,
    "EngineWire.Core.ModalDialog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpModalDialogTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestFalse(TEXT("no window, no dialog"), McpModalDialog::Describe(nullptr).IsValid());
    const TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("Save Content")))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()[SNew(STextBlock).Text(FText::FromString(TEXT("Save changes to 2 assets?")))]
            + SVerticalBox::Slot()[SNew(SButton)[SNew(STextBlock).Text(FText::FromString(TEXT("Save Selected")))]]
            + SVerticalBox::Slot()[SNew(SButton).IsEnabled(false)[SNew(STextBlock).Text(FText::FromString(TEXT("Don't Save")))]]
            + SVerticalBox::Slot()[SNew(SButton)] // no words: left out
        ];
    const TSharedPtr<FJsonObject> Dialog = McpModalDialog::Describe(Window);
    if (!TestTrue(TEXT("a window has a dialog"), Dialog.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("title"), Dialog->GetStringField(TEXT("title")), FString(TEXT("Save Content")));
    TestEqual(TEXT("message: the words outside the buttons"), Dialog->GetStringField(TEXT("message")),
              FString(TEXT("Save changes to 2 assets?")));
    const TArray<TSharedPtr<FJsonValue>> Buttons = Dialog->GetArrayField(TEXT("buttons"));
    if (!TestEqual(TEXT("the two buttons with words"), Buttons.Num(), 2))
    {
        return false;
    }
    TestEqual(TEXT("first label"), Buttons[0]->AsObject()->GetStringField(TEXT("label")), FString(TEXT("Save Selected")));
    TestTrue(TEXT("first enabled"), Buttons[0]->AsObject()->GetBoolField(TEXT("enabled")));
    TestEqual(TEXT("second label"), Buttons[1]->AsObject()->GetStringField(TEXT("label")), FString(TEXT("Don't Save")));
    TestFalse(TEXT("second disabled"), Buttons[1]->AsObject()->GetBoolField(TEXT("enabled")));
    TestTrue(TEXT("the same window reads once"), McpModalDialog::Describe(Window) == Dialog);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
