#pragma once

#include "CoreMinimal.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Dom/JsonObject.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

inline FSlateFontInfo McpTextBlockFont(const UTextBlock *Text) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
  return Text->GetFont();
#else
  return Text->Font;
#endif
}

// set_style's font fields on a text block: fontFamily (a Font asset), typeface (one
// of that font's faces, checked, so "Bold" on a font without it is refused with the
// faces it has), letterSpacing, and copyStyleFrom (another widget's font, colour and
// shadow). There was no way to pick a face or match one label to another at all.
inline bool McpApplyTextFont(UTextBlock *Text, const TSharedPtr<FJsonObject> &Payload,
                             TArray<TSharedPtr<FJsonValue>> &Applied, FString &OutUnsupported) {
  FString Family, Typeface, CopyFrom;
  double Spacing = 0.0;
  const bool bFamily = Payload->TryGetStringField(TEXT("fontFamily"), Family) && !Family.IsEmpty();
  const bool bTypeface = Payload->TryGetStringField(TEXT("typeface"), Typeface) && !Typeface.IsEmpty();
  const bool bSpacing = Payload->TryGetNumberField(TEXT("letterSpacing"), Spacing);
  const bool bCopy = Payload->TryGetStringField(TEXT("copyStyleFrom"), CopyFrom) && !CopyFrom.IsEmpty();
  FSlateFontInfo Font = McpTextBlockFont(Text);
  if (bCopy) {
    const UWidgetTree *Tree = Text->GetTypedOuter<UWidgetTree>();
    const UTextBlock *Source = Tree ? Cast<UTextBlock>(Tree->FindWidget(FName(*CopyFrom))) : nullptr;
    if (!Source) {
      OutUnsupported = FString::Printf(TEXT("copyStyleFrom '%s' names no text block in this Widget Blueprint."), *CopyFrom);
      return false;
    }
    Font = McpTextBlockFont(Source);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Text->SetColorAndOpacity(Source->GetColorAndOpacity());
    Text->SetShadowOffset(Source->GetShadowOffset());
    Text->SetShadowColorAndOpacity(Source->GetShadowColorAndOpacity());
#else
    Text->SetColorAndOpacity(Source->ColorAndOpacity);
    Text->SetShadowOffset(Source->ShadowOffset);
    Text->SetShadowColorAndOpacity(Source->ShadowColorAndOpacity);
#endif
    Applied.Add(MakeShared<FJsonValueString>(TEXT("copyStyleFrom")));
  }
  if (bFamily) {
    UFont *Asset = LoadObject<UFont>(nullptr, *Family);
    if (!Asset) {
      OutUnsupported = FString::Printf(TEXT("fontFamily '%s' is not a Font asset (e.g. /Engine/EngineFonts/Roboto)."), *Family);
      return false;
    }
    Font.FontObject = Asset;
    Applied.Add(MakeShared<FJsonValueString>(TEXT("fontFamily")));
  }
  if (bTypeface) {
    const UFont *Asset = Cast<UFont>(Font.FontObject);
    const FCompositeFont *Composite = Asset ? Asset->GetCompositeFont() : nullptr;
    TArray<FString> Faces;
    if (Composite) {
      for (const FTypefaceEntry &Entry : Composite->DefaultTypeface.Fonts) {
        Faces.Add(Entry.Name.ToString());
      }
    }
    if (Faces.Num() > 0 && !Faces.Contains(Typeface)) {
      OutUnsupported = FString::Printf(TEXT("typeface '%s' is not a face of this font; it has %s."),
                                       *Typeface, *FString::Join(Faces, TEXT(", ")));
      return false;
    }
    Font.TypefaceFontName = FName(*Typeface);
    Applied.Add(MakeShared<FJsonValueString>(TEXT("typeface")));
  }
  if (bSpacing) {
    Font.LetterSpacing = static_cast<int32>(Spacing);
    Applied.Add(MakeShared<FJsonValueString>(TEXT("letterSpacing")));
  }
  if (bCopy || bFamily || bTypeface || bSpacing) {
    Text->SetFont(Font);
  }
  return true;
}

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
  const bool bFont = Payload->HasField(TEXT("fontFamily")) || Payload->HasField(TEXT("typeface")) ||
                     Payload->HasField(TEXT("letterSpacing")) || Payload->HasField(TEXT("copyStyleFrom"));
  if (!bText && !bSize && !bFont) {
    return true;
  }
  UWidget *Target = Widget;
  if (UContentWidget *Content = Cast<UContentWidget>(Widget)) {
    Target = Content->GetContent() ? Content->GetContent() : Widget;
  }
  if (UTextBlock *Text = Cast<UTextBlock>(Target)) {
    // Face, family, spacing and copied style first: fontSize then sizes whichever font they left.
    if (bFont && !McpApplyTextFont(Text, Payload, Applied, OutUnsupported)) {
      return false;
    }
    if (bSize) {
      FSlateFontInfo Font = McpTextBlockFont(Text);
      Font.Size = static_cast<int32>(FontSize);
      Text->SetFont(Font);
    }
    if (bText) {
      Text->SetText(FText::FromString(NewText));
    }
  } else if (URichTextBlock *Rich = Cast<URichTextBlock>(Target)) {
    if (bFont) {
      OutUnsupported = TEXT("A RichTextBlock takes its faces from its text style set; fontFamily, typeface, ")
                       TEXT("letterSpacing and copyStyleFrom apply to a TextBlock.");
      return false;
    }
    if (bSize) {
      Rich->SetDefaultFont(FCoreStyle::GetDefaultFontStyle("Regular", static_cast<int32>(FontSize)));
    }
    if (bText) {
      Rich->SetText(FText::FromString(NewText));
    }
  } else {
    OutUnsupported = FString::Printf(
        TEXT("%s shows no text that `text`, `fontSize` or the font fields can change; target the text ")
        TEXT("block itself (a button's label is its child)."), *Widget->GetClass()->GetName());
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
