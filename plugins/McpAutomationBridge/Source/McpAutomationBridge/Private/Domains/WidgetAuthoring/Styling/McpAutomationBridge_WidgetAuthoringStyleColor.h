#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Styling/SlateColor.h"

// `colorAndOpacity` on set_style used to be honoured for UTextBlock only. On any
// other widget the field was quietly ignored, nothing was applied, and the call
// fell through to the generic reflection path -- which, with no propertyName to
// write, READ the style back and answered "set_style property read" with
// success:true. Asking to recolour a button therefore reported success and
// changed nothing. Every widget type below exposes its own tint under a
// different name; map the one field onto whichever the target actually has.
inline bool McpApplyWidgetStyleColor(UWidget *Widget, const FLinearColor &Color,
                                     FString &OutPropertyName) {
  if (UTextBlock *Text = Cast<UTextBlock>(Widget)) {
    Text->SetColorAndOpacity(FSlateColor(Color));
    OutPropertyName = TEXT("ColorAndOpacity");
    return true;
  }
  if (UButton *Button = Cast<UButton>(Widget)) {
    Button->SetBackgroundColor(Color);
    OutPropertyName = TEXT("BackgroundColor");
    return true;
  }
  if (UImage *Image = Cast<UImage>(Widget)) {
    Image->SetColorAndOpacity(Color);
    OutPropertyName = TEXT("ColorAndOpacity");
    return true;
  }
  if (UBorder *Border = Cast<UBorder>(Widget)) {
    Border->SetBrushColor(Color);
    OutPropertyName = TEXT("BrushColor");
    return true;
  }
  if (UProgressBar *Bar = Cast<UProgressBar>(Widget)) {
    Bar->SetFillColorAndOpacity(Color);
    OutPropertyName = TEXT("FillColorAndOpacity");
    return true;
  }
  return false;
}

// The whole convenience surface of set_style (fontSize, text, colorAndOpacity,
// renderOpacity) in one place, so the handler is a call plus a refusal instead
// of a forty-line ladder. Returns false only when a field was asked for and the
// target widget cannot honour it -- the caller must then refuse rather than
// fall through to the generic reflection path, which would answer success.
inline bool McpApplyWidgetStyleConvenience(
    UWidget *Widget, const TSharedPtr<FJsonObject> &Payload,
    const TSharedPtr<FJsonObject> &ResultJson,
    TArray<TSharedPtr<FJsonValue>> &Applied, FString &OutUnsupported) {
  if (UTextBlock *Text = Cast<UTextBlock>(Widget)) {
    double FontSize = 0.0;
    if (Payload->TryGetNumberField(TEXT("fontSize"), FontSize) && FontSize > 0.0) {
      FSlateFontInfo Font = Text->GetFont();
      Font.Size = static_cast<int32>(FontSize);
      Text->SetFont(Font);
      Applied.Add(MakeShared<FJsonValueString>(TEXT("fontSize")));
    }
    FString NewText;
    if (Payload->TryGetStringField(TEXT("text"), NewText)) {
      Text->SetText(FText::FromString(NewText));
      Applied.Add(MakeShared<FJsonValueString>(TEXT("text")));
    }
  }
  const TSharedPtr<FJsonObject> *ColorObj = nullptr;
  if (Payload->TryGetObjectField(TEXT("colorAndOpacity"), ColorObj) && ColorObj &&
      (*ColorObj).IsValid()) {
    auto Channel = [&ColorObj](const TCHAR *Key) {
      return (*ColorObj)->HasField(Key) ? (*ColorObj)->GetNumberField(Key) : 1.0;
    };
    FString ColorProperty;
    const FLinearColor Color(Channel(TEXT("r")), Channel(TEXT("g")),
                             Channel(TEXT("b")), Channel(TEXT("a")));
    if (!McpApplyWidgetStyleColor(Widget, Color, ColorProperty)) {
      OutUnsupported = FString::Printf(
          TEXT("%s has no colour that `colorAndOpacity` maps to; pass ")
          TEXT("propertyName/value to write one of its style properties directly."),
          *Widget->GetClass()->GetName());
      return false;
    }
    ResultJson->SetStringField(TEXT("colorProperty"), ColorProperty);
    Applied.Add(MakeShared<FJsonValueString>(TEXT("colorAndOpacity")));
  }
  double RenderOpacity = 0.0;
  if (Payload->TryGetNumberField(TEXT("renderOpacity"), RenderOpacity)) {
    Widget->SetRenderOpacity(static_cast<float>(RenderOpacity));
    Applied.Add(MakeShared<FJsonValueString>(TEXT("renderOpacity")));
  }
  return true;
}
#endif
