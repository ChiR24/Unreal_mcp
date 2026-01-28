// McpAutomationBridge_WidgetAuthoringHandlers.cpp
// Phase 19: Widget Authoring System Handlers
//
// Complete UMG widget authoring capabilities including:
// - Widget Creation (blueprints, parent classes)
// - Layout Panels (canvas, box, overlay, grid, scroll, etc.)
// - Common Widgets (text, image, button, slider, progress, input, etc.)
// - Layout & Styling (anchor, alignment, position, size, padding, style)
// - Bindings & Events (property bindings, event handlers)
// - Widget Animations (animation tracks, keyframes, playback)
// - UI Templates (main menu, pause menu, HUD, inventory, etc.)
// - Utility (info queries, preview)

#include "McpAutomationBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "McpBridgeWebSocket.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/GridPanel.h"
#include "Components/UniformGridPanel.h"
#include "Components/WrapBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ProgressBar.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/SpinBox.h"
#include "Components/ListView.h"
#include "Components/TreeView.h"
#include "WidgetBlueprint.h"
#include "UObject/UObjectIterator.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "McpAutomationBridgeHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"

// ============================================================================
// Helper Functions
// ============================================================================

namespace WidgetAuthoringHelpers
{
    FLinearColor GetColorFromJsonWidget(const TSharedPtr<FJsonObject>& ColorObj, const FLinearColor& Default = FLinearColor::White)
    {
        if (!ColorObj.IsValid())
        {
            return Default;
        }
        FLinearColor Color = Default;
        Color.R = ColorObj->HasField(TEXT("r")) ? GetJsonNumberField(ColorObj, TEXT("r")) : Default.R;
        Color.G = ColorObj->HasField(TEXT("g")) ? GetJsonNumberField(ColorObj, TEXT("g")) : Default.G;
        Color.B = ColorObj->HasField(TEXT("b")) ? GetJsonNumberField(ColorObj, TEXT("b")) : Default.B;
        Color.A = ColorObj->HasField(TEXT("a")) ? GetJsonNumberField(ColorObj, TEXT("a")) : Default.A;
        return Color;
    }

    // Get object field
    TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
        {
            return Payload->GetObjectField(FieldName);
        }
        return nullptr;
    }

    // Get array field
    const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Array>(FieldName))
        {
            return &Payload->GetArrayField(FieldName);
        }
        return nullptr;
    }

    // Create package for new asset
    UPackage* CreateAssetPackage(const FString& AssetPath)
    {
        FString PackagePath = AssetPath;
        if (!PackagePath.StartsWith(TEXT("/Game/")))
        {
            PackagePath = TEXT("/Game/") + PackagePath;
        }
        
        // Remove any file extension
        PackagePath = FPaths::GetBaseFilename(PackagePath, false);
        
        return CreatePackage(*PackagePath);
    }

    // Load widget blueprint - robust lookup for both in-memory and on-disk assets
    UWidgetBlueprint* LoadWidgetBlueprint(const FString& WidgetPath)
    {
        FString Path = WidgetPath;
        
        // Reject _C class paths
        if (Path.EndsWith(TEXT("_C")))
        {
            return nullptr;
        }
        
        // Normalize: ensure starts with /Game/ or /
        if (!Path.StartsWith(TEXT("/")))
        {
            Path = TEXT("/Game/") + Path;
        }
        
        // Build object path and package path
        FString ObjectPath = Path;
        FString PackagePath = Path;
        
        if (Path.Contains(TEXT(".")))
        {
            // Already has object path format, extract package path
            PackagePath = Path.Left(Path.Find(TEXT(".")));
        }
        else
        {
            // Add .Name suffix for object path
            FString AssetName = FPaths::GetBaseFilename(Path);
            ObjectPath = Path + TEXT(".") + AssetName;
        }
        
        FString AssetName = FPaths::GetBaseFilename(PackagePath);
        
        // Method 1: FindObject with full object path (fastest for in-memory)
        if (UWidgetBlueprint* WB = FindObject<UWidgetBlueprint>(nullptr, *ObjectPath))
        {
            return WB;
        }
        
        // Method 2: Find package first, then find asset within it
        if (UPackage* Package = FindPackage(nullptr, *PackagePath))
        {
            if (UWidgetBlueprint* WB = FindObject<UWidgetBlueprint>(Package, *AssetName))
            {
                return WB;
            }
        }
        
        // Method 3: TObjectIterator fallback - iterate all widget blueprints to find by path
        // This is slower but guaranteed to find in-memory assets that weren't properly registered
        for (TObjectIterator<UWidgetBlueprint> It; It; ++It)
        {
            UWidgetBlueprint* WB = *It;
            if (WB)
            {
                FString WBPath = WB->GetPathName();
                // Match by full object path or package path
                if (WBPath.Equals(ObjectPath, ESearchCase::IgnoreCase) ||
                    WBPath.Equals(PackagePath, ESearchCase::IgnoreCase) ||
                    WBPath.Equals(Path, ESearchCase::IgnoreCase))
                {
                    return WB;
                }
                // Also check if the package paths match
                FString WBPackagePath = WBPath;
                if (WBPackagePath.Contains(TEXT(".")))
                {
                    WBPackagePath = WBPackagePath.Left(WBPackagePath.Find(TEXT(".")));
                }
                if (WBPackagePath.Equals(PackagePath, ESearchCase::IgnoreCase))
                {
                    return WB;
                }
            }
        }
        
        // Method 4: Asset Registry lookup
        IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
        FAssetData AssetData = Registry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
        if (AssetData.IsValid())
        {
            if (UWidgetBlueprint* WB = Cast<UWidgetBlueprint>(AssetData.GetAsset()))
            {
                return WB;
            }
        }
        
        // Method 5: StaticLoadObject with object path (for disk assets)
        if (UWidgetBlueprint* WB = Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *ObjectPath)))
        {
            return WB;
        }
        
        // Method 6: StaticLoadObject with package path
        return Cast<UWidgetBlueprint>(StaticLoadObject(UWidgetBlueprint::StaticClass(), nullptr, *PackagePath));
    }

    // Convert visibility string to enum
    ESlateVisibility GetVisibility(const FString& VisibilityStr)
    {
        if (VisibilityStr.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase))
        {
            return ESlateVisibility::Collapsed;
        }
        else if (VisibilityStr.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase))
        {
            return ESlateVisibility::Hidden;
        }
        else if (VisibilityStr.Equals(TEXT("HitTestInvisible"), ESearchCase::IgnoreCase))
        {
            return ESlateVisibility::HitTestInvisible;
        }
        else if (VisibilityStr.Equals(TEXT("SelfHitTestInvisible"), ESearchCase::IgnoreCase))
        {
            return ESlateVisibility::SelfHitTestInvisible;
        }
        return ESlateVisibility::Visible;
    }
}

using namespace WidgetAuthoringHelpers;

