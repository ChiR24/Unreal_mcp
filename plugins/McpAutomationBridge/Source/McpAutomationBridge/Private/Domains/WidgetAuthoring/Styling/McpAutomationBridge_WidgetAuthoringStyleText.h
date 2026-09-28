#pragma once

#include "CoreMinimal.h"

#include "Components/Border.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Styling/CoreStyle.h"

// set_style `text` and `fontSize`. They used to apply to a TextBlock only; on a Button
// (whose label is a child text block) or a RichTextBlock they were skipped, and with
// nothing applied the handler fell into read mode and answered success. A content
// widget (Button, Border, SizeBox) whose child is a text block now edits that child.
inline bool McpApplyWidgetText(UWidget *Widget, const TSharedPtr<FJsonObject> &Payload,
                               TArray<TSharedPtr<FJsonValue>> &Applied, FString &OutUnsupported) {
  FString NewText;
  double FontSize = 0.0;
  const bool bText = Payload->TryGetStringField(TEXT("text"), NewText);
  const bool bSize = Payload->TryGetNumberField(TEXT("fontSize"), FontSize) && FontSize > 0.0;
  if (!bText && !bSize) {
    return true;
  }
  UWidget *Target = Widget;
  if (UContentWidget *Content = Cast<UContentWidget>(Widget)) {
    Target = Content->GetContent() ? Content->GetContent() : Widget;
  }
  if (UTextBlock *Text = Cast<UTextBlock>(Target)) {
    if (bSize) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
      FSlateFontInfo Font = Text->GetFont();
#else
      FSlateFontInfo Font = Text->Font;
#endif
      Font.Size = static_cast<int32>(FontSize);
      Text->SetFont(Font);
    }
    if (bText) {
      Text->SetText(FText::FromString(NewText));
    }
  } else if (URichTextBlock *Rich = Cast<URichTextBlock>(Target)) {
    if (bSize) {
      Rich->SetDefaultFont(FCoreStyle::GetDefaultFontStyle("Regular", static_cast<int32>(FontSize)));
    }
    if (bText) {
      Rich->SetText(FText::FromString(NewText));
    }
  } else {
    OutUnsupported = FString::Printf(
        TEXT("%s shows no text that `text` or `fontSize` can change; target the text block itself ")
        TEXT("(a button's label is its child)."), *Widget->GetClass()->GetName());
    return false;
  }
  if (bSize) {
    Applied.Add(MakeShared<FJsonValueString>(TEXT("fontSize")));
  }
  if (bText) {
    Applied.Add(MakeShared<FJsonValueString>(TEXT("text")));
  }
  return true;
}
