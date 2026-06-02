#include "UI/SMcpStatusBarWidget.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeSettings.h"
#include "MCP/McpNativeTransport.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"

void SMcpStatusBarWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.OnClicked_Raw(this, &SMcpStatusBarWidget::OnClicked)
		.ToolTipText(FText::FromString(TEXT("Open MCP Automation Bridge settings")))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.f, 0.f, 2.f, 0.f)
			[
				SNew(SImage)
				.ColorAndOpacity_Raw(this, &SMcpStatusBarWidget::GetStatusColor)
				.Image(FAppStyle::Get().GetBrush("Icons.FilledCircle"))
				.DesiredSizeOverride(FVector2D(8.0, 8.0))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f, 0.f, 4.f, 0.f)
			[
				SNew(STextBlock)
				.Text_Raw(this, &SMcpStatusBarWidget::GetStatusText)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];
}

FReply SMcpStatusBarWidget::OnClicked()
{
	ISettingsModule* SettingsModule = FModuleManager::LoadModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
	{
		SettingsModule->ShowViewer("Project", "Plugins", "McpAutomationBridgeSettings");
	}
	return FReply::Handled();
}

FText SMcpStatusBarWidget::GetStatusText() const
{
	if (!GEditor) return FText::FromString(TEXT("MCP"));

	auto* Subsystem = GEditor->GetEditorSubsystem<UMcpAutomationBridgeSubsystem>();
	if (Subsystem && Subsystem->NativeTransport && Subsystem->NativeTransport->IsRunning())
	{
		const int32 Port = Subsystem->NativeTransport->GetListenPort();
		const int32 Sessions = Subsystem->NativeTransport->GetActiveSessionCount();
		return FText::FromString(FString::Printf(TEXT("MCP :%d (%d)"), Port, Sessions));
	}

	return FText::FromString(TEXT("MCP off"));
}

FSlateColor SMcpStatusBarWidget::GetStatusColor() const
{
	if (!GEditor) return FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f));

	auto* Subsystem = GEditor->GetEditorSubsystem<UMcpAutomationBridgeSubsystem>();
	if (Subsystem && Subsystem->NativeTransport && Subsystem->NativeTransport->IsRunning())
	{
		return FSlateColor(FLinearColor(0.1f, 0.8f, 0.1f)); // Green
	}

	return FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)); // Gray
}
