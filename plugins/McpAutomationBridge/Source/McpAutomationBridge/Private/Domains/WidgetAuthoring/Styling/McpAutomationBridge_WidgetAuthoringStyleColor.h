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
#include "Styling/SlateBrush.h"
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

// A flat rectangle is what every UMG panel is by default, and nothing on the
// published surface could change that -- so a caller asked to "polish" a UI had
// no way to round a single corner without hand-editing the asset. UMG has had
// RoundedBox brushes since 5.0; expose them.
inline void McpApplyBrushRounding(FSlateBrush &Brush, float Radius,
                                  const FLinearColor &OutlineColor,
                                  float OutlineWidth) {
  FSlateBrushOutlineSettings Outline;
  Outline.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
  Outline.RoundingType = ESlateBrushRoundingType::FixedRadius;
  Outline.Color = FSlateColor(OutlineColor);
  Outline.Width = OutlineWidth;
  Brush.OutlineSettings = Outline;
  Brush.DrawAs = Radius > 0.0f ? ESlateBrushDrawType::RoundedBox
                               : ESlateBrushDrawType::Box;
}

// Rounds whichever brush the widget actually draws with. A Button carries four
// (normal/hovered/pressed/disabled) and rounding only one of them makes the
// corners pop square on hover, so all four move together.
inline bool McpApplyWidgetCornerRadius(UWidget *Widget, float Radius,
                                       const FLinearColor &OutlineColor,
                                       float OutlineWidth,
                                       FString &OutPropertyName) {
  if (UImage *Image = Cast<UImage>(Widget)) {
    FSlateBrush Brush = Image->GetBrush();
    McpApplyBrushRounding(Brush, Radius, OutlineColor, OutlineWidth);
    Image->SetBrush(Brush);
    OutPropertyName = TEXT("Brush");
    return true;
  }
  if (UButton *Button = Cast<UButton>(Widget)) {
    FButtonStyle Style = Button->GetStyle();
    McpApplyBrushRounding(Style.Normal, Radius, OutlineColor, OutlineWidth);
    McpApplyBrushRounding(Style.Hovered, Radius, OutlineColor, OutlineWidth);
    McpApplyBrushRounding(Style.Pressed, Radius, OutlineColor, OutlineWidth);
    McpApplyBrushRounding(Style.Disabled, Radius, OutlineColor, OutlineWidth);
    Button->SetStyle(Style);
    OutPropertyName = TEXT("WidgetStyle");
    return true;
  }
  if (UBorder *Border = Cast<UBorder>(Widget)) {
    FSlateBrush Brush = Border->Background;
    McpApplyBrushRounding(Brush, Radius, OutlineColor, OutlineWidth);
    Border->SetBrush(Brush);
    OutPropertyName = TEXT("Background");
    return true;
  }
  return false;
}

// Re-points the texture of whichever brush the widget draws with. Until this
// existed nothing on the published surface could change an EXISTING widget's
// image: `add_content_widget` takes texturePath only while creating one, so a
// caller who wanted to swap an icon had to add a second Image over the first
// and collapse the original. Same four-brush rule as the rounding above -- a
// Button that only re-textures Normal flips back to the old art on hover.
inline bool McpApplyWidgetBrushTexture(UWidget *Widget, UObject *Texture,
                                       FString &OutPropertyName) {
  if (UImage *Image = Cast<UImage>(Widget)) {
    Image->SetBrushResourceObject(Texture);
    OutPropertyName = TEXT("Brush");
    return true;
  }
  if (UButton *Button = Cast<UButton>(Widget)) {
    FButtonStyle Style = Button->GetStyle();
    Style.Normal.SetResourceObject(Texture);
    Style.Hovered.SetResourceObject(Texture);
    Style.Pressed.SetResourceObject(Texture);
    Style.Disabled.SetResourceObject(Texture);
    Button->SetStyle(Style);
    OutPropertyName = TEXT("WidgetStyle");
    return true;
  }
  if (UBorder *Border = Cast<UBorder>(Widget)) {
    // Through the brush rather than SetBrushFromTexture, which rebuilds it from
    // scratch and would drop any rounding or tint already set on this border.
    FSlateBrush Brush = Border->Background;
    Brush.SetResourceObject(Texture);
    Border->SetBrush(Brush);
    OutPropertyName = TEXT("Background");
    return true;
  }
  return false;
}

