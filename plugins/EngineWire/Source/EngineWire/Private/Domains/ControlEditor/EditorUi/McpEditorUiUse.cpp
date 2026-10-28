// use_editor_ui: one action on one widget, as Slate input from a virtual user with its own pointer and focus. Pointer
// events go down the widget's own path (FindPathToWidget arranges it without painting), so the system cursor never
// moves, nothing on screen is hit-tested and a minimized editor works. The reply says what changed: the widget's text,
// where that user's focus went, and the windows (menus and popups among them) that opened or closed.

#include "Domains/ControlEditor/EditorUi/McpEditorUi.h"

#include "Dom/JsonObject.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Input/Events.h"
#include "Layout/WidgetPath.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SWindow.h"

namespace
{
// A virtual user index of its own, so no widget component's virtual user (they start at 0) shares its focus.
constexpr int32 UiVirtualUser = 7;

uint32 UiUserIndex()
{
	static TSharedPtr<FSlateVirtualUserHandle> Handle;
	if (!Handle.IsValid())
	{
		Handle = FSlateApplication::Get().FindOrCreateVirtualUser(UiVirtualUser);
	}
	return static_cast<uint32>(Handle->GetUserIndex());
}

// "Ctrl+Shift+S" as its key and modifiers; false when the key has no such name.
bool UiParseKey(const FString& Chord, FKey& OutKey, FModifierKeysState& OutModifiers)
{
	TArray<FString> Parts;
	Chord.ParseIntoArray(Parts, TEXT("+"));
	bool bShift = false, bCtrl = false, bAlt = false, bCommand = false;
	FString Name;
	for (const FString& Part : Parts)
	{
		const FString Each = Part.TrimStartAndEnd();
		bool* Modifier = Each.Equals(TEXT("Ctrl")) || Each.Equals(TEXT("Control")) ? &bCtrl : Each.Equals(TEXT("Shift")) ? &bShift
			: Each.Equals(TEXT("Alt")) ? &bAlt : Each.Equals(TEXT("Cmd")) || Each.Equals(TEXT("Command")) ? &bCommand : nullptr;
		if (Modifier)
		{
			*Modifier = true;
		}
		else
		{
			Name = Each;
		}
	}
	OutKey = FKey(*Name);
	OutModifiers = FModifierKeysState(bShift, false, bCtrl, false, bAlt, false, bCommand, false, false);
	return !Name.IsEmpty() && OutKey.IsValid();
}

bool UiSendKey(uint32 User, const FKey& Key, const FModifierKeysState& Modifiers)
{
	FSlateApplication& Slate = FSlateApplication::Get();
	const bool bDown = Slate.ProcessKeyDownEvent(FKeyEvent(Key, Modifiers, User, false, 0, 0));
	return Slate.ProcessKeyUpEvent(FKeyEvent(Key, Modifiers, User, false, 0, 0)) || bDown;
}

// A path FindPathToWidget arranged carries no pointer position per widget, and routing a pointer event reads one for
// every widget on it (an index out of bounds otherwise): the same widgets as a hit-test path, with none.
FWidgetPath UiPointerPath(const FWidgetPath& Arranged)
{
	TArray<FWidgetAndPointer> Widgets;
	for (int32 Index = 0; Index < Arranged.Widgets.Num(); ++Index)
	{
		Widgets.Add(FWidgetAndPointer(Arranged.Widgets[Index]));
	}
	return FWidgetPath(Widgets);
}

// Move onto the widget's middle, then press and release (twice for a double click, the second press a double click).
bool UiPointer(uint32 User, const FWidgetPath& Arranged, const FString& Do)
{
	FSlateApplication& Slate = FSlateApplication::Get();
	const FWidgetPath Path = UiPointerPath(Arranged);
	const FVector2D At = FVector2D(Path.Widgets.Last().Geometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f)));
	const TSet<FKey> None;
	bool bHandled = Slate.RoutePointerMoveEvent(Path, FPointerEvent(User, 0, At, At, None, EKeys::Invalid, 0.0f, FModifierKeysState()), false);
	if (Do == TEXT("hover"))
	{
		return bHandled;
	}
	const FKey Button = Do == TEXT("right_click") ? EKeys::RightMouseButton : EKeys::LeftMouseButton;
	TSet<FKey> Pressed;
	Pressed.Add(Button);
	const FPointerEvent Down(User, 0, At, At, Pressed, Button, 0.0f, FModifierKeysState());
	const FPointerEvent Up(User, 0, At, At, None, Button, 0.0f, FModifierKeysState());
	bHandled |= Slate.RoutePointerDownEvent(Path, Down).IsEventHandled();
	bHandled |= Slate.RoutePointerUpEvent(Path, Up).IsEventHandled();
	if (Do == TEXT("double_click"))
	{
		bHandled |= Slate.RoutePointerDoubleClickEvent(Path, Down).IsEventHandled();
		bHandled |= Slate.RoutePointerUpEvent(Path, Up).IsEventHandled();
	}
	return bHandled;
}

TSharedPtr<FJsonObject> UiWidgetJson(const TSharedPtr<SWidget>& Widget)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	if (Widget.IsValid())
	{
		Json->SetStringField(TEXT("ref"), McpEditorUi::RefOf(Widget.ToSharedRef()));
		Json->SetStringField(TEXT("type"), Widget->GetTypeAsString());
		Json->SetStringField(TEXT("text"), McpEditorUi::TextOf(Widget.ToSharedRef()));
	}
	return Json;
}

