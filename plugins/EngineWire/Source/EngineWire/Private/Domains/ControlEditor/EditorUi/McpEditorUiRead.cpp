// read_editor_ui: the editor's windows, and an outline of their widgets or of one branch. Layout-only widgets fold
// away (their children move up a level), so the outline holds what a person reads or uses, each with its ref.

#include "Domains/ControlEditor/EditorUi/McpEditorUi.h"

#include "Dom/JsonObject.h"
#include "Domains/ControlEditor/McpEditorScreenshotSupport.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Widgets/SWindow.h"

namespace
{
constexpr int32 UiWalkLimit = 256;

struct FUiPendingLine
{
	FString Line;
	bool bEmitted = false;
};

struct FUiOutline
{
	FString Filter;
	int32 MaxDepth = 12;
	int32 MaxWidgets = 300;
	int32 Listed = 0;
	bool bTruncated = false;
	TArray<FString> Lines;
	// The listed widgets above the one being walked: with a text filter they are written only above a match.
	TArray<FUiPendingLine> Above;
};

void UiEmitAbove(FUiOutline& Out)
{
	for (FUiPendingLine& Pending : Out.Above)
	{
		if (Pending.bEmitted)
		{
			continue;
		}
		if (Out.Listed >= Out.MaxWidgets)
		{
			Out.bTruncated = true;
			return;
		}
		Out.Lines.Add(Pending.Line);
		Pending.bEmitted = true;
		++Out.Listed;
	}
}

void UiWalk(FUiOutline& Out, const TSharedRef<SWidget>& Widget, int32 Depth, int32 Walked)
{
	if (Out.bTruncated || Walked > UiWalkLimit || !Widget->GetVisibility().IsVisible())
	{
		return;
	}
	const FString Text = McpEditorUi::TextOf(Widget);
	const bool bListed = McpEditorUi::IsListed(Widget, Text);
	if (bListed)
	{
		Out.Above.Add({FString::ChrN(Depth * 2, TEXT(' ')) + McpEditorUi::RefOf(Widget) + TEXT(" ") + Widget->GetTypeAsString() +
			(Text.IsEmpty() ? FString() : FString::Printf(TEXT(" \"%s\""), *Text)) + McpEditorUi::FlagsOf(Widget), false});
		if (Out.Filter.IsEmpty() || Text.Contains(Out.Filter))
		{
			UiEmitAbove(Out);
		}
	}
	const int32 ChildDepth = bListed ? Depth + 1 : Depth;
	FChildren* Children = ChildDepth <= Out.MaxDepth ? Widget->GetChildren() : nullptr;
	for (int32 Index = 0; Children && Index < Children->Num(); ++Index)
	{
		UiWalk(Out, Children->GetChildAt(Index), ChildDepth, Walked + 1);
	}
	if (bListed)
	{
		Out.Above.Pop();
	}
}
} // namespace

namespace McpEditorUi
{
bool HandleRead(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FUiSocket Socket)
{
	FString WindowQuery, From;
	FUiOutline Out;
	double Depth = 12, MaxWidgets = 300;
	Payload->TryGetStringField(TEXT("window"), WindowQuery);
	Payload->TryGetStringField(TEXT("from"), From);
	Payload->TryGetStringField(TEXT("text"), Out.Filter);
	Payload->TryGetNumberField(TEXT("depth"), Depth);
	Payload->TryGetNumberField(TEXT("maxWidgets"), MaxWidgets);
	Out.MaxDepth = FMath::Clamp(static_cast<int32>(Depth), 1, 40);
	Out.MaxWidgets = FMath::Clamp(static_cast<int32>(MaxWidgets), 1, 1000);

	TArray<TSharedRef<SWindow>> Windows;
	EnumerateEditorSlateWindowsForMcp(Windows);
	TArray<TSharedPtr<FJsonValue>> WindowRows;
	for (int32 Index = 0; Index < Windows.Num(); ++Index)
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("index"), Index);
		Row->SetStringField(TEXT("title"), Windows[Index]->GetTitle().ToString());
		Row->SetStringField(TEXT("ref"), RefOf(Windows[Index]));
		WindowRows.Add(MakeShared<FJsonValueObject>(Row));
	}
	TArray<TSharedRef<SWidget>> Roots;
	if (!From.IsEmpty())
	{
		bool bKnown = false;
		const TSharedPtr<SWidget> Branch = Resolve(From, bKnown);
		if (!Branch.IsValid())
		{
			Self->SendAutomationError(Socket, RequestId, bKnown ? FString::Printf(TEXT("%s is gone: its widget was closed "
				"since it was read. Read the UI again."), *From) : FString::Printf(TEXT("No widget has the ref '%s'; refs come "
				"from read_editor_ui."), *From), bKnown ? TEXT("WIDGET_GONE") : TEXT("WIDGET_NOT_FOUND"));
			return true;
		}
		Roots.Add(Branch.ToSharedRef());
	}
	else if (!WindowQuery.IsEmpty())
	{
		FString Title, Error;
		const TSharedPtr<SWindow> Window = FindEditorSlateWindowForMcp(WindowQuery, Title, Error);
		if (!Window.IsValid())
		{
			Self->SendAutomationError(Socket, RequestId, Error, TEXT("WINDOW_NOT_FOUND"));
			return true;
		}
		Roots.Add(Window.ToSharedRef());
	}
	else
	{
		for (const TSharedRef<SWindow>& Window : Windows)
		{
			Roots.Add(Window);
		}
	}
	for (const TSharedRef<SWidget>& Root : Roots)
	{
		UiWalk(Out, Root, 0, 0);
	}
	TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
	Result->SetArrayField(TEXT("windows"), WindowRows);
	Result->SetStringField(TEXT("outline"), FString::Join(Out.Lines, TEXT("\n")));
	Result->SetNumberField(TEXT("listed"), Out.Listed);
	Result->SetBoolField(TEXT("truncated"), Out.bTruncated);
	Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%d widget(s) listed%s."), Out.Listed,
		Out.bTruncated ? TEXT(", more beyond maxWidgets: narrow with text, from or window") : TEXT("")), Result);
	return true;
}
} // namespace McpEditorUi
