// Widget refs and what a widget says. A ref is a number handed out once per widget and kept for the editor session
// (a weak pointer each way), so a widget read twice keeps its ref and one destroyed since reads as gone.

#include "Domains/ControlEditor/EditorUi/McpEditorUi.h"

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
constexpr int32 UiTextShown = 120;
constexpr int32 UiRefsBeforePrune = 20000;

struct FUiRefs
{
	TMap<int32, TWeakPtr<SWidget>> ByRef;
	TMap<const SWidget*, int32> ByWidget;
	int32 Next = 1;
};

FUiRefs& UiRefs()
{
	static FUiRefs Refs;
	return Refs;
}

bool UiTypeIs(const TSharedRef<SWidget>& Widget, const TCHAR* Type)
{
	return Widget->GetType() == FName(Type);
}

// Controls a person uses; templated ones (SComboBox<T>, SSpinBox<T>) are named with their type argument. A combo
// button is left out: the button inside it takes the click and carries the same tooltip.
bool UiIsControl(const TSharedRef<SWidget>& Widget)
{
	static const TCHAR* const Controls[] = {TEXT("SButton"), TEXT("SCheckBox"), TEXT("SComboBox"),
		TEXT("SSpinBox"), TEXT("SNumericEntryBox"), TEXT("SSlider"), TEXT("SHyperlink"), TEXT("SDockTab"), TEXT("SWindow"),
		TEXT("SSearchBox"), TEXT("SEditableText"), TEXT("SEditableTextBox"), TEXT("SMultiLineEditableTextBox")};
	const FString Type = Widget->GetTypeAsString();
	for (const TCHAR* Control : Controls)
	{
		if (Type.StartsWith(Control) && (Type.Len() == FCString::Strlen(Control) || Type[FCString::Strlen(Control)] == TEXT('<')))
		{
			return true;
		}
	}
	return false;
}
} // namespace

namespace McpEditorUi
{
FString RefOf(const TSharedRef<SWidget>& Widget)
{
	FUiRefs& Refs = UiRefs();
	if (const int32* Known = Refs.ByWidget.Find(&Widget.Get()))
	{
		if (Refs.ByRef.FindRef(*Known).Pin().Get() == &Widget.Get())
		{
			return FString::Printf(TEXT("e%d"), *Known);
		}
	}
	if (Refs.ByRef.Num() > UiRefsBeforePrune)
	{
		for (auto It = Refs.ByRef.CreateIterator(); It; ++It)
		{
			if (!It.Value().IsValid())
			{
				It.RemoveCurrent();
			}
		}
		for (auto It = Refs.ByWidget.CreateIterator(); It; ++It)
		{
			if (!Refs.ByRef.Contains(It.Value()))
			{
				It.RemoveCurrent();
			}
		}
	}
	const int32 Ref = Refs.Next++;
	Refs.ByRef.Add(Ref, TWeakPtr<SWidget>(Widget));
	Refs.ByWidget.Add(&Widget.Get(), Ref);
	return FString::Printf(TEXT("e%d"), Ref);
}

TSharedPtr<SWidget> Resolve(const FString& Ref, bool& bOutKnown)
{
	const FString Number = Ref.StartsWith(TEXT("e")) ? Ref.RightChop(1) : FString();
	const TWeakPtr<SWidget>* Weak = !Number.IsEmpty() && Number.IsNumeric() ? UiRefs().ByRef.Find(FCString::Atoi(*Number)) : nullptr;
	bOutKnown = Weak != nullptr;
	return Weak ? Weak->Pin() : nullptr;
}

FString TextOf(const TSharedRef<SWidget>& Widget)
{
	FText Text;
	if (UiTypeIs(Widget, TEXT("STextBlock")))
	{
		Text = StaticCastSharedRef<STextBlock>(Widget)->GetText();
	}
	else if (UiTypeIs(Widget, TEXT("SEditableText")))
	{
		Text = StaticCastSharedRef<SEditableText>(Widget)->GetText();
	}
	else if (UiTypeIs(Widget, TEXT("SEditableTextBox")) || UiTypeIs(Widget, TEXT("SSearchBox")))
	{
		Text = StaticCastSharedRef<SEditableTextBox>(Widget)->GetText();
	}
	else if (UiTypeIs(Widget, TEXT("SMultiLineEditableTextBox")))
	{
		Text = StaticCastSharedRef<SMultiLineEditableTextBox>(Widget)->GetText();
	}
	else if (UiIsControl(Widget))
	{
		// A control's words are usually a text block inside it (listed on its own); say what an icon-only one does.
		const TSharedPtr<IToolTip> Tip = Widget->GetToolTip();
		const TSharedPtr<SWidget> TipWidget = Tip.IsValid() ? Tip->AsWidget() : TSharedPtr<SWidget>();
		if (TipWidget.IsValid() && TipWidget->GetType() == FName(TEXT("SToolTip")))
		{
			Text = StaticCastSharedPtr<SToolTip>(TipWidget)->GetTextTooltip();
		}
	}
	FString Out = Text.ToString().Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
	return Out.Len() > UiTextShown ? Out.Left(UiTextShown) + TEXT("...") : Out;
}

bool IsListed(const TSharedRef<SWidget>& Widget, const FString& Text)
{
	return !Text.IsEmpty() || UiIsControl(Widget) || Widget->SupportsKeyboardFocus();
}

FString FlagsOf(const TSharedRef<SWidget>& Widget)
{
	FString Flags;
	if (!Widget->IsEnabled())
	{
		Flags += TEXT(" [disabled]");
	}
	if (Widget->HasAnyUserFocus().IsSet())
	{
		Flags += TEXT(" [focused]");
	}
	if (UiTypeIs(Widget, TEXT("SCheckBox")))
	{
		Flags += StaticCastSharedRef<SCheckBox>(Widget)->IsChecked() ? TEXT(" [checked]") : TEXT(" [unchecked]");
	}
	return Flags;
}
} // namespace McpEditorUi
