#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/DateTime.h"
#include <functional>
#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "EditorAssetLibrary.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"
#include "WidgetBlueprint.h"
#if __has_include("Factories/WidgetBlueprintFactory.h")
#include "Factories/WidgetBlueprintFactory.h"
#define MCP_HAS_WIDGET_FACTORY 1
#else
#define MCP_HAS_WIDGET_FACTORY 0
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleUiAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  bool bIsSystemControl =
      LowerAction.Equals(TEXT("system_control"), ESearchCase::IgnoreCase);
  bool bIsManageUi =
      LowerAction.Equals(TEXT("manage_ui"), ESearchCase::IgnoreCase);

  if (!bIsSystemControl && !bIsManageUi) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (Payload->HasField(TEXT("subAction"))) {
    SubAction = Payload->GetStringField(TEXT("subAction"));
  } else {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }
  const FString LowerSub = SubAction.ToLower();

  TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
  Resp->SetStringField(TEXT("action"), LowerSub);

  bool bSuccess = false;
  FString Message;
  FString ErrorCode;

#if WITH_EDITOR
  if (LowerSub == TEXT("create_widget")) {
    FString WidgetPath;
    if (!Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath) || WidgetPath.IsEmpty()) {
      Message = TEXT("widgetPath required for create_widget");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UClass* WidgetClass = nullptr;
      
      // First, try to load as a Widget Blueprint (asset paths like /Game/...)
      if (WidgetPath.StartsWith(TEXT("/Game/")) || WidgetPath.Contains(TEXT("."))) {
        UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
        if (WidgetBP && WidgetBP->GeneratedClass) {
          WidgetClass = WidgetBP->GeneratedClass;
        }
      }
      
      // Fallback: try ResolveClassByName for native classes (/Script/UMG.MyWidget)
      if (!WidgetClass) {
        WidgetClass = ResolveClassByName(WidgetPath);
      }
      
      if (!WidgetClass || !WidgetClass->IsChildOf(UUserWidget::StaticClass())) {
        Message = FString::Printf(TEXT("Could not resolve valid UUserWidget class from '%s'. For Widget Blueprints, use the full asset path (e.g., /Game/UI/WBP_MyWidget). For native classes, use /Script/UMG.MyClass."), *WidgetPath);
        ErrorCode = TEXT("CLASS_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        UWorld* World = GetActiveWorld();
        if (World) {
          UUserWidget* Widget = CreateWidget<UUserWidget>(World, WidgetClass);
          if (Widget) {
            bool bAddToViewport = true;
            Payload->TryGetBoolField(TEXT("addToViewport"), bAddToViewport);
            if (bAddToViewport) {
              int32 ZOrder = 0;
              Payload->TryGetNumberField(TEXT("zOrder"), ZOrder);
              Widget->AddToViewport(ZOrder);
            }
            bSuccess = true;
            Message = FString::Printf(TEXT("Widget created: %s"), *Widget->GetName());
            Resp->SetStringField(TEXT("widgetName"), Widget->GetName());
            Resp->SetStringField(TEXT("widgetPath"), Widget->GetPathName());
          } else {
            Message = TEXT("Failed to create widget instance");
            ErrorCode = TEXT("CREATE_FAILED");
            Resp->SetStringField(TEXT("error"), Message);
          }
        } else {
          Message = TEXT("No active world context found");
          ErrorCode = TEXT("NO_WORLD");
          Resp->SetStringField(TEXT("error"), Message);
        }
      }
    }
  } else if (LowerSub == TEXT("add_widget_child")) {
#if WITH_EDITOR && MCP_HAS_WIDGET_FACTORY
    FString WidgetPath;
    if (!Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath) ||
        WidgetPath.IsEmpty()) {
      Message = TEXT("widgetPath required for add_widget_child");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UWidgetBlueprint *WidgetBP =
          LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
      if (!WidgetBP) {
        Message = FString::Printf(TEXT("Could not find Widget Blueprint at %s"),
                                  *WidgetPath);
        ErrorCode = TEXT("ASSET_NOT_FOUND");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        FString ChildClassPath;
        if (!Payload->TryGetStringField(TEXT("childClass"), ChildClassPath) ||
            ChildClassPath.IsEmpty()) {
          // Fallback to commonly used types if only short name provided?
          // For now require full path or class name if it can be found.
          Message = TEXT("childClass required (e.g. /Script/UMG.Button)");
          ErrorCode = TEXT("INVALID_ARGUMENT");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          UClass* WidgetClass = ResolveClassByName(ChildClassPath);

          if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass())) {
            Message = FString::Printf(
                TEXT("Could not resolve valid UWidget class from '%s'"),
                *ChildClassPath);
            ErrorCode = TEXT("CLASS_NOT_FOUND");
            Resp->SetStringField(TEXT("error"), Message);
          } else {
            FString ParentName;
            Payload->TryGetStringField(TEXT("parentName"), ParentName);

            WidgetBP->Modify();

            UWidget *NewWidget =
                WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass);

            bool bAdded = false;
            bool bIsRoot = false;

            if (ParentName.IsEmpty()) {
              // Try to set as RootWidget if empty
              if (WidgetBP->WidgetTree->RootWidget == nullptr) {
                WidgetBP->WidgetTree->RootWidget = NewWidget;
                bAdded = true;
                bIsRoot = true;
              } else {
                // Try to add to existing root if it's a panel
                UPanelWidget *RootPanel =
                    Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
                if (RootPanel) {
                  RootPanel->AddChild(NewWidget);
                  bAdded = true;
                } else {
                  Message = TEXT("Root widget is not a panel and already "
                                 "exists. Specify parentName.");
                  ErrorCode = TEXT("ROOT_Full");
                }
              }
            } else {
              // Find parent
              UWidget *ParentWidget =
                  WidgetBP->WidgetTree->FindWidget(FName(*ParentName));
              UPanelWidget *ParentPanel = Cast<UPanelWidget>(ParentWidget);
              if (ParentPanel) {
                ParentPanel->AddChild(NewWidget);
                bAdded = true;
              } else {
                Message = FString::Printf(
                    TEXT("Parent '%s' not found or is not a PanelWidget"),
                    *ParentName);
                ErrorCode = TEXT("PARENT_NOT_FOUND");
              }
            }

            if (bAdded) {
              bSuccess = true;
              Message = FString::Printf(TEXT("Added %s to %s"),
                                        *WidgetClass->GetName(),
                                        *WidgetBP->GetName());
              Resp->SetStringField(TEXT("widgetName"), NewWidget->GetName());
              Resp->SetStringField(TEXT("childClass"), WidgetClass->GetName());
            } else {
              if (Message.IsEmpty())
                Message = TEXT("Failed to add widget child.");
              Resp->SetStringField(TEXT("error"), Message);
            }
          }
        }
      }
    }