// The whole convenience surface of set_style (fontSize, text, texturePath,
// colorAndOpacity, renderOpacity) in one place, so the handler is a call plus a
// refusal instead of a forty-line ladder. Returns false only when a field was
// asked for and the target widget cannot honour it -- the caller must then
// refuse rather than fall through to the generic reflection path, which would
// answer success.
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
  double CornerRadius = 0.0;
  if (Payload->TryGetNumberField(TEXT("cornerRadius"), CornerRadius)) {
    const TSharedPtr<FJsonObject> *OutlineObj = nullptr;
    FLinearColor OutlineColor(1.0f, 1.0f, 1.0f, 0.0f);
    if (Payload->TryGetObjectField(TEXT("outlineColor"), OutlineObj) && OutlineObj &&
        (*OutlineObj).IsValid()) {
      auto Ch = [&OutlineObj](const TCHAR *Key) {
        return (*OutlineObj)->HasField(Key) ? (*OutlineObj)->GetNumberField(Key) : 1.0;
      };
      OutlineColor = FLinearColor(Ch(TEXT("r")), Ch(TEXT("g")), Ch(TEXT("b")),
                                  Ch(TEXT("a")));
    }
    double OutlineWidth = 0.0;
    Payload->TryGetNumberField(TEXT("outlineWidth"), OutlineWidth);
    FString BrushProperty;
    if (!McpApplyWidgetCornerRadius(Widget, static_cast<float>(CornerRadius),
                                    OutlineColor, static_cast<float>(OutlineWidth),
                                    BrushProperty)) {
      OutUnsupported = FString::Printf(
          TEXT("%s draws no brush that `cornerRadius` can round; only Image, ")
          TEXT("Button and Border carry one."),
          *Widget->GetClass()->GetName());
      return false;
    }
    ResultJson->SetStringField(TEXT("brushProperty"), BrushProperty);
    Applied.Add(MakeShared<FJsonValueString>(TEXT("cornerRadius")));
  }
  FString TexturePath;
  if (Payload->TryGetStringField(TEXT("texturePath"), TexturePath) &&
      !TexturePath.IsEmpty()) {
    UObject *Texture = StaticLoadObject(UObject::StaticClass(), nullptr,
                                        *TexturePath);
    if (!Texture) {
      OutUnsupported = FString::Printf(
          TEXT("`texturePath` %s could not be loaded; pass a canonical asset ")
          TEXT("path such as /Game/UI/Icons/T_Icon_Coin."),
          *TexturePath);
      return false;
    }
    FString BrushProperty;
    if (!McpApplyWidgetBrushTexture(Widget, Texture, BrushProperty)) {
      OutUnsupported = FString::Printf(
          TEXT("%s draws no brush that `texturePath` can re-point; only Image, ")
          TEXT("Button and Border carry one."),
          *Widget->GetClass()->GetName());
      return false;
    }
    ResultJson->SetStringField(TEXT("textureProperty"), BrushProperty);
    Applied.Add(MakeShared<FJsonValueString>(TEXT("texturePath")));
  }
  double RenderOpacity = 0.0;
  if (Payload->TryGetNumberField(TEXT("renderOpacity"), RenderOpacity)) {
    Widget->SetRenderOpacity(static_cast<float>(RenderOpacity));
    Applied.Add(MakeShared<FJsonValueString>(TEXT("renderOpacity")));
  }
  return true;
}
#endif
