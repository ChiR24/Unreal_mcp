#include "SUnrealAgentPanel.h"
#include "SUnrealAgentPanelPrivate.h"

#include "../Acp/McpOpenCodeAcpClient.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleColors.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SUnrealAgentPanel"

using namespace UnrealAgent::Panel;

TSharedPtr<SMultiLineEditableTextBox> SUnrealAgentPanel::GetActivePromptTextBox() const
{
    return bHasConversationContent ? BottomPromptTextBox : CenterPromptTextBox;
}

void SUnrealAgentPanel::ClearPromptTextBoxes()
{
    if (CenterPromptTextBox.IsValid())
    {
        CenterPromptTextBox->SetText(FText::GetEmpty());
    }
    if (BottomPromptTextBox.IsValid())
    {
        BottomPromptTextBox->SetText(FText::GetEmpty());
    }
}

void SUnrealAgentPanel::RefreshModelComboButtons()
{
    PopulateModelMenuList();
}

void SUnrealAgentPanel::RebuildFilteredModelOptions()
{
    FilteredModelOptions.Reset();

    for (const TSharedPtr<FOpenCodeAcpModelOption>& ModelOption : ModelOptions)
    {
        if (!ModelOption.IsValid())
        {
            continue;
        }

        if (ModelSearchText.IsEmpty()
            || ModelOption->GetDisplayName().Contains(ModelSearchText, ESearchCase::IgnoreCase)
            || ModelOption->GetProviderName().Contains(ModelSearchText, ESearchCase::IgnoreCase)
            || ModelOption->Id.Contains(ModelSearchText, ESearchCase::IgnoreCase))
        {
            FilteredModelOptions.Add(ModelOption);
        }
    }

    RefreshModelComboButtons();
}

#undef LOCTEXT_NAMESPACE
