#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Status bar widget showing MCP server state.
 * Displays: ● MCP :3000 (2)
 */
class SMcpStatusBarWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMcpStatusBarWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
	FReply OnClicked();
};