#else
    Message = TEXT("add_widget_child requires editor build");
    ErrorCode = TEXT("NOT_AVAILABLE");
    Resp->SetStringField(TEXT("error"), Message);
#endif
  } else if (LowerSub == TEXT("screenshot")) {
    // Take a screenshot of the viewport and return as base64
    FString ScreenshotPath;
    Payload->TryGetStringField(TEXT("path"), ScreenshotPath);
    if (ScreenshotPath.IsEmpty()) {
      ScreenshotPath =
          FPaths::ProjectSavedDir() / TEXT("Screenshots/WindowsEditor");
    }

    FString Filename;
    Payload->TryGetStringField(TEXT("filename"), Filename);
    if (Filename.IsEmpty()) {
      Filename = FString::Printf(TEXT("Screenshot_%lld"),
                                 FDateTime::Now().ToUnixTimestamp());
    }

    bool bReturnBase64 = true;
    Payload->TryGetBoolField(TEXT("returnBase64"), bReturnBase64);

    // Get viewport
    if (!GEngine || !GEngine->GameViewport) {
      Message = TEXT("No game viewport available");
      ErrorCode = TEXT("NO_VIEWPORT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UGameViewportClient *ViewportClient = GEngine->GameViewport;
      FViewport *Viewport = ViewportClient->Viewport;

      if (!Viewport) {
        Message = TEXT("No viewport available");
        ErrorCode = TEXT("NO_VIEWPORT");
        Resp->SetStringField(TEXT("error"), Message);
      } else {
        // Capture viewport pixels
        TArray<FColor> Bitmap;
        FIntVector Size(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0);

        bool bReadSuccess = Viewport->ReadPixels(Bitmap);

        if (!bReadSuccess || Bitmap.Num() == 0) {
          Message = TEXT("Failed to read viewport pixels");
          ErrorCode = TEXT("CAPTURE_FAILED");
          Resp->SetStringField(TEXT("error"), Message);
        } else {
          // Ensure we have the right size
          const int32 Width = Size.X;
          const int32 Height = Size.Y;

          // Compress to PNG
          TArray<uint8> PngData;
          FImageUtils::ThumbnailCompressImageArray(Width, Height, Bitmap,
                                                   PngData);

          if (PngData.Num() == 0) {
            // Alternative: compress as PNG using IImageWrapper
            IImageWrapperModule &ImageWrapperModule =
                FModuleManager::LoadModuleChecked<IImageWrapperModule>(
                    FName("ImageWrapper"));
            TSharedPtr<IImageWrapper> ImageWrapper =
                ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

            if (ImageWrapper.IsValid()) {
              TArray<uint8> RawData;
              RawData.SetNum(Width * Height * 4);
              for (int32 i = 0; i < Bitmap.Num(); ++i) {
                RawData[i * 4 + 0] = Bitmap[i].R;
                RawData[i * 4 + 1] = Bitmap[i].G;
                RawData[i * 4 + 2] = Bitmap[i].B;
                RawData[i * 4 + 3] = Bitmap[i].A;
              }

              if (ImageWrapper->SetRaw(RawData.GetData(), RawData.Num(), Width,
                                       Height, ERGBFormat::RGBA, 8)) {
                PngData = ImageWrapper->GetCompressed(100);
              }
            }
          }

          FString FullPath =
              FPaths::Combine(ScreenshotPath, Filename + TEXT(".png"));
          FPaths::MakeStandardFilename(FullPath);

          // Always save to disk
          IFileManager::Get().MakeDirectory(*ScreenshotPath, true);
          bool bSaved = FFileHelper::SaveArrayToFile(PngData, *FullPath);

          bSuccess = true;
          Message = FString::Printf(TEXT("Screenshot captured (%dx%d)"), Width,
                                    Height);
          Resp->SetStringField(TEXT("screenshotPath"), FullPath);
          Resp->SetStringField(TEXT("filename"), Filename);
          Resp->SetNumberField(TEXT("width"), Width);
          Resp->SetNumberField(TEXT("height"), Height);
          Resp->SetNumberField(TEXT("sizeBytes"), PngData.Num());

          // Return base64 encoded image if requested
          if (bReturnBase64 && PngData.Num() > 0) {
            FString Base64Data = FBase64::Encode(PngData);
            Resp->SetStringField(TEXT("imageBase64"), Base64Data);
            Resp->SetStringField(TEXT("mimeType"), TEXT("image/png"));
          }
        }
      }
    }
  } else if (LowerSub == TEXT("play_in_editor")) {
    // Start play in editor
    if (GEditor && GEditor->PlayWorld) {
      Message = TEXT("Already playing in editor");
      ErrorCode = TEXT("ALREADY_PLAYING");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      // Execute play command
      bool bCommandSuccess = GEditor->Exec(nullptr, TEXT("Play In Editor"));
      if (bCommandSuccess) {
        bSuccess = true;
        Message = TEXT("Started play in editor");
        Resp->SetStringField(TEXT("status"), TEXT("playing"));
      } else {
        Message = TEXT("Failed to start play in editor");
        ErrorCode = TEXT("PLAY_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  } else if (LowerSub == TEXT("stop_play")) {
    // Stop play in editor
    if (GEditor && GEditor->PlayWorld) {
      // Execute stop command
      bool bCommandSuccess =
          GEditor->Exec(nullptr, TEXT("Stop Play In Editor"));
      if (bCommandSuccess) {
        bSuccess = true;
        Message = TEXT("Stopped play in editor");
        Resp->SetStringField(TEXT("status"), TEXT("stopped"));
      } else {
        Message = TEXT("Failed to stop play in editor");
        ErrorCode = TEXT("STOP_FAILED");
        Resp->SetStringField(TEXT("error"), Message);
      }
    } else {
      Message = TEXT("Not currently playing in editor");
      ErrorCode = TEXT("NOT_PLAYING");
      Resp->SetStringField(TEXT("error"), Message);
    }
  } else if (LowerSub == TEXT("save_all")) {
    // Save all assets and levels
    bool bCommandSuccess = GEditor->Exec(nullptr, TEXT("Asset Save All"));
    if (bCommandSuccess) {
      bSuccess = true;
      Message = TEXT("Saved all assets");
      Resp->SetStringField(TEXT("status"), TEXT("saved"));
    } else {
      Message = TEXT("Failed to save all assets");
      ErrorCode = TEXT("SAVE_FAILED");
      Resp->SetStringField(TEXT("error"), Message);
    }
  } else if (LowerSub == TEXT("simulate_input")) {
    FString KeyName;
    Payload->TryGetStringField(TEXT("keyName"),
                               KeyName); // Changed to keyName to match schema
    if (KeyName.IsEmpty())
      Payload->TryGetStringField(TEXT("key"), KeyName); // Fallback

    FString EventType;
    Payload->TryGetStringField(TEXT("eventType"), EventType);

    FKey Key = FKey(FName(*KeyName));
    if (Key.IsValid()) {
      const uint32 CharacterCode = 0;
      const uint32 KeyCode = 0;
      const bool bIsRepeat = false;
      FModifierKeysState ModifierState;

      if (EventType == TEXT("KeyDown")) {
        FKeyEvent KeyEvent(Key, ModifierState,
                           FSlateApplication::Get().GetUserIndexForKeyboard(),
                           bIsRepeat, CharacterCode, KeyCode);
        FSlateApplication::Get().ProcessKeyDownEvent(KeyEvent);
      } else if (EventType == TEXT("KeyUp")) {
        FKeyEvent KeyEvent(Key, ModifierState,
                           FSlateApplication::Get().GetUserIndexForKeyboard(),
                           bIsRepeat, CharacterCode, KeyCode);
        FSlateApplication::Get().ProcessKeyUpEvent(KeyEvent);
      } else {
        // Press and Release
        FKeyEvent KeyDownEvent(
            Key, ModifierState,
            FSlateApplication::Get().GetUserIndexForKeyboard(), bIsRepeat,
            CharacterCode, KeyCode);
        FSlateApplication::Get().ProcessKeyDownEvent(KeyDownEvent);

        FKeyEvent KeyUpEvent(Key, ModifierState,
                             FSlateApplication::Get().GetUserIndexForKeyboard(),
                             bIsRepeat, CharacterCode, KeyCode);
        FSlateApplication::Get().ProcessKeyUpEvent(KeyUpEvent);
      }

      bSuccess = true;
      Message = FString::Printf(TEXT("Simulated input for key: %s"), *KeyName);
    } else {
      Message = FString::Printf(TEXT("Invalid key name: %s"), *KeyName);
      ErrorCode = TEXT("INVALID_KEY");
      Resp->SetStringField(TEXT("error"), Message);
    }
  } else if (LowerSub == TEXT("create_hud")) {
    FString WidgetPath;
    Payload->TryGetStringField(TEXT("widgetPath"), WidgetPath);
    
    UClass* WidgetClass = nullptr;
    
    // First, try to load as a Widget Blueprint (asset paths like /Game/...)
    if (WidgetPath.StartsWith(TEXT("/Game/")) || WidgetPath.Contains(TEXT("."))) {
      UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
      if (WidgetBP && WidgetBP->GeneratedClass) {
        WidgetClass = WidgetBP->GeneratedClass;
      }
    }
    
    // Fallback: try ResolveClassByName for native classes (/Script/UMG.MyWidget)
    if (!WidgetClass) {
      WidgetClass = ResolveClassByName(WidgetPath);
    }
    
    if (WidgetClass && WidgetClass->IsChildOf(UUserWidget::StaticClass()) &&
        GEngine && GEngine->GameViewport) {
      UWorld *World = GEngine->GameViewport->GetWorld();
      if (World) {
        UUserWidget *Widget = CreateWidget<UUserWidget>(World, WidgetClass);
        if (Widget) {
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
      Message = FString::Printf(TEXT("Failed to load widget class: %s. For Widget Blueprints, use the full asset path (e.g., /Game/UI/WBP_MyWidget)."), *WidgetPath);
      ErrorCode = TEXT("CLASS_NOT_FOUND");
    }
  } else if (LowerSub == TEXT("set_widget_text")) {
    FString Key, Value;
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetStringField(TEXT("value"), Value);

    bool bFound = false;
    // Iterate all widgets to find one matching Key (Name)
    TArray<UUserWidget *> Widgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        GetActiveWorld(), Widgets,
        UUserWidget::StaticClass(), false);
    // Also try Game Viewport world if Editor World is not right context (PIE)
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld()) {
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
          GEngine->GameViewport->GetWorld(), Widgets,
          UUserWidget::StaticClass(), false);
    }

    for (UUserWidget *Widget : Widgets) {
      // Search inside this widget for a TextBlock named Key
      UWidget *Child = Widget->GetWidgetFromName(FName(*Key));
      if (UTextBlock *TextBlock = Cast<UTextBlock>(Child)) {
        TextBlock->SetText(FText::FromString(Value));
        bFound = true;
        bSuccess = true;
        Message =
            FString::Printf(TEXT("Set text on '%s' to '%s'"), *Key, *Value);
        break;
      }
      // Also check if the widget ITSELF is the one (though UserWidget !=
      // TextBlock usually)
      if (Widget->GetName() == Key) {
        // Can't set text on UserWidget directly unless it implements interface?
        // Assuming Key refers to child widget name usually
      }
    }

    if (!bFound) {
      // Replaced iterator with world-scoped lookup using GetActiveWorld()
      UWorld* ActiveWorld = GetActiveWorld();
      if (ActiveWorld) {
        TArray<UUserWidget*> FoundWidgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(ActiveWorld, FoundWidgets, UUserWidget::StaticClass(), false);
        for (UUserWidget* Widget : FoundWidgets) {
          if (UWidget* Child = Widget->GetWidgetFromName(FName(*Key))) {
            if (UTextBlock* TB = Cast<UTextBlock>(Child)) {
              TB->SetText(FText::FromString(Value));
              bFound = true;
              bSuccess = true;
              Message = FString::Printf(TEXT("Set text on '%s' (world-scoped lookup)"), *Key);
              UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("World-scoped lookup used for widget '%s'"), *Key);
              break;
            }
          }
        }
      }
    }

    if (!bFound) {
      Message = FString::Printf(TEXT("Widget/TextBlock '%s' not found"), *Key);
      ErrorCode = TEXT("WIDGET_NOT_FOUND");
    }
  } else if (LowerSub == TEXT("set_widget_image")) {
    FString Key, TexturePath;
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
    UTexture2D *Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
    if (Texture) {
      bool bFound = false;
      // Replaced iterator with world-scoped lookup using GetActiveWorld()
      UWorld* ActiveWorld = GetActiveWorld();
      if (ActiveWorld) {
        TArray<UUserWidget*> FoundWidgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(ActiveWorld, FoundWidgets, UUserWidget::StaticClass(), false);
        for (UUserWidget* Widget : FoundWidgets) {
          if (UWidget* Child = Widget->GetWidgetFromName(FName(*Key))) {
            if (UImage* Image = Cast<UImage>(Child)) {
              Image->SetBrushFromTexture(Texture);
              bFound = true;
              bSuccess = true;
              Message = FString::Printf(TEXT("Set image on '%s' (world-scoped lookup)"), *Key);
              UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("World-scoped lookup used for widget '%s'"), *Key);
              break;
            }
          }
        }
      }
      if (!bFound) {
        Message = FString::Printf(TEXT("Image widget '%s' not found"), *Key);
        ErrorCode = TEXT("WIDGET_NOT_FOUND");
      }
    } else {
      Message = TEXT("Failed to load texture");
      ErrorCode = TEXT("ASSET_NOT_FOUND");
    }
  } else if (LowerSub == TEXT("set_widget_visibility")) {
    FString Key;
    bool bVisible = true;
    Payload->TryGetStringField(TEXT("key"), Key);
    Payload->TryGetBoolField(TEXT("visible"), bVisible);

    bool bFound = false;
    // Replaced iteration with world-scoped lookup using GetActiveWorld()
    UWorld* ActiveWorld = GetActiveWorld();
    if (ActiveWorld) {
      TArray<UUserWidget*> FoundWidgets;
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(ActiveWorld, FoundWidgets, UUserWidget::StaticClass(), false);
      for (UUserWidget* Widget : FoundWidgets) {
        if (Widget->GetName() == Key) {
          Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
          bFound = true;
          bSuccess = true;
          UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("World-scoped lookup used for UserWidget '%s'"), *Key);
          break;
        }
      }
    }
    // If not found, try generic UWidget (child widgets)
    if (!bFound && ActiveWorld) {
      TArray<UUserWidget*> FoundWidgets;
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(ActiveWorld, FoundWidgets, UUserWidget::StaticClass(), false);
      for (UUserWidget* Widget : FoundWidgets) {
        if (UWidget* Child = Widget->GetWidgetFromName(FName(*Key))) {
          Child->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
          bFound = true;
          bSuccess = true;
          UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("World-scoped lookup used for UWidget '%s'"), *Key);
          break;
        }
      }
    }

    if (bFound) {
      Message = FString::Printf(TEXT("Set visibility on '%s' to %s"), *Key,
                                bVisible ? TEXT("Visible") : TEXT("Collapsed"));
    } else {
      Message = FString::Printf(TEXT("Widget '%s' not found"), *Key);
      ErrorCode = TEXT("WIDGET_NOT_FOUND");
    }
  } else if (LowerSub == TEXT("remove_widget_from_viewport")) {
    FString Key;
    Payload->TryGetStringField(TEXT("key"),
                               Key); // If empty, remove all? OR specific

    if (Key.IsEmpty()) {
      // Remove all user widgets?
      TArray<UUserWidget *> TempWidgets;
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
          GetActiveWorld(), TempWidgets,
          UUserWidget::StaticClass(), true);
      // Implementation:
      if (GEngine && GEngine->GameViewport &&
          GEngine->GameViewport->GetWorld()) {
        TArray<UUserWidget *> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
            GEngine->GameViewport->GetWorld(), Widgets,
            UUserWidget::StaticClass(), true);
        for (UUserWidget *W : Widgets) {
          W->RemoveFromParent();
        }
        bSuccess = true;
        Message = TEXT("Removed all widgets");
      }
    } else {
      bool bFound = false;
      UWorld* ActiveWorld = GetActiveWorld();
      if (ActiveWorld) {
        TArray<UUserWidget*> FoundWidgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(ActiveWorld, FoundWidgets, UUserWidget::StaticClass(), false);
        for (UUserWidget* Widget : FoundWidgets) {
          if (Widget->GetName() == Key) {
            Widget->RemoveFromParent();
            bFound = true;
            bSuccess = true;
            break;
          }
        }
      }
      
      if (bFound) {
        Message = FString::Printf(TEXT("Removed widget '%s'"), *Key);
      } else {
        Message = FString::Printf(TEXT("Widget '%s' not found"), *Key);
        ErrorCode = TEXT("WIDGET_NOT_FOUND");
      }
    }
  } else if (LowerSub == TEXT("get_all_widgets")) {
    // Get all active user widgets in the viewport
    TArray<TSharedPtr<FJsonValue>> WidgetArray;
    UWorld* ActiveWorld = GetActiveWorld();
    
    if (ActiveWorld) {
      TArray<UUserWidget*> FoundWidgets;
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(ActiveWorld, FoundWidgets, UUserWidget::StaticClass(), false);
      
      for (UUserWidget* Widget : FoundWidgets) {
        if (Widget) {
          TSharedPtr<FJsonObject> WidgetInfo = MakeShared<FJsonObject>();
          WidgetInfo->SetStringField(TEXT("name"), Widget->GetName());
          WidgetInfo->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
          WidgetInfo->SetBoolField(TEXT("isInViewport"), Widget->IsInViewport());
          WidgetInfo->SetBoolField(TEXT("isVisible"), Widget->IsVisible());
          WidgetArray.Add(MakeShared<FJsonValueObject>(WidgetInfo));
        }
      }
    }
    
    // Also check GameViewport world (for PIE)
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->GetWorld()) {
      TArray<UUserWidget*> PIEWidgets;
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
          GEngine->GameViewport->GetWorld(), PIEWidgets,
          UUserWidget::StaticClass(), false);
      
      for (UUserWidget* Widget : PIEWidgets) {
        if (Widget) {
          TSharedPtr<FJsonObject> WidgetInfo = MakeShared<FJsonObject>();
          WidgetInfo->SetStringField(TEXT("name"), Widget->GetName());
          WidgetInfo->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
          WidgetInfo->SetBoolField(TEXT("isInViewport"), Widget->IsInViewport());
          WidgetInfo->SetBoolField(TEXT("isVisible"), Widget->IsVisible());
          WidgetInfo->SetStringField(TEXT("context"), TEXT("PIE"));
          WidgetArray.Add(MakeShared<FJsonValueObject>(WidgetInfo));
        }
      }
    }
    
    Resp->SetArrayField(TEXT("widgets"), WidgetArray);
    Resp->SetNumberField(TEXT("count"), WidgetArray.Num());
    bSuccess = true;
    Message = FString::Printf(TEXT("Found %d widgets"), WidgetArray.Num());
  } else if (LowerSub == TEXT("get_widget_hierarchy")) {
    // Get widget hierarchy for a specific widget or all widgets
    FString Key;
    Payload->TryGetStringField(TEXT("key"), Key);
    
    TArray<TSharedPtr<FJsonValue>> HierarchyArray;
    UWorld* ActiveWorld = GetActiveWorld();
    
    auto BuildWidgetHierarchy = [](UWidget* Widget, int32 Depth) -> TSharedPtr<FJsonObject> {
      TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
      Info->SetStringField(TEXT("name"), Widget->GetName());
      Info->SetStringField(TEXT("class"), Widget->GetClass()->GetName());
      Info->SetNumberField(TEXT("depth"), Depth);
      Info->SetBoolField(TEXT("isVisible"), Widget->IsVisible());
      
      // Get slot info if available
      if (UPanelSlot* Slot = Widget->Slot) {
        Info->SetStringField(TEXT("slotType"), Slot->GetClass()->GetName());
      }
      
      return Info;
    };
    
    std::function<void(UWidget*, int32, TArray<TSharedPtr<FJsonValue>>&)> TraverseChildren;
    TraverseChildren = [&](UWidget* Parent, int32 Depth, TArray<TSharedPtr<FJsonValue>>& OutArray) {
      if (!Parent) return;
      
      OutArray.Add(MakeShared<FJsonValueObject>(BuildWidgetHierarchy(Parent, Depth)));
      
      if (UPanelWidget* Panel = Cast<UPanelWidget>(Parent)) {
        for (int32 i = 0; i < Panel->GetChildrenCount(); ++i) {
          if (UWidget* Child = Panel->GetChildAt(i)) {
            TraverseChildren(Child, Depth + 1, OutArray);
          }
        }
      }
    };
    
    if (ActiveWorld) {
      TArray<UUserWidget*> FoundWidgets;
      UWidgetBlueprintLibrary::GetAllWidgetsOfClass(ActiveWorld, FoundWidgets, UUserWidget::StaticClass(), false);
      
      for (UUserWidget* Widget : FoundWidgets) {
        if (Widget && (Key.IsEmpty() || Widget->GetName() == Key)) {
          TSharedPtr<FJsonObject> WidgetHierarchy = MakeShared<FJsonObject>();
          WidgetHierarchy->SetStringField(TEXT("rootWidget"), Widget->GetName());
          
          TArray<TSharedPtr<FJsonValue>> Children;
          if (Widget->WidgetTree && Widget->WidgetTree->RootWidget) {
            TraverseChildren(Widget->WidgetTree->RootWidget, 0, Children);
          }
          WidgetHierarchy->SetArrayField(TEXT("children"), Children);
          HierarchyArray.Add(MakeShared<FJsonValueObject>(WidgetHierarchy));
          
          if (!Key.IsEmpty()) break; // Found specific widget
        }
      }
    }
    
    Resp->SetArrayField(TEXT("hierarchy"), HierarchyArray);
    bSuccess = true;
    Message = FString::Printf(TEXT("Retrieved hierarchy for %d widget(s)"), HierarchyArray.Num());
  } else if (LowerSub == TEXT("set_input_mode")) {
    // Set input mode for the player controller
    FString InputMode;
    Payload->TryGetStringField(TEXT("inputMode"), InputMode);
    
    if (InputMode.IsEmpty()) {
      Message = TEXT("inputMode required (GameOnly, UIOnly, GameAndUI)");
      ErrorCode = TEXT("INVALID_ARGUMENT");
      Resp->SetStringField(TEXT("error"), Message);
    } else {
      UWorld* World = nullptr;
      if (GEngine && GEngine->GameViewport) {
        World = GEngine->GameViewport->GetWorld();
      }
      if (!World) {
        World = GetActiveWorld();
      }
      
      if (World) {
        APlayerController* PC = World->GetFirstPlayerController();
        if (PC) {
          if (InputMode.Equals(TEXT("GameOnly"), ESearchCase::IgnoreCase)) {
            PC->SetInputMode(FInputModeGameOnly());
            bSuccess = true;
            Message = TEXT("Set input mode to GameOnly");
          } else if (InputMode.Equals(TEXT("UIOnly"), ESearchCase::IgnoreCase)) {
            PC->SetInputMode(FInputModeUIOnly());
            bSuccess = true;
            Message = TEXT("Set input mode to UIOnly");
          } else if (InputMode.Equals(TEXT("GameAndUI"), ESearchCase::IgnoreCase)) {
            PC->SetInputMode(FInputModeGameAndUI());
            bSuccess = true;
            Message = TEXT("Set input mode to GameAndUI");
          } else {
            Message = FString::Printf(TEXT("Invalid input mode: %s (use GameOnly, UIOnly, or GameAndUI)"), *InputMode);
            ErrorCode = TEXT("INVALID_ARGUMENT");
            Resp->SetStringField(TEXT("error"), Message);
          }
          Resp->SetStringField(TEXT("inputMode"), InputMode);
        } else {
          Message = TEXT("No player controller found (is PIE running?)");
          ErrorCode = TEXT("NO_PLAYER_CONTROLLER");
          Resp->SetStringField(TEXT("error"), Message);
        }
      } else {
        Message = TEXT("No world context found");
        ErrorCode = TEXT("NO_WORLD");
        Resp->SetStringField(TEXT("error"), Message);
      }
    }
  } else if (LowerSub == TEXT("show_mouse_cursor")) {
    // Show or hide the mouse cursor
    bool bShowCursor = true;
    Payload->TryGetBoolField(TEXT("showCursor"), bShowCursor);
    
    UWorld* World = nullptr;
    if (GEngine && GEngine->GameViewport) {
      World = GEngine->GameViewport->GetWorld();
    }
    if (!World) {
      World = GetActiveWorld();
    }
    
    if (World) {
      APlayerController* PC = World->GetFirstPlayerController();
      if (PC) {
        PC->bShowMouseCursor = bShowCursor;
        bSuccess = true;
        Message = FString::Printf(TEXT("Mouse cursor %s"), bShowCursor ? TEXT("shown") : TEXT("hidden"));
        Resp->SetBoolField(TEXT("showCursor"), bShowCursor);
      } else {
        Message = TEXT("No player controller found (is PIE running?)");
        ErrorCode = TEXT("NO_PLAYER_CONTROLLER");
        Resp->SetStringField(TEXT("error"), Message);
      }
    } else {
      Message = TEXT("No world context found");
      ErrorCode = TEXT("NO_WORLD");
      Resp->SetStringField(TEXT("error"), Message);
    }
  } else {
    Message = FString::Printf(
        TEXT("System control action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
  }
#else
  Message = TEXT("System control actions require editor build.");
  ErrorCode = TEXT("NOT_IMPLEMENTED");
  Resp->SetStringField(TEXT("error"), Message);
#endif

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("System control action completed")
                       : TEXT("System control action failed");
  }

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}
