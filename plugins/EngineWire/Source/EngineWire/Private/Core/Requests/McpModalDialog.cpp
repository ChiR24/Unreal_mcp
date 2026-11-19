#include "Core/Requests/McpModalDialog.h"

#include "Dom/JsonValue.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace McpModalDialogParts
{
constexpr int32 MaxDepth = 48;
constexpr int32 MaxMessage = 1000;
constexpr int32 MaxButtons = 12;

// The words a widget and its children show, depth first. A button's words are its label, not part of the message: a
// widget whose type ends in Button (SButton, SPrimaryButton, ...) is read whole and not walked further.
void Collect(const TSharedRef<SWidget>& Widget, TArray<FString>& OutTexts, TArray<TSharedPtr<FJsonValue>>& OutButtons, int32 Depth)
{
    if (Depth > MaxDepth)
    {
        return;
    }
    const bool bButton = Widget->GetTypeAsString().EndsWith(TEXT("Button"));
    TArray<FString> Label;
    TArray<FString>& Into = bButton ? Label : OutTexts;
    if (Widget->GetType() == FName(TEXT("STextBlock")))
    {
        const FString Text = StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString().TrimStartAndEnd();
        if (!Text.IsEmpty())
        {
            Into.Add(Text);
        }
    }
    if (FChildren* Children = Widget->GetChildren())
    {
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            Collect(Children->GetChildAt(Index), Into, OutButtons, Depth + 1);
        }
    }
    // A title bar's close and minimize buttons carry no words: leaving them out keeps the list to the dialog's choices.
    if (bButton && Label.Num() > 0 && OutButtons.Num() < MaxButtons)
    {
        TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("label"), FString::Join(Label, TEXT(" ")));
        Row->SetBoolField(TEXT("enabled"), Widget->IsEnabled());
        OutButtons.Add(MakeShared<FJsonValueObject>(Row));
    }
}
} // namespace McpModalDialogParts

TSharedPtr<FJsonObject> McpModalDialog::Describe(const TSharedPtr<SWindow>& Window)
{
    // ponytail: one cached window, game thread only (the modal tick and the request door both run there).
    static TWeakPtr<SWindow> CachedWindow;
    static TSharedPtr<FJsonObject> Cached;
    if (!Window.IsValid())
    {
        return nullptr;
    }
    if (Cached.IsValid() && CachedWindow.Pin() == Window)
    {
        return Cached;
    }
    const FString Title = Window->GetTitle().ToString();
    TArray<FString> Texts;
    TArray<TSharedPtr<FJsonValue>> Buttons;
    McpModalDialogParts::Collect(Window.ToSharedRef(), Texts, Buttons, 0);
    Texts.RemoveAll([&Title](const FString& Text) { return Text == Title; }); // the title bar repeats it
    FString Message = FString::Join(Texts, TEXT("\n"));
    if (Message.Len() > McpModalDialogParts::MaxMessage)
    {
        Message = Message.Left(McpModalDialogParts::MaxMessage) + TEXT("...");
    }
    Cached = MakeShared<FJsonObject>();
    Cached->SetStringField(TEXT("title"), Title);
    Cached->SetStringField(TEXT("message"), Message);
    Cached->SetArrayField(TEXT("buttons"), Buttons);
    CachedWindow = Window;
    return Cached;
}