// Titles of the windows in A that B lacks (an untitled one is a menu or popup).
TArray<FString> UiWindowsNotIn(const TArray<TSharedRef<SWindow>>& A, const TArray<TSharedRef<SWindow>>& B)
{
	TArray<FString> Titles;
	for (const TSharedRef<SWindow>& Window : A)
	{
		if (!B.Contains(Window))
		{
			const FString Title = Window->GetTitle().ToString();
			Titles.Add(Title.IsEmpty() ? FString::Printf(TEXT("%s (untitled: a menu or popup)"), *McpEditorUi::RefOf(Window)) : Title);
		}
	}
	return Titles;
}
} // namespace

namespace McpEditorUi
{
bool HandleUse(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FUiSocket Socket)
{
	FString Ref, Do, Text, Key;
	bool bSubmit = false;
	Payload->TryGetStringField(TEXT("ref"), Ref);
	Payload->TryGetStringField(TEXT("do"), Do);
	Payload->TryGetStringField(TEXT("text"), Text);
	Payload->TryGetStringField(TEXT("key"), Key);
	Payload->TryGetBoolField(TEXT("submit"), bSubmit);
	const auto Refuse = [&](const FString& Message, const TCHAR* Code)
	{
		Self->SendAutomationError(Socket, RequestId, Message + TEXT(" Nothing was done."), Code);
		return true;
	};
	bool bKnown = false;
	const TSharedPtr<SWidget> Widget = Resolve(Ref, bKnown);
	if (!Widget.IsValid())
	{
		return Refuse(bKnown ? FString::Printf(TEXT("%s is gone: its widget was closed since it was read."), *Ref)
			: FString::Printf(TEXT("No widget has the ref '%s'; refs come from read_editor_ui."), *Ref),
			bKnown ? TEXT("WIDGET_GONE") : TEXT("WIDGET_NOT_FOUND"));
	}
	FKey Chord;
	FModifierKeysState Modifiers;
	const bool bCheck = Do == TEXT("check") || Do == TEXT("uncheck");
	const bool bIsCheckBox = Widget->GetType() == FName(TEXT("SCheckBox"));
	if ((Do == TEXT("type") && Text.IsEmpty()) || (Do == TEXT("press_key") && !UiParseKey(Key, Chord, Modifiers)) || (bCheck && !bIsCheckBox))
	{
		return Refuse(Do == TEXT("type") ? FString(TEXT("type needs text.")) : bCheck ? FString::Printf(TEXT("%s is a %s, not a "
			"check box."), *Ref, *Widget->GetTypeAsString()) : FString::Printf(TEXT("'%s' is no key (Enter, Escape, F5, "
			"Ctrl+S...)."), *Key), TEXT("INVALID_ARGUMENT"));
	}
	FWidgetPath Path;
	if (!FSlateApplication::Get().FindPathToWidget(Widget.ToSharedRef(), Path) || !Path.IsValid())
	{
		return Refuse(FString::Printf(TEXT("%s is not shown: no window holds it on screen now (a closed tab, a collapsed "
			"panel)."), *Ref), TEXT("WIDGET_NOT_SHOWN"));
	}
	if (Do != TEXT("hover") && !Widget->IsEnabled())
	{
		return Refuse(FString::Printf(TEXT("%s is disabled."), *Ref), TEXT("WIDGET_DISABLED"));
	}
	TArray<TSharedRef<SWindow>> Before, After;
	EnumerateEditorSlateWindowsForMcp(Before);
	const uint32 User = UiUserIndex();
	FSlateApplication& Slate = FSlateApplication::Get();
	bool bHandled = false;
	bool bAlready = false;
	if (Do == TEXT("focus") || Do == TEXT("type") || Do == TEXT("press_key"))
	{
		bHandled = Slate.SetUserFocus(User, Widget, EFocusCause::SetDirectly);
		for (const TCHAR Character : Do == TEXT("type") ? Text : FString())
		{
			bHandled |= Slate.ProcessKeyCharEvent(FCharacterEvent(Character, FModifierKeysState(), User, false));
		}
		if ((Do == TEXT("type") && bSubmit) || Do == TEXT("press_key"))
		{
			bHandled |= UiSendKey(User, Do == TEXT("type") ? EKeys::Enter : Chord, Do == TEXT("type") ? FModifierKeysState() : Modifiers);
		}
	}
	else if (bCheck && StaticCastSharedPtr<SCheckBox>(Widget)->IsChecked() == (Do == TEXT("check")))
	{
		bAlready = true;
	}
	else
	{
		bHandled = UiPointer(User, Path, bCheck ? FString(TEXT("click")) : Do);
	}
	EnumerateEditorSlateWindowsForMcp(After);
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetStringField(TEXT("did"), bAlready ? FString::Printf(TEXT("nothing: %s already"), *Do) : Do);
	Result->SetObjectField(TEXT("widget"), UiWidgetJson(Widget));
	Result->SetBoolField(TEXT("handled"), bHandled);
	Result->SetObjectField(TEXT("focus"), UiWidgetJson(Slate.GetUserFocusedWidget(User)));
	Result->SetArrayField(TEXT("windowsOpened"), McpHandlerUtils::ToJsonStringArray(UiWindowsNotIn(After, Before)));
	Result->SetArrayField(TEXT("windowsClosed"), McpHandlerUtils::ToJsonStringArray(UiWindowsNotIn(Before, After)));
	Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%s %s (%s)%s."), *Do, *Ref,
		*Widget->GetTypeAsString(), bHandled || bAlready || Do == TEXT("hover") ? TEXT("") : TEXT(": the widget did not handle it")), Result);
	return true;
}
} // namespace McpEditorUi
