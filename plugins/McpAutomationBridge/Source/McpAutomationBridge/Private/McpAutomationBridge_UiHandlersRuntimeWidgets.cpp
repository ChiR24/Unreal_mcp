#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_UiHandlersPrivate.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
namespace McpUiHandlers {

bool HandleRuntimeWidgetAction(const FString &LowerSub,
                               const TSharedPtr<FJsonObject> &Payload,
                               const TSharedPtr<FJsonObject> &Resp,
                               bool &bSuccess, FString &Message,
                               FString &ErrorCode) {
  if (LowerSub == TEXT("create_hud")) {
    FString WidgetPath;
    Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath);
    UClass *WidgetClass = LoadClass<UUserWidget>(nullptr, *WidgetPath);
    if (WidgetClass && GEngine && GEngine->GameViewport) {
      if (UWorld *World = GEngine->GameViewport->GetWorld()) {
        if (UUserWidget *Widget = CreateWidget<UUserWidget>(World, WidgetClass)) {
          Widget->AddToViewport();
          bSuccess = true;
          Message = TEXT("HUD created and added to viewport");
          Resp->SetStringField(TEXT("widgetName"), Widget->GetName());
        } else {
          Message = TEXT("Failed to create widget");
          ErrorCode = TEXT("CREATE_FAILED");
        }
      } else {
        Message = TEXT("No world context found (is PIE running?)");
        ErrorCode = TEXT("NO_WORLD");
      }
    } else {
      Message =
          FString::Printf(TEXT("Failed to load widget class: %s"), *WidgetPath);
      ErrorCode = TEXT("CLASS_NOT_FOUND");
    }
    return true;
  }

  if (LowerSub == TEXT("set_widget_text")) {
    FString Key;
    FString Value;
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetStringField(TEXT("value"), Value);

    bool bFound = false;
    TArray<UUserWidget *> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        GEditor->GetEditorWorldContext().World(), Widgets,
        UUserWidget::StaticClass(), false);
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld()) {
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
          GEngine->GameViewport->GetWorld(), Widgets,
          UUserWidget::StaticClass(), false);
    }

    for (UUserWidget *Widget : Widgets) {
      UWidget *Child = Widget->GetWidgetFromName(FName(*Key));
      if (UTextBlock *TextBlock = Cast<UTextBlock>(Child)) {
        TextBlock->SetText(FText::FromString(Value));
        bFound = true;
        bSuccess = true;
        Message =
            FString::Printf(TEXT("Set text on '%s' to '%s'"), *Key, *Value);
        break;
      }
    }

    if (!bFound) {
      for (TObjectIterator<UTextBlock> It; It; ++It) {
        if (It->GetName() == Key && It->GetWorld()) {
          It->SetText(FText::FromString(Value));
          bFound = true;
          bSuccess = true;
          Message = FString::Printf(TEXT("Set text on global '%s'"), *Key);
          break;
        }
      }
    }

    if (!bFound) {
      Message = FString::Printf(TEXT("Widget/TextBlock '%s' not found"), *Key);
      ErrorCode = TEXT("WIDGET_NOT_FOUND");
    }
    return true;
  }

  if (LowerSub == TEXT("set_widget_image")) {
    FString Key;
    FString TexturePath;
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetStringField(TEXT("texturePath"), TexturePath);

    UTexture2D *Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
    if (!Texture) {
      Message = TEXT("Failed to load texture");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
      return true;
    }

    bool bFound = false;
    for (TObjectIterator<UImage> It; It; ++It) {
      if (It->GetName() == Key && It->GetWorld()) {
        It->SetBrushFromTexture(Texture);
        bFound = true;
        bSuccess = true;
        Message = FString::Printf(TEXT("Set image on '%s'"), *Key);
        break;
      }
    }
    if (!bFound) {
      Message = FString::Printf(TEXT("Image widget '%s' not found"), *Key);
      ErrorCode = TEXT("WIDGET_NOT_FOUND");
    }
    return true;
  }

  if (LowerSub == TEXT("set_widget_visibility")) {
    FString Key;
    bool bVisible = true;
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetBoolField(TEXT("visible"), bVisible);

    bool bFound = false;
    for (TObjectIterator<UUserWidget> It; It; ++It) {
      if (It->GetName() == Key && It->GetWorld()) {
        It->SetVisibility(bVisible ? ESlateVisibility::Visible
                                   : ESlateVisibility::Collapsed);
        bFound = true;
        bSuccess = true;
        break;
      }
    }
    if (!bFound) {
      for (TObjectIterator<UWidget> It; It; ++It) {
        if (It->GetName() == Key && It->GetWorld()) {
          It->SetVisibility(bVisible ? ESlateVisibility::Visible
                                     : ESlateVisibility::Collapsed);
          bFound = true;
          bSuccess = true;
          break;
        }
      }
    }

    if (bFound) {
      Message = FString::Printf(TEXT("Set visibility on '%s' to %s"), *Key,
                                bVisible ? TEXT("Visible")
                                         : TEXT("Collapsed"));
    } else {
      Message = FString::Printf(TEXT("Widget '%s' not found"), *Key);
      ErrorCode = TEXT("WIDGET_NOT_FOUND");
    }
    return true;
  }

  if (LowerSub != TEXT("remove_widget_from_viewport")) {
    return false;
  }

  FString Key;
  Payload->TryGetStringField(TEXT("key"), Key);

  if (Key.IsEmpty()) {
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld()) {
      TArray<UUserWidget *> Widgets;
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
          GEngine->GameViewport->GetWorld(), Widgets,
          UUserWidget::StaticClass(), true);
      for (UUserWidget *Widget : Widgets) {
        Widget->RemoveFromParent();
      }
      bSuccess = true;
      Message = TEXT("Removed all widgets");
    }
    return true;
  }

  bool bFound = false;
  for (TObjectIterator<UUserWidget> It; It; ++It) {
    if (It->GetName() == Key && It->GetWorld()) {
      It->RemoveFromParent();
      bFound = true;
      bSuccess = true;
      break;
    }
  }

  if (bFound) {
    Message = FString::Printf(TEXT("Removed widget '%s'"), *Key);
  } else {
    Message = FString::Printf(TEXT("Widget '%s' not found"), *Key);
    ErrorCode = TEXT("WIDGET_NOT_FOUND");
  }
  return true;
}

}
#endif