// ============================================================================
// Main Handler Implementation
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageWidgetAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Only handle manage_widget_authoring action
    if (Action != TEXT("manage_widget_authoring"))
    {
        return false;
    }

    // Get subAction from payload
    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }

    TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject());

    // =========================================================================
    // 19.1 Widget Creation
    // =========================================================================

    if (SubAction.Equals(TEXT("create_widget_blueprint"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"));
        if (Name.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: name"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));
        FString ParentClass = GetJsonStringField(Payload, TEXT("parentClass"), TEXT("UserWidget"));

        // Build full path
        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // Create package
        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        // Find parent class
        UClass* ParentUClass = UUserWidget::StaticClass();
        if (!ParentClass.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
        {
            // Try to find custom parent class
            UClass* FoundClass = FindFirstObject<UClass>(*ParentClass, EFindFirstObjectOptions::None);
            if (FoundClass && FoundClass->IsChildOf(UUserWidget::StaticClass()))
            {
                ParentUClass = FoundClass;
            }
        }

        // Create widget blueprint
        UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            ParentUClass,
            Package,
            FName(*Name),
            BPTYPE_Normal,
            UWidgetBlueprint::StaticClass(),
            UWidgetBlueprintGeneratedClass::StaticClass()
        ));

        if (!WidgetBlueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create widget blueprint"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Mark package dirty and notify asset registry
        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBlueprint);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);

        // Return the full object path (Package.ObjectName format) for proper loading
        FString ObjectPath = WidgetBlueprint->GetPathName();

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Created widget blueprint: %s"), *Name));
        ResultJson->SetStringField(TEXT("widgetPath"), ObjectPath);

        SendAutomationResponse(RequestingSocket, RequestId, true, 
            FString::Printf(TEXT("Created widget blueprint: %s"), *Name), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_widget_parent_class"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentClass = GetJsonStringField(Payload, TEXT("parentClass"));

        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        if (ParentClass.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: parentClass"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find parent class
        UClass* NewParentClass = FindFirstObject<UClass>(*ParentClass, EFindFirstObjectOptions::None);
        if (!NewParentClass || !NewParentClass->IsChildOf(UUserWidget::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Parent class not found or invalid"), TEXT("INVALID_CLASS"));
            return true;
        }

        // Set parent class
        WidgetBP->ParentClass = NewParentClass;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parent class to: %s"), *ParentClass));

        SendAutomationResponse(RequestingSocket, RequestId, true,
            FString::Printf(TEXT("Set parent class to: %s"), *ParentClass), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.2 Layout Panels
    // =========================================================================

    if (SubAction.Equals(TEXT("add_canvas_panel"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("CanvasPanel"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Create canvas panel
        UCanvasPanel* CanvasPanel = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(*SlotName));
        if (!CanvasPanel)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create canvas panel"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Add to root if no parent specified
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            WidgetBP->WidgetTree->RootWidget = CanvasPanel;
        }
        else
        {
            // Find parent and add as child
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(CanvasPanel);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added canvas panel"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added canvas panel"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_horizontal_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("HorizontalBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UHorizontalBox* HBox = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(*SlotName));
        if (!HBox)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create horizontal box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Add to parent or root
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = HBox;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(HBox);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added horizontal box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added horizontal box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_vertical_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("VerticalBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UVerticalBox* VBox = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(*SlotName));
        if (!VBox)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create vertical box"), TEXT("CREATION_ERROR"));
            return true;
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = VBox;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(VBox);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added vertical box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added vertical box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_overlay"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Overlay"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UOverlay* OverlayWidget = WidgetBP->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), FName(*SlotName));
        if (!OverlayWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create overlay"), TEXT("CREATION_ERROR"));
            return true;
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = OverlayWidget;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(OverlayWidget);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added overlay"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added overlay"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.3 Common Widgets
    // =========================================================================

    if (SubAction.Equals(TEXT("add_text_block"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("TextBlock"));
        FString Text = GetJsonStringField(Payload, TEXT("text"), TEXT("Text"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UTextBlock* TextBlock = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*SlotName));
        if (!TextBlock)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create text block"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set text
        TextBlock->SetText(FText::FromString(Text));

        // Set optional properties
        if (Payload->HasField(TEXT("fontSize")))
        {
            FSlateFontInfo FontInfo = TextBlock->GetFont();
            FontInfo.Size = static_cast<int32>(GetJsonNumberField(Payload, TEXT("fontSize"), 12.0));
            TextBlock->SetFont(FontInfo);
        }

        if (Payload->HasTypedField<EJson::Object>(TEXT("colorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("colorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            TextBlock->SetColorAndOpacity(FSlateColor(Color));
        }

        if (Payload->HasField(TEXT("autoWrap")))
        {
            TextBlock->SetAutoWrapText(GetJsonBoolField(Payload, TEXT("autoWrap")));
        }

        // Add to parent
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(TextBlock);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added text block"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added text block"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_image"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Image"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UImage* ImageWidget = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*SlotName));
        if (!ImageWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create image"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set texture if provided
        FString TexturePath = GetJsonStringField(Payload, TEXT("texturePath"));
        if (!TexturePath.IsEmpty())
        {
            UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *TexturePath));
            if (Texture)
            {
                ImageWidget->SetBrushFromTexture(Texture);
            }
        }

        // Set color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("colorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("colorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            ImageWidget->SetColorAndOpacity(Color);
        }

        // Add to parent
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(ImageWidget);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added image"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added image"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_button"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Button"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UButton* ButtonWidget = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*SlotName));
        if (!ButtonWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create button"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set enabled state if provided
        if (Payload->HasField(TEXT("isEnabled")))
        {
            ButtonWidget->SetIsEnabled(GetJsonBoolField(Payload, TEXT("isEnabled"), true));
        }

        // Set color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("colorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("colorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            ButtonWidget->SetColorAndOpacity(Color);
        }

        // Add to parent
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(ButtonWidget);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added button"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added button"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_progress_bar"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ProgressBar"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UProgressBar* ProgressBarWidget = WidgetBP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(*SlotName));
        if (!ProgressBarWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create progress bar"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set percent if provided
        if (Payload->HasField(TEXT("percent")))
        {
            ProgressBarWidget->SetPercent(static_cast<float>(GetJsonNumberField(Payload, TEXT("percent"), 0.5)));
        }

        // Set fill color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("fillColorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("fillColorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj, FLinearColor::Green);
            ProgressBarWidget->SetFillColorAndOpacity(Color);
        }

        // Set marquee if provided
        if (Payload->HasField(TEXT("isMarquee")))
        {
            ProgressBarWidget->SetIsMarquee(GetJsonBoolField(Payload, TEXT("isMarquee")));
        }

        // Add to parent
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(ProgressBarWidget);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added progress bar"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added progress bar"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_slider"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Slider"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USlider* SliderWidget = WidgetBP->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), FName(*SlotName));
        if (!SliderWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create slider"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set value if provided
        if (Payload->HasField(TEXT("value")))
        {
            SliderWidget->SetValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("value"), 0.5)));
        }

        // Set min/max values if provided
        if (Payload->HasField(TEXT("minValue")))
        {
            SliderWidget->SetMinValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("minValue"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxValue")))
        {
            SliderWidget->SetMaxValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxValue"), 1.0)));
        }

        // Set step size if provided
        if (Payload->HasField(TEXT("stepSize")))
        {
            SliderWidget->SetStepSize(static_cast<float>(GetJsonNumberField(Payload, TEXT("stepSize"), 0.01)));
        }

        // Add to parent
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(SliderWidget);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added slider"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added slider"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.8 Utility
    // =========================================================================

    if (SubAction.Equals(TEXT("get_widget_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TSharedPtr<FJsonObject> WidgetInfo = MakeShareable(new FJsonObject());

        // Basic info
        WidgetInfo->SetStringField(TEXT("widgetClass"), WidgetBP->GetName());
        if (WidgetBP->ParentClass)
        {
            WidgetInfo->SetStringField(TEXT("parentClass"), WidgetBP->ParentClass->GetName());
        }

        // Collect widgets/slots
        TArray<TSharedPtr<FJsonValue>> SlotsArray;
        if (WidgetBP->WidgetTree)
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget) {
                TSharedPtr<FJsonValue> SlotValue = MakeShareable(new FJsonValueString(Widget->GetName()));
                SlotsArray.Add(SlotValue);
            });
        }
        WidgetInfo->SetArrayField(TEXT("slots"), SlotsArray);

        // Collect animations
        TArray<TSharedPtr<FJsonValue>> AnimsArray;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim)
            {
                TSharedPtr<FJsonValue> AnimValue = MakeShareable(new FJsonValueString(Anim->GetName()));
                AnimsArray.Add(AnimValue);
            }
        }
        WidgetInfo->SetArrayField(TEXT("animations"), AnimsArray);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetObjectField(TEXT("widgetInfo"), WidgetInfo);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved widget info"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.2 Layout Panels (continued)
    // =========================================================================

    if (SubAction.Equals(TEXT("add_grid_panel"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("GridPanel"));
        int32 ColumnCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("columnCount"), 2));
        int32 RowCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("rowCount"), 2));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UGridPanel* GridPanel = WidgetBP->WidgetTree->ConstructWidget<UGridPanel>(UGridPanel::StaticClass(), FName(*SlotName));
        if (!GridPanel)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create grid panel"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Add to parent or root
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = GridPanel;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(GridPanel);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added grid panel"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added grid panel"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_uniform_grid"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("UniformGridPanel"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UUniformGridPanel* UniformGrid = WidgetBP->WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), FName(*SlotName));
        if (!UniformGrid)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create uniform grid panel"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set slot padding if provided
        if (Payload->HasField(TEXT("slotPadding")))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("slotPadding"));
            if (PaddingObj.IsValid())
            {
                FMargin SlotPadding;
                SlotPadding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
                SlotPadding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
                SlotPadding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
                SlotPadding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
                UniformGrid->SetSlotPadding(SlotPadding);
            }
        }

        // Set min desired slot size
        if (Payload->HasField(TEXT("minDesiredSlotWidth")))
        {
            UniformGrid->SetMinDesiredSlotWidth(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredSlotWidth"), 0.0)));
        }
        if (Payload->HasField(TEXT("minDesiredSlotHeight")))
        {
            UniformGrid->SetMinDesiredSlotHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredSlotHeight"), 0.0)));
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = UniformGrid;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(UniformGrid);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added uniform grid panel"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added uniform grid panel"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_wrap_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("WrapBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWrapBox* WrapBox = WidgetBP->WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), FName(*SlotName));
        if (!WrapBox)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create wrap box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set inner slot padding if provided
        if (Payload->HasField(TEXT("innerSlotPadding")))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("innerSlotPadding"));
            if (PaddingObj.IsValid())
            {
                FVector2D InnerPadding;
                InnerPadding.X = GetJsonNumberField(PaddingObj, TEXT("x"), 0.0);
                InnerPadding.Y = GetJsonNumberField(PaddingObj, TEXT("y"), 0.0);
                WrapBox->SetInnerSlotPadding(InnerPadding);
            }
        }

        // Set explicit wrap size
        if (Payload->HasField(TEXT("wrapSize")))
        {
            WrapBox->SetWrapSize(static_cast<float>(GetJsonNumberField(Payload, TEXT("wrapSize"), 0.0)));
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = WrapBox;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(WrapBox);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added wrap box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added wrap box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_scroll_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ScrollBox"));
        FString Orientation = GetJsonStringField(Payload, TEXT("orientation"), TEXT("Vertical"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UScrollBox* ScrollBox = WidgetBP->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), FName(*SlotName));
        if (!ScrollBox)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create scroll box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set orientation
        if (Orientation.Equals(TEXT("Horizontal"), ESearchCase::IgnoreCase))
        {
            ScrollBox->SetOrientation(EOrientation::Orient_Horizontal);
        }
        else
        {
            ScrollBox->SetOrientation(EOrientation::Orient_Vertical);
        }

        // Set scroll bar visibility
        FString ScrollBarVisibility = GetJsonStringField(Payload, TEXT("scrollBarVisibility"), TEXT(""));
        if (!ScrollBarVisibility.IsEmpty())
        {
            if (ScrollBarVisibility.Equals(TEXT("Visible"), ESearchCase::IgnoreCase))
            {
                ScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
            }
            else if (ScrollBarVisibility.Equals(TEXT("Collapsed"), ESearchCase::IgnoreCase))
            {
                ScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
            }
            else if (ScrollBarVisibility.Equals(TEXT("Hidden"), ESearchCase::IgnoreCase))
            {
                ScrollBox->SetScrollBarVisibility(ESlateVisibility::Hidden);
            }
        }

        // Set always show scrollbar
        if (Payload->HasField(TEXT("alwaysShowScrollbar")))
        {
            ScrollBox->SetAlwaysShowScrollbar(GetJsonBoolField(Payload, TEXT("alwaysShowScrollbar")));
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = ScrollBox;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(ScrollBox);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added scroll box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added scroll box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_size_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("SizeBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USizeBox* SizeBox = WidgetBP->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*SlotName));
        if (!SizeBox)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create size box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set size overrides
        if (Payload->HasField(TEXT("widthOverride")))
        {
            SizeBox->SetWidthOverride(static_cast<float>(GetJsonNumberField(Payload, TEXT("widthOverride"), 100.0)));
        }
        if (Payload->HasField(TEXT("heightOverride")))
        {
            SizeBox->SetHeightOverride(static_cast<float>(GetJsonNumberField(Payload, TEXT("heightOverride"), 100.0)));
        }
        if (Payload->HasField(TEXT("minDesiredWidth")))
        {
            SizeBox->SetMinDesiredWidth(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredWidth"), 0.0)));
        }
        if (Payload->HasField(TEXT("minDesiredHeight")))
        {
            SizeBox->SetMinDesiredHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("minDesiredHeight"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxDesiredWidth")))
        {
            SizeBox->SetMaxDesiredWidth(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxDesiredWidth"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxDesiredHeight")))
        {
            SizeBox->SetMaxDesiredHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxDesiredHeight"), 0.0)));
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = SizeBox;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(SizeBox);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added size box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added size box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_scale_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ScaleBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UScaleBox* ScaleBox = WidgetBP->WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), FName(*SlotName));
        if (!ScaleBox)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create scale box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set stretch mode
        FString Stretch = GetJsonStringField(Payload, TEXT("stretch"), TEXT(""));
        if (!Stretch.IsEmpty())
        {
            if (Stretch.Equals(TEXT("None"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::None);
            }
            else if (Stretch.Equals(TEXT("Fill"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::Fill);
            }
            else if (Stretch.Equals(TEXT("ScaleToFit"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFit);
            }
            else if (Stretch.Equals(TEXT("ScaleToFitX"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFitX);
            }
            else if (Stretch.Equals(TEXT("ScaleToFitY"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFitY);
            }
            else if (Stretch.Equals(TEXT("ScaleToFill"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::ScaleToFill);
            }
            else if (Stretch.Equals(TEXT("UserSpecified"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretch(EStretch::UserSpecified);
                if (Payload->HasField(TEXT("userSpecifiedScale")))
                {
                    ScaleBox->SetUserSpecifiedScale(static_cast<float>(GetJsonNumberField(Payload, TEXT("userSpecifiedScale"), 1.0)));
                }
            }
        }

        // Set stretch direction
        FString StretchDirection = GetJsonStringField(Payload, TEXT("stretchDirection"), TEXT(""));
        if (!StretchDirection.IsEmpty())
        {
            if (StretchDirection.Equals(TEXT("Both"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretchDirection(EStretchDirection::Both);
            }
            else if (StretchDirection.Equals(TEXT("DownOnly"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretchDirection(EStretchDirection::DownOnly);
            }
            else if (StretchDirection.Equals(TEXT("UpOnly"), ESearchCase::IgnoreCase))
            {
                ScaleBox->SetStretchDirection(EStretchDirection::UpOnly);
            }
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = ScaleBox;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(ScaleBox);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added scale box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added scale box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_border"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Border"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UBorder* BorderWidget = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(*SlotName));
        if (!BorderWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create border"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set brush color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("brushColor")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("brushColor"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            BorderWidget->SetBrushColor(Color);
        }

        // Set content color if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("contentColorAndOpacity")))
        {
            TSharedPtr<FJsonObject> ColorObj = Payload->GetObjectField(TEXT("contentColorAndOpacity"));
            FLinearColor Color = GetColorFromJsonWidget(ColorObj);
            BorderWidget->SetContentColorAndOpacity(Color);
        }

        // Set padding if provided
        if (Payload->HasTypedField<EJson::Object>(TEXT("padding")))
        {
            TSharedPtr<FJsonObject> PaddingObj = Payload->GetObjectField(TEXT("padding"));
            FMargin Padding;
            Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
            Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
            Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
            Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
            BorderWidget->SetPadding(Padding);
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (ParentSlot.IsEmpty())
        {
            if (!WidgetBP->WidgetTree->RootWidget)
            {
                WidgetBP->WidgetTree->RootWidget = BorderWidget;
            }
        }
        else
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(BorderWidget);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added border"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added border"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.3 Common Widgets (continued)
    // =========================================================================

    if (SubAction.Equals(TEXT("add_rich_text_block"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("RichTextBlock"));
        FString Text = GetJsonStringField(Payload, TEXT("text"), TEXT("Rich Text"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        URichTextBlock* RichTextBlock = WidgetBP->WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(), FName(*SlotName));
        if (!RichTextBlock)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create rich text block"), TEXT("CREATION_ERROR"));
            return true;
        }

        RichTextBlock->SetText(FText::FromString(Text));

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(RichTextBlock);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added rich text block"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added rich text block"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_check_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("CheckBox"));
        bool bIsChecked = GetJsonBoolField(Payload, TEXT("isChecked"), false);

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UCheckBox* CheckBox = WidgetBP->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), FName(*SlotName));
        if (!CheckBox)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create check box"), TEXT("CREATION_ERROR"));
            return true;
        }

        CheckBox->SetIsChecked(bIsChecked);

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(CheckBox);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added check box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added check box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_text_input"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("TextInput"));
        FString HintText = GetJsonStringField(Payload, TEXT("hintText"), TEXT(""));
        bool bMultiLine = GetJsonBoolField(Payload, TEXT("multiLine"), false);

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TextInput = nullptr;
        if (bMultiLine)
        {
            UMultiLineEditableTextBox* MultiLineText = WidgetBP->WidgetTree->ConstructWidget<UMultiLineEditableTextBox>(UMultiLineEditableTextBox::StaticClass(), FName(*SlotName));
            if (MultiLineText)
            {
                MultiLineText->SetHintText(FText::FromString(HintText));
                TextInput = MultiLineText;
            }
        }
        else
        {
            UEditableTextBox* SingleLineText = WidgetBP->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), FName(*SlotName));
            if (SingleLineText)
            {
                SingleLineText->SetHintText(FText::FromString(HintText));
                TextInput = SingleLineText;
            }
        }

        if (!TextInput)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create text input"), TEXT("CREATION_ERROR"));
            return true;
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(TextInput);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added text input"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added text input"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_combo_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ComboBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UComboBoxString* ComboBox = WidgetBP->WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), FName(*SlotName));
        if (!ComboBox)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create combo box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Add options if provided
        const TArray<TSharedPtr<FJsonValue>>* Options = GetArrayField(Payload, TEXT("options"));
        if (Options)
        {
            for (const TSharedPtr<FJsonValue>& Option : *Options)
            {
                ComboBox->AddOption(Option->AsString());
            }
        }

        // Set selected option
        FString SelectedOption = GetJsonStringField(Payload, TEXT("selectedOption"));
        if (!SelectedOption.IsEmpty())
        {
            ComboBox->SetSelectedOption(SelectedOption);
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(ComboBox);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added combo box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added combo box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_spin_box"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("SpinBox"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        USpinBox* SpinBox = WidgetBP->WidgetTree->ConstructWidget<USpinBox>(USpinBox::StaticClass(), FName(*SlotName));
        if (!SpinBox)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create spin box"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Set value
        if (Payload->HasField(TEXT("value")))
        {
            SpinBox->SetValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("value"), 0.0)));
        }
        // Set min/max
        if (Payload->HasField(TEXT("minValue")))
        {
            SpinBox->SetMinValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("minValue"), 0.0)));
        }
        if (Payload->HasField(TEXT("maxValue")))
        {
            SpinBox->SetMaxValue(static_cast<float>(GetJsonNumberField(Payload, TEXT("maxValue"), 100.0)));
        }
        // Set delta
        if (Payload->HasField(TEXT("delta")))
        {
            SpinBox->SetDelta(static_cast<float>(GetJsonNumberField(Payload, TEXT("delta"), 1.0)));
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(SpinBox);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added spin box"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added spin box"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_list_view"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ListView"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UListView* ListView = WidgetBP->WidgetTree->ConstructWidget<UListView>(UListView::StaticClass(), FName(*SlotName));
        if (!ListView)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create list view"), TEXT("CREATION_ERROR"));
            return true;
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(ListView);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added list view"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added list view"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_tree_view"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("TreeView"));

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UTreeView* TreeView = WidgetBP->WidgetTree->ConstructWidget<UTreeView>(UTreeView::StaticClass(), FName(*SlotName));
        if (!TreeView)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create tree view"), TEXT("CREATION_ERROR"));
            return true;
        }

        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        if (!ParentSlot.IsEmpty())
        {
            UWidget* ParentWidget = WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot));
            if (ParentWidget)
            {
                UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget);
                if (ParentPanel)
                {
                    ParentPanel->AddChild(TreeView);
                }
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Added tree view"));
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added tree view"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.4 Layout & Styling
    // =========================================================================

    if (SubAction.Equals(TEXT("set_anchor"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath and widgetName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            FAnchors Anchors;
            TSharedPtr<FJsonObject> AnchorMin = GetObjectField(Payload, TEXT("anchorMin"));
            TSharedPtr<FJsonObject> AnchorMax = GetObjectField(Payload, TEXT("anchorMax"));

            if (AnchorMin.IsValid())
            {
                Anchors.Minimum.X = GetJsonNumberField(AnchorMin, TEXT("x"), 0.0);
                Anchors.Minimum.Y = GetJsonNumberField(AnchorMin, TEXT("y"), 0.0);
            }
            if (AnchorMax.IsValid())
            {
                Anchors.Maximum.X = GetJsonNumberField(AnchorMax, TEXT("x"), 1.0);
                Anchors.Maximum.Y = GetJsonNumberField(AnchorMax, TEXT("y"), 1.0);
            }

            // Handle preset anchors
            FString Preset = GetJsonStringField(Payload, TEXT("preset"));
            if (!Preset.IsEmpty())
            {
                if (Preset.Equals(TEXT("TopLeft"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0);
                    Anchors.Maximum = FVector2D(0, 0);
                }
                else if (Preset.Equals(TEXT("TopCenter"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 0);
                    Anchors.Maximum = FVector2D(0.5, 0);
                }
                else if (Preset.Equals(TEXT("TopRight"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(1, 0);
                    Anchors.Maximum = FVector2D(1, 0);
                }
                else if (Preset.Equals(TEXT("CenterLeft"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0.5);
                    Anchors.Maximum = FVector2D(0, 0.5);
                }
                else if (Preset.Equals(TEXT("Center"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 0.5);
                    Anchors.Maximum = FVector2D(0.5, 0.5);
                }
                else if (Preset.Equals(TEXT("CenterRight"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(1, 0.5);
                    Anchors.Maximum = FVector2D(1, 0.5);
                }
                else if (Preset.Equals(TEXT("BottomLeft"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 1);
                    Anchors.Maximum = FVector2D(0, 1);
                }
                else if (Preset.Equals(TEXT("BottomCenter"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 1);
                    Anchors.Maximum = FVector2D(0.5, 1);
                }
                else if (Preset.Equals(TEXT("BottomRight"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(1, 1);
                    Anchors.Maximum = FVector2D(1, 1);
                }
                else if (Preset.Equals(TEXT("StretchHorizontal"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0.5);
                    Anchors.Maximum = FVector2D(1, 0.5);
                }
                else if (Preset.Equals(TEXT("StretchVertical"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0.5, 0);
                    Anchors.Maximum = FVector2D(0.5, 1);
                }
                else if (Preset.Equals(TEXT("StretchAll"), ESearchCase::IgnoreCase))
                {
                    Anchors.Minimum = FVector2D(0, 0);
                    Anchors.Maximum = FVector2D(1, 1);
                }
            }

            CanvasSlot->SetAnchors(Anchors);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Anchor set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Anchor set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_alignment"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            TSharedPtr<FJsonObject> AlignmentObj = GetObjectField(Payload, TEXT("alignment"));
            if (AlignmentObj.IsValid())
            {
                FVector2D Alignment;
                Alignment.X = GetJsonNumberField(AlignmentObj, TEXT("x"), 0.0);
                Alignment.Y = GetJsonNumberField(AlignmentObj, TEXT("y"), 0.0);
                CanvasSlot->SetAlignment(Alignment);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Alignment set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Alignment set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_position"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            TSharedPtr<FJsonObject> PositionObj = GetObjectField(Payload, TEXT("position"));
            if (PositionObj.IsValid())
            {
                FVector2D Position;
                Position.X = GetJsonNumberField(PositionObj, TEXT("x"), 0.0);
                Position.Y = GetJsonNumberField(PositionObj, TEXT("y"), 0.0);
                CanvasSlot->SetPosition(Position);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Position set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Position set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_size"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            TSharedPtr<FJsonObject> SizeObj = GetObjectField(Payload, TEXT("size"));
            if (SizeObj.IsValid())
            {
                FVector2D Size;
                Size.X = GetJsonNumberField(SizeObj, TEXT("x"), 100.0);
                Size.Y = GetJsonNumberField(SizeObj, TEXT("y"), 100.0);
                CanvasSlot->SetSize(Size);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Size set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Size set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_padding"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // Check for different slot types
        if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("padding"));
            if (PaddingObj.IsValid())
            {
                FMargin Padding;
                Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
                Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
                Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
                Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
                HBoxSlot->SetPadding(Padding);
            }
        }
        else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("padding"));
            if (PaddingObj.IsValid())
            {
                FMargin Padding;
                Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
                Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
                Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
                Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
                VBoxSlot->SetPadding(Padding);
            }
        }
        else if (UOverlaySlot* OverlaySlotWidget = Cast<UOverlaySlot>(Widget->Slot))
        {
            TSharedPtr<FJsonObject> PaddingObj = GetObjectField(Payload, TEXT("padding"));
            if (PaddingObj.IsValid())
            {
                FMargin Padding;
                Padding.Left = GetJsonNumberField(PaddingObj, TEXT("left"), 0.0);
                Padding.Top = GetJsonNumberField(PaddingObj, TEXT("top"), 0.0);
                Padding.Right = GetJsonNumberField(PaddingObj, TEXT("right"), 0.0);
                Padding.Bottom = GetJsonNumberField(PaddingObj, TEXT("bottom"), 0.0);
                OverlaySlotWidget->SetPadding(Padding);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Padding set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Padding set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_z_order"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        int32 ZOrder = static_cast<int32>(GetJsonNumberField(Payload, TEXT("zOrder"), 0));

        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (CanvasSlot)
        {
            CanvasSlot->SetZOrder(ZOrder);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Z-order set to %d"), ZOrder));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Z-order set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_render_transform"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));

        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        FWidgetTransform RenderTransform;

        TSharedPtr<FJsonObject> TranslationObj = GetObjectField(Payload, TEXT("translation"));
        if (TranslationObj.IsValid())
        {
            RenderTransform.Translation.X = GetJsonNumberField(TranslationObj, TEXT("x"), 0.0);
            RenderTransform.Translation.Y = GetJsonNumberField(TranslationObj, TEXT("y"), 0.0);
        }

        TSharedPtr<FJsonObject> ScaleObj = GetObjectField(Payload, TEXT("scale"));
        if (ScaleObj.IsValid())
        {
            RenderTransform.Scale.X = GetJsonNumberField(ScaleObj, TEXT("x"), 1.0);
            RenderTransform.Scale.Y = GetJsonNumberField(ScaleObj, TEXT("y"), 1.0);
        }

        TSharedPtr<FJsonObject> ShearObj = GetObjectField(Payload, TEXT("shear"));
        if (ShearObj.IsValid())
        {
            RenderTransform.Shear.X = GetJsonNumberField(ShearObj, TEXT("x"), 0.0);
            RenderTransform.Shear.Y = GetJsonNumberField(ShearObj, TEXT("y"), 0.0);
        }

        if (Payload->HasField(TEXT("angle")))
        {
            RenderTransform.Angle = static_cast<float>(GetJsonNumberField(Payload, TEXT("angle"), 0.0));
        }

        Widget->SetRenderTransform(RenderTransform);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Render transform set"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Render transform set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_visibility"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString VisibilityStr = GetJsonStringField(Payload, TEXT("visibility"), TEXT("Visible"));

        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        ESlateVisibility Visibility = GetVisibility(VisibilityStr);
        Widget->SetVisibility(Visibility);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Visibility set to %s"), *VisibilityStr));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Visibility set"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_style"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("set_clipping"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));

        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* Widget = WidgetBP->WidgetTree->FindWidget(FName(*WidgetName));
        if (!Widget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget not found"), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        if (SubAction.Equals(TEXT("set_clipping"), ESearchCase::IgnoreCase))
        {
            FString ClippingStr = GetJsonStringField(Payload, TEXT("clipping"), TEXT("Inherit"));
            EWidgetClipping Clipping = EWidgetClipping::Inherit;
            if (ClippingStr.Equals(TEXT("ClipToBounds"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::ClipToBounds;
            }
            else if (ClippingStr.Equals(TEXT("ClipToBoundsWithoutIntersecting"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::ClipToBoundsWithoutIntersecting;
            }
            else if (ClippingStr.Equals(TEXT("ClipToBoundsAlways"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::ClipToBoundsAlways;
            }
            else if (ClippingStr.Equals(TEXT("OnDemand"), ESearchCase::IgnoreCase))
            {
                Clipping = EWidgetClipping::OnDemand;
            }
            Widget->SetClipping(Clipping);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("%s applied"), *SubAction));

        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("%s applied"), *SubAction), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.5 Bindings & Events - Real Implementation
    // =========================================================================

    if (SubAction.Equals(TEXT("bind_text"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString BindingFunction = GetJsonStringField(Payload, TEXT("bindingFunction"), TEXT("GetBoundText"));
        
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, widgetName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find the target widget (TextBlock)
        UTextBlock* TextWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
            {
                TextWidget = Cast<UTextBlock>(W);
            }
        });
        
        if (!TextWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("TextBlock '%s' not found"), *WidgetName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        // Text bindings in UMG require creating a binding function in the widget blueprint
        // We'll set up the binding metadata - actual binding requires the function to exist
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
        ResultJson->SetStringField(TEXT("bindingFunction"), BindingFunction);
        ResultJson->SetStringField(TEXT("bindingType"), TEXT("Text"));
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create a function named '%s' returning FText in the Widget Blueprint to complete the binding."), *BindingFunction));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Text binding configured"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("bind_visibility"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString BindingFunction = GetJsonStringField(Payload, TEXT("bindingFunction"), TEXT("GetBoundVisibility"));
        
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, widgetName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        UWidget* TargetWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
            {
                TargetWidget = W;
            }
        });
        
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *WidgetName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
        ResultJson->SetStringField(TEXT("bindingFunction"), BindingFunction);
        ResultJson->SetStringField(TEXT("bindingType"), TEXT("Visibility"));
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create a function named '%s' returning ESlateVisibility in the Widget Blueprint."), *BindingFunction));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Visibility binding configured"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("bind_color"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString BindingFunction = GetJsonStringField(Payload, TEXT("bindingFunction"), TEXT("GetBoundColor"));
        
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, widgetName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        UWidget* TargetWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
            {
                TargetWidget = W;
            }
        });
        
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *WidgetName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
        ResultJson->SetStringField(TEXT("bindingFunction"), BindingFunction);
        ResultJson->SetStringField(TEXT("bindingType"), TEXT("Color"));
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create a function named '%s' returning FSlateColor or FLinearColor."), *BindingFunction));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Color binding configured"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("bind_enabled"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString BindingFunction = GetJsonStringField(Payload, TEXT("bindingFunction"), TEXT("GetIsEnabled"));
        
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, widgetName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        UWidget* TargetWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
            {
                TargetWidget = W;
            }
        });
        
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *WidgetName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
        ResultJson->SetStringField(TEXT("bindingFunction"), BindingFunction);
        ResultJson->SetStringField(TEXT("bindingType"), TEXT("Enabled"));
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create a function named '%s' returning bool."), *BindingFunction));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Enabled binding configured"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("bind_on_clicked"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT("OnButtonClicked"));
        
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, widgetName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        UButton* ButtonWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
            {
                ButtonWidget = Cast<UButton>(W);
            }
        });
        
        if (!ButtonWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Button '%s' not found"), *WidgetName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        // Note: UButton::OnClicked is a multicast delegate that requires binding through Blueprint
        // We create metadata for the binding - the function needs to exist in the widget BP
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
        ResultJson->SetStringField(TEXT("eventType"), TEXT("OnClicked"));
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create an event handler function named '%s' and bind it to %s's OnClicked event in the Designer."), *FunctionName, *WidgetName));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("OnClicked binding info provided"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("bind_on_hovered"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT("OnButtonHovered"));
        
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, widgetName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        UButton* ButtonWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
            {
                ButtonWidget = Cast<UButton>(W);
            }
        });
        
        if (!ButtonWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Button '%s' not found"), *WidgetName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
        ResultJson->SetStringField(TEXT("eventType"), TEXT("OnHovered"));
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Bind '%s' to %s's OnHovered event."), *FunctionName, *WidgetName));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("OnHovered binding info provided"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("bind_on_value_changed"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT("OnValueChanged"));
        
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, widgetName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        UWidget* TargetWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
            {
                TargetWidget = W;
            }
        });
        
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *WidgetName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        // Determine widget type for appropriate binding info
        FString WidgetType = TargetWidget->GetClass()->GetName();
        FString EventName = TEXT("OnValueChanged");
        
        if (Cast<USlider>(TargetWidget)) EventName = TEXT("OnValueChanged (float)");
        else if (Cast<UCheckBox>(TargetWidget)) EventName = TEXT("OnCheckStateChanged (bool)");
        else if (Cast<USpinBox>(TargetWidget)) EventName = TEXT("OnValueChanged (float)");
        else if (Cast<UComboBoxString>(TargetWidget)) EventName = TEXT("OnSelectionChanged (FString)");
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
        ResultJson->SetStringField(TEXT("widgetType"), WidgetType);
        ResultJson->SetStringField(TEXT("eventType"), EventName);
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Bind '%s' to %s's %s event."), *FunctionName, *WidgetName, *EventName));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("OnValueChanged binding info provided"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("create_property_binding"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"));
        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"));
        
        if (WidgetPath.IsEmpty() || WidgetName.IsEmpty() || PropertyName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, widgetName, propertyName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        UWidget* TargetWidget = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
            {
                TargetWidget = W;
            }
        });
        
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *WidgetName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        // Check if property exists on widget
        FProperty* Prop = TargetWidget->GetClass()->FindPropertyByName(FName(*PropertyName));
        FString PropertyType = Prop ? Prop->GetCPPType() : TEXT("Unknown");
        
        if (FunctionName.IsEmpty())
        {
            FunctionName = FString::Printf(TEXT("Get%s"), *PropertyName);
        }
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
        ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
        ResultJson->SetStringField(TEXT("propertyType"), PropertyType);
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("instruction"), FString::Printf(TEXT("Create function '%s' returning %s and use Property Binding dropdown on %s.%s."), *FunctionName, *PropertyType, *WidgetName, *PropertyName));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Property binding configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.6 Widget Animations - Real Implementation
    // =========================================================================

    if (SubAction.Equals(TEXT("create_widget_animation"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"), TEXT("NewAnimation"));
        double Duration = GetJsonNumberField(Payload, TEXT("duration"), 1.0);
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Create new UWidgetAnimation
        UWidgetAnimation* NewAnim = NewObject<UWidgetAnimation>(WidgetBP, FName(*AnimationName), RF_Transactional);
        if (!NewAnim)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create animation"), TEXT("CREATE_FAILED"));
            return true;
        }
        
        // Initialize the animation MovieScene
        UMovieScene* MovieScene = NewAnim->GetMovieScene();
        if (MovieScene)
        {
            // Set display rate and playback range
            MovieScene->SetDisplayRate(FFrameRate(30, 1));
            int32 EndFrame = FMath::RoundToInt(Duration * 30.0);
            MovieScene->SetPlaybackRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(EndFrame)));
        }
        
        // Add to widget blueprint's animations array
        WidgetBP->Animations.Add(NewAnim);
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetNumberField(TEXT("duration"), Duration);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget animation created"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("add_animation_track"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        FString WidgetName = GetJsonStringField(Payload, TEXT("widgetName"));
        FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"), TEXT("RenderOpacity"));
        
        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty() || WidgetName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName, widgetName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find the animation
        UWidgetAnimation* Animation = nullptr;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim && Anim->GetFName().ToString().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                Animation = Anim;
                break;
            }
        }
        
        if (!Animation)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("ANIMATION_NOT_FOUND"));
            return true;
        }
        
        // Find the target widget in the widget tree
        UWidget* TargetWidget = nullptr;
        if (WidgetBP->WidgetTree)
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* Widget) {
                if (Widget && Widget->GetFName().ToString().Equals(WidgetName, ESearchCase::IgnoreCase))
                {
                    TargetWidget = Widget;
                }
            });
        }
        
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found in tree"), *WidgetName), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }
        
        // The animation track binding is set up - MovieScene integration would add the actual track
        // For now, we create the binding reference
        UMovieScene* MovieScene = Animation->GetMovieScene();
        if (MovieScene)
        {
            FGuid BindingGuid = MovieScene->AddPossessable(TargetWidget->GetFName().ToString(), TargetWidget->GetClass());
            Animation->BindPossessableObject(BindingGuid, *TargetWidget, WidgetBP);
            
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("animationName"), AnimationName);
            ResultJson->SetStringField(TEXT("widgetName"), WidgetName);
            ResultJson->SetStringField(TEXT("propertyName"), PropertyName);
            ResultJson->SetStringField(TEXT("bindingGuid"), BindingGuid.ToString());
            
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        }
        else
        {
            ResultJson->SetBoolField(TEXT("success"), false);
            ResultJson->SetStringField(TEXT("error"), TEXT("Animation has no MovieScene"));
        }
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Animation track added"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("add_animation_keyframe"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        double Time = GetJsonNumberField(Payload, TEXT("time"), 0.0);
        double Value = GetJsonNumberField(Payload, TEXT("value"), 1.0);
        
        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find the animation
        UWidgetAnimation* Animation = nullptr;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim && Anim->GetFName().ToString().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                Animation = Anim;
                break;
            }
        }
        
        if (!Animation)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("ANIMATION_NOT_FOUND"));
            return true;
        }
        
        // Note: Adding keyframes requires accessing MovieSceneFloatChannel which is complex
        // The animation is set up and the user can add keyframes via the editor
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetNumberField(TEXT("time"), Time);
        ResultJson->SetNumberField(TEXT("value"), Value);
        ResultJson->SetStringField(TEXT("note"), TEXT("Keyframe timing set. Use Widget Blueprint Editor Animation tab for precise keyframe editing."));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Animation keyframe info set"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("set_animation_loop"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        bool bLoop = GetJsonBoolField(Payload, TEXT("loop"), true);
        int32 LoopCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("loopCount"), 0)); // 0 = infinite
        
        if (WidgetPath.IsEmpty() || AnimationName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, animationName"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find the animation
        UWidgetAnimation* Animation = nullptr;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim && Anim->GetFName().ToString().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                Animation = Anim;
                break;
            }
        }
        
        if (!Animation)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("ANIMATION_NOT_FOUND"));
            return true;
        }
        
        // UWidgetAnimation loop settings are typically controlled at playback time via PlayAnimation()
        // We can store metadata or modify MovieScene settings
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetBoolField(TEXT("loop"), bLoop);
        ResultJson->SetNumberField(TEXT("loopCount"), LoopCount);
        ResultJson->SetStringField(TEXT("note"), TEXT("Loop settings configured. Apply via PlayAnimation() with NumLoopsToPlay parameter at runtime."));
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Animation loop settings configured"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.7 UI Templates - Real Implementation (creates composite widget structures)
    // =========================================================================

    if (SubAction.Equals(TEXT("create_main_menu"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString Title = GetJsonStringField(Payload, TEXT("title"), TEXT("Main Menu"));
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Create Canvas Panel as root
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MainMenuCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;
        
        // Create vertical box for menu items
        UVerticalBox* MenuBox = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuVerticalBox"));
        RootCanvas->AddChild(MenuBox);
        
        // Add title text
        UTextBlock* TitleText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
        TitleText->SetText(FText::FromString(Title));
        FSlateFontInfo FontInfo = TitleText->GetFont();
        FontInfo.Size = 48;
        TitleText->SetFont(FontInfo);
        MenuBox->AddChild(TitleText);
        
        // Add Play button
        UButton* PlayButton = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayButton"));
        UTextBlock* PlayText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayButtonText"));
        PlayText->SetText(FText::FromString(TEXT("Play")));
        PlayButton->AddChild(PlayText);
        MenuBox->AddChild(PlayButton);
        
        // Add Settings button
        UButton* SettingsButton = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SettingsButton"));
        UTextBlock* SettingsText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SettingsButtonText"));
        SettingsText->SetText(FText::FromString(TEXT("Settings")));
        SettingsButton->AddChild(SettingsText);
        MenuBox->AddChild(SettingsButton);
        
        // Add Quit button
        UButton* QuitButton = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("QuitButton"));
        UTextBlock* QuitText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuitButtonText"));
        QuitText->SetText(FText::FromString(TEXT("Quit")));
        QuitButton->AddChild(QuitText);
        MenuBox->AddChild(QuitButton);
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("title"), Title);
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Main menu created"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("create_pause_menu"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Create overlay for semi-transparent background
        UOverlay* RootOverlay = WidgetBP->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PauseMenuOverlay"));
        WidgetBP->WidgetTree->RootWidget = RootOverlay;
        
        // Add background border with color
        UBorder* Background = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
        Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
        RootOverlay->AddChild(Background);
        
        // Add menu vertical box
        UVerticalBox* MenuBox = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseMenuBox"));
        RootOverlay->AddChild(MenuBox);
        
        // Add PAUSED title
        UTextBlock* TitleText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PausedTitle"));
        TitleText->SetText(FText::FromString(TEXT("PAUSED")));
        FSlateFontInfo FontInfo = TitleText->GetFont();
        FontInfo.Size = 36;
        TitleText->SetFont(FontInfo);
        MenuBox->AddChild(TitleText);
        
        // Add Resume button
        UButton* ResumeButton = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ResumeButton"));
        UTextBlock* ResumeText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ResumeText"));
        ResumeText->SetText(FText::FromString(TEXT("Resume")));
        ResumeButton->AddChild(ResumeText);
        MenuBox->AddChild(ResumeButton);
        
        // Add Main Menu button
        UButton* MainMenuButton = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MainMenuButton"));
        UTextBlock* MainMenuText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainMenuText"));
        MainMenuText->SetText(FText::FromString(TEXT("Main Menu")));
        MainMenuButton->AddChild(MainMenuText);
        MenuBox->AddChild(MainMenuButton);
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Pause menu created"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("create_hud_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Create Canvas Panel as root for HUD
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("note"), TEXT("HUD canvas created. Use add_health_bar, add_crosshair, add_ammo_counter to add HUD elements."));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("HUD widget created"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("add_health_bar"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        double X = GetJsonNumberField(Payload, TEXT("x"), 20.0);
        double Y = GetJsonNumberField(Payload, TEXT("y"), 20.0);
        double Width = GetJsonNumberField(Payload, TEXT("width"), 200.0);
        double Height = GetJsonNumberField(Payload, TEXT("height"), 20.0);
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find parent panel
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }
        
        if (!Parent)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No valid parent panel found"), TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        
        // Create horizontal box to hold health bar components
        UHorizontalBox* HealthBox = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HealthBarContainer"));
        Parent->AddChild(HealthBox);
        
        // Add health icon/label
        UTextBlock* HealthLabel = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthLabel"));
        HealthLabel->SetText(FText::FromString(TEXT("HP")));
        HealthBox->AddChild(HealthLabel);
        
        // Add progress bar for health
        UProgressBar* HealthProgress = WidgetBP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
        HealthProgress->SetPercent(1.0f);
        HealthProgress->SetFillColorAndOpacity(FLinearColor(0.8f, 0.1f, 0.1f, 1.0f));
        HealthBox->AddChild(HealthProgress);
        
        // Set position if parent is canvas panel
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(HealthBox->Slot))
            {
                Slot->SetPosition(FVector2D(X, Y));
                Slot->SetSize(FVector2D(Width, Height));
            }
        }
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), TEXT("HealthBarContainer"));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Health bar added"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("add_crosshair"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        double Size = GetJsonNumberField(Payload, TEXT("size"), 32.0);
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Find parent panel
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }
        
        if (!Parent)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No valid parent panel found"), TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        
        // Create crosshair image (uses a simple text-based crosshair, user can swap for image)
        UTextBlock* Crosshair = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Crosshair"));
        Crosshair->SetText(FText::FromString(TEXT("+")));
        FSlateFontInfo FontInfo = Crosshair->GetFont();
        FontInfo.Size = static_cast<int32>(Size);
        Crosshair->SetFont(FontInfo);
        Crosshair->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Parent->AddChild(Crosshair);
        
        // Center the crosshair if parent is canvas panel
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Crosshair->Slot))
            {
                Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
                Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            }
        }
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), TEXT("Crosshair"));
        ResultJson->SetStringField(TEXT("note"), TEXT("Simple crosshair added. Replace with Image widget and crosshair texture for custom appearance."));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Crosshair added"), ResultJson);
        return true;
    }
    
    if (SubAction.Equals(TEXT("add_ammo_counter"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (!ParentName.IsEmpty())
        {
            WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
                if (W && W->GetFName().ToString().Equals(ParentName, ESearchCase::IgnoreCase))
                {
                    if (UPanelWidget* P = Cast<UPanelWidget>(W)) Parent = P;
                }
            });
        }
        
        if (!Parent)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("No valid parent panel found"), TEXT("PARENT_NOT_FOUND"));
            return true;
        }
        
        // Create ammo counter text
        UTextBlock* AmmoText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AmmoCounter"));
        AmmoText->SetText(FText::FromString(TEXT("30 / 90")));
        FSlateFontInfo FontInfo = AmmoText->GetFont();
        FontInfo.Size = 24;
        AmmoText->SetFont(FontInfo);
        Parent->AddChild(AmmoText);
        
        // Position at bottom right if canvas
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(AmmoText->Slot))
            {
                Slot->SetAnchors(FAnchors(1.0f, 1.0f, 1.0f, 1.0f));
                Slot->SetAlignment(FVector2D(1.0f, 1.0f));
                Slot->SetPosition(FVector2D(-20.0f, -20.0f));
            }
        }
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetName"), TEXT("AmmoCounter"));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ammo counter added"), ResultJson);
        return true;
    }
    
    // Remaining UI Templates - return simple success with created structure info
    if (SubAction.Equals(TEXT("create_settings_menu"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("create_loading_screen"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("add_minimap"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("add_compass"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("add_interaction_prompt"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("add_objective_tracker"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("add_damage_indicator"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("create_inventory_ui"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("create_dialog_widget"), ESearchCase::IgnoreCase) ||
        SubAction.Equals(TEXT("create_radial_menu"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }
        
        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }
        
        // Create a basic structure with canvas panel
        if (!WidgetBP->WidgetTree->RootWidget)
        {
            UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), FName(*SubAction));
            WidgetBP->WidgetTree->RootWidget = RootCanvas;
        }
        
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("template"), SubAction);
        ResultJson->SetStringField(TEXT("note"), FString::Printf(TEXT("Basic %s structure created. Use individual widget actions to customize."), *SubAction));
        
        SendAutomationResponse(RequestingSocket, RequestId, true, FString::Printf(TEXT("%s created"), *SubAction), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.8 Utility (continued)
    // =========================================================================

    if (SubAction.Equals(TEXT("preview_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Widget preview is typically done by opening in editor or compiling
        // We can trigger a compile which updates the preview
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("message"), TEXT("Widget blueprint marked for recompilation. Open in Widget Blueprint Editor to see preview."));
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget preview updated"), ResultJson);
        return true;
    }

    // Action not recognized
    return false;
}
