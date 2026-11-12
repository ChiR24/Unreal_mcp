#include "UI/SMcpStatusBarWidget.h"
#include "EngineWireSubsystem.h"
#include "EngineWireSettings.h"
#include "MCP/Transport/McpNativeTransport.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"

namespace
{
// The native transport while it serves /mcp, else null.
FMcpNativeTransport* RunningTransport()
{
	const UEngineWireSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UEngineWireSubsystem>() : nullptr;
	return Subsystem && Subsystem->NativeTransport && Subsystem->NativeTransport->IsRunning() ? Subsystem->NativeTransport.Get() : nullptr;
}
}

void SMcpStatusBarWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
		.OnClicked_Raw(this, &SMcpStatusBarWidget::OnClicked)
		.ToolTipText(FText::FromString(TEXT("Open EngineWire settings")))
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
		SettingsModule->ShowViewer("Project", "Plugins", "EngineWireSettings");
	}
	return FReply::Handled();
}

FText SMcpStatusBarWidget::GetStatusText() const
{
	if (!GEditor) return FText::FromString(TEXT("MCP"));

	if (const FMcpNativeTransport* Transport = RunningTransport())
	{
		return FText::FromString(FString::Printf(TEXT("MCP :%d (%d)"), Transport->GetListenPort(), Transport->GetActiveSessionCount()));
	}

	return FText::FromString(TEXT("MCP off"));
}

FSlateColor SMcpStatusBarWidget::GetStatusColor() const
{
	// Green while /mcp is served, gray otherwise.
	return FSlateColor(RunningTransport() ? FLinearColor(0.1f, 0.8f, 0.1f) : FLinearColor(0.4f, 0.4f, 0.4f));
}
