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

FReply SUnrealAgentPanel::OnModelOptionClicked(TSharedPtr<FOpenCodeAcpModelOption> SelectedModel)
{
    OnModelSelected(SelectedModel);

    if (CenterModelComboButton.IsValid() && CenterModelComboButton->IsOpen())
    {
        CenterModelComboButton->SetIsOpen(false);
    }
    if (BottomModelComboButton.IsValid() && BottomModelComboButton->IsOpen())
    {
        BottomModelComboButton->SetIsOpen(false);
    }

    return FReply::Handled();
}

void SUnrealAgentPanel::OnModelSelected(TSharedPtr<FOpenCodeAcpModelOption> SelectedModel)
{
    SelectedModelOption = SelectedModel;

    if (!SelectedModel.IsValid() || !AcpClient.IsValid())
    {
        return;
    }

    AcpClient->SetModel(SelectedModel->Id);
}

FReply SUnrealAgentPanel::OnAgentOptionClicked(TSharedPtr<FOpenCodeAcpAgentOption> SelectedAgent)
{
    OnAgentSelected(SelectedAgent);

    if (CenterAgentComboButton.IsValid() && CenterAgentComboButton->IsOpen())
    {
        CenterAgentComboButton->SetIsOpen(false);
    }
    if (BottomAgentComboButton.IsValid() && BottomAgentComboButton->IsOpen())
    {
        BottomAgentComboButton->SetIsOpen(false);
    }

    return FReply::Handled();
}

FReply SUnrealAgentPanel::OnThinkingOptionClicked(TSharedPtr<FOpenCodeAcpThinkingOption> SelectedThinking)
{
    OnThinkingSelected(SelectedThinking);

    if (CenterThinkingComboButton.IsValid() && CenterThinkingComboButton->IsOpen())
    {
        CenterThinkingComboButton->SetIsOpen(false);
    }
    if (BottomThinkingComboButton.IsValid() && BottomThinkingComboButton->IsOpen())
    {
        BottomThinkingComboButton->SetIsOpen(false);
    }

    return FReply::Handled();
}

void SUnrealAgentPanel::OnThinkingSelected(TSharedPtr<FOpenCodeAcpThinkingOption> SelectedThinking)
{
    SelectedThinkingOption = SelectedThinking;

    if (!SelectedThinking.IsValid() || !AcpClient.IsValid())
    {
        return;
    }

    AcpClient->SetThinking(SelectedThinking->Id);
}

void SUnrealAgentPanel::OnAgentSelected(TSharedPtr<FOpenCodeAcpAgentOption> SelectedAgent)
{
    SelectedAgentOption = SelectedAgent;

    if (!SelectedAgent.IsValid() || !AcpClient.IsValid())
    {
        return;
    }

    AcpClient->SetAgent(SelectedAgent->Id);
}

#if WITH_DEV_AUTOMATION_TESTS

void SUnrealAgentPanel::OnModelMenuOpened()
{
    ModelSearchText.Reset();
    if (ModelSearchBox.IsValid())
    {
        ModelSearchBox->SetText(FText::GetEmpty());
    }
    RebuildFilteredModelOptions();
}

void SUnrealAgentPanel::OnModelSearchChanged(const FText& SearchText)
{
    ModelSearchText = SearchText.ToString();
    RebuildFilteredModelOptions();
}

void SUnrealAgentPanel::RefreshModelOptions()
{
    StoreActiveContextWindowUsage();

    ModelOptions.Reset();
    SelectedModelOption.Reset();
    SelectedThinkingOption.Reset();
    SelectedAgentOption.Reset();

    if (AcpClient.IsValid())
    {
        const FString& CurrentModel = AcpClient->GetCurrentModel();
        for (const FOpenCodeAcpModelOption& ModelOption : AcpClient->GetModelOptions())
        {
            TSharedPtr<FOpenCodeAcpModelOption> SharedOption = MakeShared<FOpenCodeAcpModelOption>(ModelOption);
            if (SharedOption->Id == CurrentModel)
            {
                SelectedModelOption = SharedOption;
            }
            ModelOptions.Add(SharedOption);
        }

        const FString& CurrentThinking = AcpClient->GetCurrentThinking();
        for (const FOpenCodeAcpThinkingOption& ThinkingOption : AcpClient->GetThinkingOptions())
        {
            if (ThinkingOption.Id == CurrentThinking)
            {
                SelectedThinkingOption = MakeShared<FOpenCodeAcpThinkingOption>(ThinkingOption);
                break;
            }
        }

        const FString& CurrentAgent = AcpClient->GetCurrentAgent();
        for (const FOpenCodeAcpAgentOption& AgentOption : AcpClient->GetAgentOptions())
        {
            if (AgentOption.Id == CurrentAgent)
            {
                SelectedAgentOption = MakeShared<FOpenCodeAcpAgentOption>(AgentOption);
                break;
            }
        }
    }

    ModelOptions.Sort([](const TSharedPtr<FOpenCodeAcpModelOption>& Left, const TSharedPtr<FOpenCodeAcpModelOption>& Right)
    {
        if (!Left.IsValid() || !Right.IsValid())
        {
            return Left.IsValid();
        }

        const int32 ProviderCompare = Left->GetProviderName().Compare(Right->GetProviderName(), ESearchCase::IgnoreCase);
        if (ProviderCompare != 0)
        {
            return ProviderCompare < 0;
        }

        return Left->GetDisplayName().Compare(Right->GetDisplayName(), ESearchCase::IgnoreCase) < 0;
    });

    RebuildFilteredModelOptions();
}

#undef LOCTEXT_NAMESPACE
