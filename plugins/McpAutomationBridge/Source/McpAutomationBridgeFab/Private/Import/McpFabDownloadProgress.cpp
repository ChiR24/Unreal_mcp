// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabDownloadProgress.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace McpFabDownloadProgress
{
namespace
{
// A notification window is small; these only stop a runaway tree.
constexpr int32 MaxDepth = 30;
constexpr int32 MaxWidgets = 600;

// The pieces of one toast, in the order Fab builds it: the bold title "Downloading <name>", then the
// percent text inside the bar, then the Cancel button. Nothing else on the screen is read.
struct FToast
{
	bool bTitleSeen = false;
	TSharedPtr<STextBlock> Percent;
	TSharedPtr<SButton> Cancel;
	int32 Visited = 0;
};

void Walk(const TSharedRef<SWidget>& Widget, const FString& Title, FToast& Toast, int32 Depth)
{
	if (Depth > MaxDepth || ++Toast.Visited > MaxWidgets || (Toast.Percent.IsValid() && Toast.Cancel.IsValid()))
	{
		return;
	}
	const FString Type = Widget->GetTypeAsString();
	if (Type == TEXT("STextBlock"))
	{
		const TSharedRef<STextBlock> Text = StaticCastSharedRef<STextBlock>(Widget);
		if (!Toast.bTitleSeen)
		{
			Toast.bTitleSeen = Text->GetText().ToString() == Title;
		}
		else if (!Toast.Percent.IsValid())
		{
			Toast.Percent = Text;
		}
	}
	else if (Toast.bTitleSeen && Type == TEXT("SButton") && !Toast.Cancel.IsValid())
	{
		Toast.Cancel = StaticCastSharedRef<SButton>(Widget);
	}
	FChildren* Children = Widget->GetChildren();
	for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
	{
		Walk(Children->GetChildAt(Index), Title, Toast, Depth + 1);
	}
}

// "42%" in most locales, "42 %" or "42,5 %" in others: the first number in the text.
float ParsePercent(const FString& Text)
{
	FString Digits;
	for (const TCHAR Ch : Text)
	{
		if (FChar::IsDigit(Ch) || Ch == TEXT('.') || Ch == TEXT(','))
		{
			Digits.AppendChar(Ch == TEXT(',') ? TEXT('.') : Ch);
		}
		else if (!Digits.IsEmpty())
		{
			break;
		}
	}
	return Digits.IsEmpty() ? -1.0f : FMath::Clamp(static_cast<float>(FCString::Atof(*Digits)), 0.0f, 100.0f);
}

// The toast for one download, found among Fab's notification windows by its title.
bool FindToast(const FString& AssetName, FToast& OutToast)
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}
	TArray<TSharedRef<SWindow>> Windows;
	FSlateNotificationManager::Get().GetWindows(Windows);
	const FString Title = TEXT("Downloading ") + AssetName;
	for (const TSharedRef<SWindow>& Window : Windows)
	{
		FToast Toast;
		Walk(Window, Title, Toast, 0);
		if (Toast.bTitleSeen)
		{
			OutToast = MoveTemp(Toast);
			return true;
		}
	}
	return false;
}
} // namespace

FNotification Read(const FString& AssetName)
{
	FNotification Result;
	FToast Toast;
	if (AssetName.IsEmpty() || !FindToast(AssetName, Toast))
	{
		return Result;
	}
	Result.bFound = true;
	if (Toast.Percent.IsValid())
	{
		Result.Percent = ParsePercent(Toast.Percent->GetText().ToString());
	}
	Result.bCancellable = Toast.Cancel.IsValid() && Toast.Cancel->IsEnabled();
	return Result;
}

bool PressCancel(const FString& AssetName)
{
	FToast Toast;
	if (AssetName.IsEmpty() || !FindToast(AssetName, Toast) || !Toast.Cancel.IsValid() || !Toast.Cancel->IsEnabled())
	{
		return false;
	}
	Toast.Cancel->SimulateClick();
	return true;
}
} // namespace McpFabDownloadProgress
