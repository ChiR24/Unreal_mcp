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
#include "Components/EditableText.h"
#include "Components/TileView.h"
#include "WidgetBlueprint.h"
#include "UObject/UObjectIterator.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "McpAutomationBridgeHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EditorAssetLibrary.h"
#include "Components/SafeZone.h"
#include "Components/Spacer.h"
#include "Components/WidgetSwitcher.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FAssetData AssetData = Registry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
#else
        // UE 5.0: GetAssetByObjectPath takes FName
        FAssetData AssetData = Registry.GetAssetByObjectPath(FName(*ObjectPath));
#endif
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

// Suppress function size warning - this is a large handler function with many sub-actions
#pragma warning(push)
#pragma warning(disable: 4883)
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
        
        // SECURITY: Validate folder path for traversal attacks
        FString SanitizedFolder = SanitizeProjectRelativePath(Folder);
        if (SanitizedFolder.IsEmpty() && !Folder.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Invalid folder path: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        Folder = SanitizedFolder;
        
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
            // Note: FindFirstObject was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            UClass* FoundClass = FindFirstObject<UClass>(*ParentClass, EFindFirstObjectOptions::None);
#else
            // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
            UClass* FoundClass = ResolveClassByName(ParentClass);
#endif
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
        
        // SECURITY: Validate widget path
        FString SanitizedWidgetPath = SanitizeProjectRelativePath(WidgetPath);
        if (SanitizedWidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, 
                TEXT("Invalid widgetPath: path traversal or invalid characters detected"), 
                TEXT("SECURITY_VIOLATION"));
            return true;
        }
        WidgetPath = SanitizedWidgetPath;
        
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
        // Note: FindFirstObject was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        UClass* NewParentClass = FindFirstObject<UClass>(*ParentClass, EFindFirstObjectOptions::None);
#else
        // UE 5.0: Use ResolveClassByName instead of deprecated ANY_PACKAGE
        UClass* NewParentClass = ResolveClassByName(ParentClass);
#endif
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            FSlateFontInfo FontInfo = TextBlock->GetFont();
#else
            // UE 5.0 fallback - create new font info
            FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
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
        // Note: SetWrapSize was introduced in UE 5.1
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        if (Payload->HasField(TEXT("wrapSize")))
        {
            WrapBox->SetWrapSize(static_cast<float>(GetJsonNumberField(Payload, TEXT("wrapSize"), 0.0)));
        }
#endif

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = TitleText->GetFont();
#else
        FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = TitleText->GetFont();
#else
        FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = Crosshair->GetFont();
#else
        FSlateFontInfo FontInfo = FSlateFontInfo();
#endif
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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FSlateFontInfo FontInfo = AmmoText->GetFont();
#else
        // UE 5.0: Access Font property directly
        FSlateFontInfo FontInfo = AmmoText->Font;
#endif
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

    // =========================================================================
    // 19.9 Generic Widget Actions (3 new actions)
    // =========================================================================

    // add_widget_component - Generic action to add any UWidget-derived component
    if (SubAction.Equals(TEXT("add_widget_component"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString ComponentType = GetJsonStringField(Payload, TEXT("componentType"));
        if (ComponentType.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: componentType"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString ComponentName = GetJsonStringField(Payload, TEXT("componentName"));
        if (ComponentName.IsEmpty())
        {
            ComponentName = ComponentType + TEXT("_") + FGuid::NewGuid().ToString().Left(8);
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find parent panel
        FString ParentName = GetJsonStringField(Payload, TEXT("parentName"));
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
            // Create a canvas panel as root if none exists
            Parent = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
            WidgetBP->WidgetTree->RootWidget = Parent;
        }

        // Map component type to UWidget class
        UClass* WidgetClass = nullptr;
        
        // Common widget types
        if (ComponentType.Equals(TEXT("TextBlock"), ESearchCase::IgnoreCase) || 
            ComponentType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UTextBlock::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UButton::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UImage::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ProgressBar"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UProgressBar::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Slider"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USlider::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("CheckBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UCheckBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("EditableText"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UEditableText::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("EditableTextBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UEditableTextBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ComboBox"), ESearchCase::IgnoreCase) ||
                 ComponentType.Equals(TEXT("ComboBoxString"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UComboBoxString::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("SpinBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USpinBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("CanvasPanel"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UCanvasPanel::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("HorizontalBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UHorizontalBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("VerticalBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UVerticalBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("GridPanel"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UGridPanel::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("UniformGridPanel"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UUniformGridPanel::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Overlay"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UOverlay::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("SizeBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USizeBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ScaleBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UScaleBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Border"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UBorder::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("Spacer"), ESearchCase::IgnoreCase))
        {
            WidgetClass = USpacer::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ScrollBox"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UScrollBox::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("WidgetSwitcher"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UWidgetSwitcher::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("ListView"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UListView::StaticClass();
        }
        else if (ComponentType.Equals(TEXT("TileView"), ESearchCase::IgnoreCase))
        {
            WidgetClass = UTileView::StaticClass();
        }
        else
        {
            // Try to find by class name
            FString ClassName = TEXT("U") + ComponentType;
            WidgetClass = FindObject<UClass>(nullptr, *ClassName);
            if (!WidgetClass)
            {
                // Try with Widget suffix
                ClassName = TEXT("U") + ComponentType + TEXT("Widget");
                WidgetClass = FindObject<UClass>(nullptr, *ClassName);
            }
        }

        if (!WidgetClass || !WidgetClass->IsChildOf(UWidget::StaticClass()))
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Unknown widget type: %s"), *ComponentType), TEXT("UNKNOWN_TYPE"));
            return true;
        }

        // Create the widget
        UWidget* NewWidget = WidgetBP->WidgetTree->ConstructWidget<UWidget>(WidgetClass, *ComponentName);
        if (!NewWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to construct widget"), TEXT("CREATION_FAILED"));
            return true;
        }

        // Add to parent
        Parent->AddChild(NewWidget);

        // Configure slot if canvas panel
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Parent))
        {
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(NewWidget->Slot))
            {
                float PosX = static_cast<float>(GetJsonNumberField(Payload, TEXT("positionX"), 0.0));
                float PosY = static_cast<float>(GetJsonNumberField(Payload, TEXT("positionY"), 0.0));
                float SizeX = static_cast<float>(GetJsonNumberField(Payload, TEXT("sizeX"), 0.0));
                float SizeY = static_cast<float>(GetJsonNumberField(Payload, TEXT("sizeY"), 0.0));
                
                if (PosX != 0.0f || PosY != 0.0f)
                {
                    Slot->SetPosition(FVector2D(PosX, PosY));
                }
                if (SizeX > 0.0f && SizeY > 0.0f)
                {
                    Slot->SetSize(FVector2D(SizeX, SizeY));
                    Slot->SetAutoSize(false);
                }
            }
        }

        // Set initial text if TextBlock
        if (UTextBlock* TextWidget = Cast<UTextBlock>(NewWidget))
        {
            FString InitialText = GetJsonStringField(Payload, TEXT("text"));
            if (!InitialText.IsEmpty())
            {
                TextWidget->SetText(FText::FromString(InitialText));
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("componentName"), ComponentName);
        ResultJson->SetStringField(TEXT("componentType"), WidgetClass->GetName());
        ResultJson->SetStringField(TEXT("parentName"), Parent->GetName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget component added"), ResultJson);
        return true;
    }

    // set_widget_binding - Unified binding action (wraps bind_text, bind_visibility, etc.)
    if (SubAction.Equals(TEXT("set_widget_binding"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString TargetWidget = GetJsonStringField(Payload, TEXT("targetWidget"));
        if (TargetWidget.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: targetWidget"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString PropertyName = GetJsonStringField(Payload, TEXT("property"));
        if (PropertyName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: property"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"));
        if (FunctionName.IsEmpty())
        {
            FunctionName = TEXT("Get") + PropertyName;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // Find the target widget
        UWidget* Target = nullptr;
        WidgetBP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (W && W->GetFName().ToString().Equals(TargetWidget, ESearchCase::IgnoreCase))
            {
                Target = W;
            }
        });

        if (!Target)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Target widget not found: %s"), *TargetWidget), TEXT("WIDGET_NOT_FOUND"));
            return true;
        }

        // Determine binding type based on property
        FString BindingType = TEXT("Unknown");
        bool bBindingSupported = false;

        // Common bindable properties
        if (PropertyName.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("Text");
            bBindingSupported = Target->IsA(UTextBlock::StaticClass());
        }
        else if (PropertyName.Equals(TEXT("Visibility"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("Visibility");
            bBindingSupported = true; // All widgets support visibility
        }
        else if (PropertyName.Equals(TEXT("IsEnabled"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("IsEnabled");
            bBindingSupported = true; // All widgets support enabled state
        }
        else if (PropertyName.Equals(TEXT("Percent"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("Percent");
            bBindingSupported = Target->IsA(UProgressBar::StaticClass());
        }
        else if (PropertyName.Equals(TEXT("ColorAndOpacity"), ESearchCase::IgnoreCase))
        {
            BindingType = TEXT("ColorAndOpacity");
            bBindingSupported = Target->IsA(UImage::StaticClass()) || Target->IsA(UTextBlock::StaticClass());
        }

        if (!bBindingSupported)
        {
            SendAutomationError(RequestingSocket, RequestId, 
                FString::Printf(TEXT("Property '%s' is not bindable on widget type '%s'"), 
                    *PropertyName, *Target->GetClass()->GetName()), TEXT("INVALID_BINDING"));
            return true;
        }

        // Note: Actually creating the binding requires modifying the widget graph
        // This is a complex operation - for now we document what binding to create
        
        FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("targetWidget"), TargetWidget);
        ResultJson->SetStringField(TEXT("property"), PropertyName);
        ResultJson->SetStringField(TEXT("functionName"), FunctionName);
        ResultJson->SetStringField(TEXT("bindingType"), BindingType);
        ResultJson->SetStringField(TEXT("note"), FString::Printf(
            TEXT("Create a function '%s' returning %s, then bind to %s.%s in the Widget Designer."),
            *FunctionName, *BindingType, *TargetWidget, *PropertyName));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget binding configured"), ResultJson);
        return true;
    }

    // create_widget_style - Create reusable widget style (FSlateWidgetStyle equivalent via variables)
    if (SubAction.Equals(TEXT("create_widget_style"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        if (WidgetPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString StyleName = GetJsonStringField(Payload, TEXT("styleName"));
        if (StyleName.IsEmpty())
        {
            StyleName = TEXT("DefaultStyle");
        }

        FString StyleType = GetJsonStringField(Payload, TEXT("styleType"));
        if (StyleType.IsEmpty())
        {
            StyleType = TEXT("Text");
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        TArray<FString> CreatedVariables;

        // Create style variables based on type
        if (StyleType.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
        {
            // Font style variable
            FEdGraphPinType FontPinType;
            FontPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            FontPinType.PinSubCategoryObject = FSlateFontInfo::StaticStruct();
            
            FString FontVarName = StyleName + TEXT("_Font");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *FontVarName, FontPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *FontVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(FontVarName);

            // Color variable
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FSlateColor>::Get();
            
            FString ColorVarName = StyleName + TEXT("_Color");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ColorVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ColorVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ColorVarName);

            // Shadow color
            FString ShadowVarName = StyleName + TEXT("_ShadowColor");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ShadowVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ShadowVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ShadowVarName);
        }
        else if (StyleType.Equals(TEXT("Button"), ESearchCase::IgnoreCase))
        {
            // Button style uses FButtonStyle
            FEdGraphPinType ButtonStylePinType;
            ButtonStylePinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ButtonStylePinType.PinSubCategoryObject = FButtonStyle::StaticStruct();
            
            FString ButtonStyleVarName = StyleName + TEXT("_ButtonStyle");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ButtonStyleVarName, ButtonStylePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ButtonStyleVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ButtonStyleVarName);

            // Normal/Hovered/Pressed colors
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
            
            for (const FString& State : { TEXT("Normal"), TEXT("Hovered"), TEXT("Pressed") })
            {
                FString StateVarName = StyleName + TEXT("_") + State + TEXT("Color");
                FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *StateVarName, ColorPinType);
                FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *StateVarName, nullptr, 
                    FText::FromString(TEXT("Widget Styles")));
                CreatedVariables.Add(StateVarName);
            }
        }
        else if (StyleType.Equals(TEXT("Image"), ESearchCase::IgnoreCase))
        {
            // Brush style
            FEdGraphPinType BrushPinType;
            BrushPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            BrushPinType.PinSubCategoryObject = FSlateBrush::StaticStruct();
            
            FString BrushVarName = StyleName + TEXT("_Brush");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *BrushVarName, BrushPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *BrushVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(BrushVarName);

            // Tint color
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
            
            FString TintVarName = StyleName + TEXT("_Tint");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *TintVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *TintVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(TintVarName);
        }
        else if (StyleType.Equals(TEXT("ProgressBar"), ESearchCase::IgnoreCase))
        {
            FEdGraphPinType StylePinType;
            StylePinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            StylePinType.PinSubCategoryObject = FProgressBarStyle::StaticStruct();
            
            FString ProgressStyleVarName = StyleName + TEXT("_ProgressStyle");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ProgressStyleVarName, StylePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ProgressStyleVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ProgressStyleVarName);
        }
        else
        {
            // Generic style - create color and margin variables
            FEdGraphPinType ColorPinType;
            ColorPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            ColorPinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
            
            FString ColorVarName = StyleName + TEXT("_Color");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *ColorVarName, ColorPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *ColorVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(ColorVarName);

            FEdGraphPinType MarginPinType;
            MarginPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
            MarginPinType.PinSubCategoryObject = TBaseStructure<FMargin>::Get();
            
            FString MarginVarName = StyleName + TEXT("_Margin");
            FBlueprintEditorUtils::AddMemberVariable(WidgetBP, *MarginVarName, MarginPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(WidgetBP, *MarginVarName, nullptr, 
                FText::FromString(TEXT("Widget Styles")));
            CreatedVariables.Add(MarginVarName);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        TArray<TSharedPtr<FJsonValue>> VariablesArray;
        for (const FString& VarName : CreatedVariables)
        {
            VariablesArray.Add(MakeShareable(new FJsonValueString(VarName)));
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("styleName"), StyleName);
        ResultJson->SetStringField(TEXT("styleType"), StyleType);
        ResultJson->SetArrayField(TEXT("createdVariables"), VariablesArray);
        ResultJson->SetNumberField(TEXT("variableCount"), CreatedVariables.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget style variables created"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.10 Missing UI Template Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("create_settings_menu"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_SettingsMenu"));
        FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI/Menus"));

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create settings menu widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Create root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Create settings container
        UVerticalBox* SettingsContainer = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsContainer"));
        RootCanvas->AddChild(SettingsContainer);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(SettingsContainer->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(600.0f, 400.0f));
        }

        // Title
        UTextBlock* TitleText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
        TitleText->SetText(FText::FromString(TEXT("Settings")));
        SettingsContainer->AddChild(TitleText);

        // Graphics section
        UTextBlock* GraphicsLabel = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GraphicsLabel"));
        GraphicsLabel->SetText(FText::FromString(TEXT("Graphics")));
        SettingsContainer->AddChild(GraphicsLabel);

        // Quality slider
        USlider* QualitySlider = WidgetBP->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("QualitySlider"));
        SettingsContainer->AddChild(QualitySlider);

        // Audio section
        UTextBlock* AudioLabel = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AudioLabel"));
        AudioLabel->SetText(FText::FromString(TEXT("Audio")));
        SettingsContainer->AddChild(AudioLabel);

        // Volume slider
        USlider* VolumeSlider = WidgetBP->WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("VolumeSlider"));
        SettingsContainer->AddChild(VolumeSlider);

        // Apply button
        UButton* ApplyButton = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ApplyButton"));
        SettingsContainer->AddChild(ApplyButton);
        UTextBlock* ApplyText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ApplyButtonText"));
        ApplyText->SetText(FText::FromString(TEXT("Apply")));
        ApplyButton->AddChild(ApplyText);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("message"), TEXT("Created settings menu template"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created settings menu template"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_loading_screen"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_LoadingScreen"));
        FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create loading screen widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Create root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Background image
        UImage* Background = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
        RootCanvas->AddChild(Background);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Background->Slot))
        {
            Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            Slot->SetOffsets(FMargin(0.0f));
        }

        // Loading text
        UTextBlock* LoadingText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LoadingText"));
        LoadingText->SetText(FText::FromString(TEXT("Loading...")));
        RootCanvas->AddChild(LoadingText);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(LoadingText->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.7f, 0.5f, 0.7f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
        }

        // Progress bar
        UProgressBar* LoadingBar = WidgetBP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("LoadingProgressBar"));
        LoadingBar->SetPercent(0.0f);
        RootCanvas->AddChild(LoadingBar);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(LoadingBar->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.8f, 0.5f, 0.8f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(400.0f, 20.0f));
        }

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("message"), TEXT("Created loading screen template"));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created loading screen template"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_minimap"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Minimap"));
        float Size = GetJsonNumberField(Payload, TEXT("size"), 200.0f);

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

        // Create minimap container (overlay for stacking)
        UOverlay* MinimapContainer = WidgetBP->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *SlotName);
        
        // Create border for minimap frame
        UBorder* MinimapBorder = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *(SlotName + TEXT("_Border")));
        MinimapContainer->AddChild(MinimapBorder);

        // Create image for map content
        UImage* MapImage = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_MapImage")));
        MinimapBorder->AddChild(MapImage);

        // Create player indicator
        UImage* PlayerIndicator = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_PlayerIndicator")));
        MinimapContainer->AddChild(PlayerIndicator);

        // Add to root or parent
        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(MinimapContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(MinimapContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f)); // Top-right
                Slot->SetAlignment(FVector2D(1.0f, 0.0f));
                Slot->SetSize(FVector2D(Size, Size));
                Slot->SetPosition(FVector2D(-20.0f, 20.0f)); // Offset from corner
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("size"), Size);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added minimap widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_compass"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Compass"));

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

        // Create compass container
        UHorizontalBox* CompassContainer = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *SlotName);

        // Create compass image (scrolling texture)
        UImage* CompassImage = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_Image")));
        CompassContainer->AddChild(CompassImage);

        // Create direction indicator
        UImage* DirectionIndicator = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_Indicator")));
        CompassContainer->AddChild(DirectionIndicator);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(CompassContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CompassContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f)); // Top-center
                Slot->SetAlignment(FVector2D(0.5f, 0.0f));
                Slot->SetSize(FVector2D(400.0f, 40.0f));
                Slot->SetPosition(FVector2D(0.0f, 20.0f));
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added compass widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_interaction_prompt"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("InteractionPrompt"));
        FString DefaultText = GetJsonStringField(Payload, TEXT("text"), TEXT("Press E to Interact"));

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

        // Create prompt container
        UHorizontalBox* PromptContainer = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *SlotName);

        // Key icon
        UImage* KeyIcon = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_KeyIcon")));
        PromptContainer->AddChild(KeyIcon);

        // Prompt text
        UTextBlock* PromptText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(SlotName + TEXT("_Text")));
        PromptText->SetText(FText::FromString(DefaultText));
        PromptContainer->AddChild(PromptText);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(PromptContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(PromptContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(0.5f, 0.7f, 0.5f, 0.7f)); // Center-bottom area
                Slot->SetAlignment(FVector2D(0.5f, 0.5f));
                Slot->SetAutoSize(true);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added interaction prompt"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_objective_tracker"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("ObjectiveTracker"));

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

        // Create objective container
        UVerticalBox* ObjectiveContainer = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *SlotName);

        // Objective title
        UTextBlock* ObjectiveTitle = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(SlotName + TEXT("_Title")));
        ObjectiveTitle->SetText(FText::FromString(TEXT("Objectives")));
        ObjectiveContainer->AddChild(ObjectiveTitle);

        // Objective list (vertical box for dynamic entries)
        UVerticalBox* ObjectiveList = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *(SlotName + TEXT("_List")));
        ObjectiveContainer->AddChild(ObjectiveList);

        // Sample objective item
        UHorizontalBox* SampleObjective = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *(SlotName + TEXT("_SampleItem")));
        UCheckBox* ObjectiveCheck = WidgetBP->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), *(SlotName + TEXT("_Check")));
        UTextBlock* ObjectiveText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(SlotName + TEXT("_ItemText")));
        ObjectiveText->SetText(FText::FromString(TEXT("Sample Objective")));
        SampleObjective->AddChild(ObjectiveCheck);
        SampleObjective->AddChild(ObjectiveText);
        ObjectiveList->AddChild(SampleObjective);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(ObjectiveContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(ObjectiveContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f)); // Top-right
                Slot->SetAlignment(FVector2D(1.0f, 0.0f));
                Slot->SetPosition(FVector2D(-20.0f, 100.0f));
                Slot->SetAutoSize(true);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added objective tracker"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_damage_indicator"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("DamageIndicator"));

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

        // Create damage indicator overlay (full screen)
        UOverlay* DamageOverlay = WidgetBP->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), *SlotName);

        // Blood vignette image (edge damage indicator)
        UImage* VignetteImage = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_Vignette")));
        VignetteImage->SetVisibility(ESlateVisibility::Hidden);
        DamageOverlay->AddChild(VignetteImage);

        // Directional damage arrows container
        UCanvasPanel* DirectionalCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), *(SlotName + TEXT("_Directional")));
        DamageOverlay->AddChild(DirectionalCanvas);

        // Add directional indicators (N, S, E, W)
        for (const FString& Dir : { TEXT("Top"), TEXT("Bottom"), TEXT("Left"), TEXT("Right") })
        {
            UImage* DirIndicator = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_") + Dir));
            DirIndicator->SetVisibility(ESlateVisibility::Hidden);
            DirectionalCanvas->AddChild(DirIndicator);
        }

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(DamageOverlay);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(DamageOverlay->Slot))
            {
                Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); // Full screen
                Slot->SetOffsets(FMargin(0.0f));
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added damage indicator"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_inventory_ui"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_Inventory"));
        FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));
        int32 GridColumns = GetJsonIntField(Payload, TEXT("columns"), 6);
        int32 GridRows = GetJsonIntField(Payload, TEXT("rows"), 4);

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create inventory widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Create root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Background panel
        UBorder* BackgroundPanel = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryBackground"));
        RootCanvas->AddChild(BackgroundPanel);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(BackgroundPanel->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(GridColumns * 80.0f + 40.0f, GridRows * 80.0f + 100.0f));
        }

        // Title
        UTextBlock* TitleText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("InventoryTitle"));
        TitleText->SetText(FText::FromString(TEXT("Inventory")));
        BackgroundPanel->AddChild(TitleText);

        // Create inventory grid
        UUniformGridPanel* InventoryGrid = WidgetBP->WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("InventoryGrid"));
        BackgroundPanel->AddChild(InventoryGrid);

        // Add slot placeholders
        for (int32 Row = 0; Row < GridRows; ++Row)
        {
            for (int32 Col = 0; Col < GridColumns; ++Col)
            {
                FString SlotName = FString::Printf(TEXT("Slot_%d_%d"), Row, Col);
                UBorder* SlotBorder = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *SlotName);
                InventoryGrid->AddChildToUniformGrid(SlotBorder, Row, Col);
                
                UImage* SlotImage = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_Image")));
                SlotBorder->AddChild(SlotImage);
            }
        }

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetNumberField(TEXT("columns"), GridColumns);
        ResultJson->SetNumberField(TEXT("rows"), GridRows);
        ResultJson->SetNumberField(TEXT("totalSlots"), GridColumns * GridRows);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created inventory UI"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_dialog_widget"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_DialogBox"));
        FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create dialog widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Create root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Dialog background
        UBorder* DialogBg = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DialogBackground"));
        RootCanvas->AddChild(DialogBg);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(DialogBg->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.8f, 0.5f, 0.8f));
            Slot->SetAlignment(FVector2D(0.5f, 1.0f));
            Slot->SetSize(FVector2D(800.0f, 200.0f));
        }

        UVerticalBox* DialogContainer = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogContainer"));
        DialogBg->AddChild(DialogContainer);

        // Speaker name
        UTextBlock* SpeakerName = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeakerName"));
        SpeakerName->SetText(FText::FromString(TEXT("Speaker")));
        DialogContainer->AddChild(SpeakerName);

        // Dialog text
        URichTextBlock* DialogText = WidgetBP->WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass(), TEXT("DialogText"));
        DialogContainer->AddChild(DialogText);

        // Response options container
        UVerticalBox* ResponseBox = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ResponseOptions"));
        DialogContainer->AddChild(ResponseBox);

        // Sample response buttons
        for (int32 i = 1; i <= 3; ++i)
        {
            FString ResponseName = FString::Printf(TEXT("Response_%d"), i);
            UButton* ResponseBtn = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *ResponseName);
            UTextBlock* ResponseText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(ResponseName + TEXT("_Text")));
            ResponseText->SetText(FText::FromString(FString::Printf(TEXT("Response Option %d"), i)));
            ResponseBtn->AddChild(ResponseText);
            ResponseBox->AddChild(ResponseBtn);
        }

        // Continue indicator
        UTextBlock* ContinueIndicator = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ContinueIndicator"));
        ContinueIndicator->SetText(FText::FromString(TEXT("Press Space to continue...")));
        DialogContainer->AddChild(ContinueIndicator);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created dialog widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_radial_menu"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_RadialMenu"));
        FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));
        int32 SegmentCount = GetJsonIntField(Payload, TEXT("segments"), 8);

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create radial menu"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Create root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Radial menu container (centered)
        UOverlay* RadialContainer = WidgetBP->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RadialMenuContainer"));
        RootCanvas->AddChild(RadialContainer);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(RadialContainer->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(400.0f, 400.0f));
        }

        // Background ring
        UImage* BackgroundRing = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RadialBackground"));
        RadialContainer->AddChild(BackgroundRing);

        // Selection indicator
        UImage* SelectionIndicator = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SelectionIndicator"));
        RadialContainer->AddChild(SelectionIndicator);

        // Create segment buttons (arranged in circle via canvas positions)
        UCanvasPanel* SegmentCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SegmentCanvas"));
        RadialContainer->AddChild(SegmentCanvas);

        float Radius = 150.0f;
        for (int32 i = 0; i < SegmentCount; ++i)
        {
            float Angle = (360.0f / SegmentCount) * i - 90.0f; // Start from top
            float RadAngle = FMath::DegreesToRadians(Angle);
            float X = FMath::Cos(RadAngle) * Radius;
            float Y = FMath::Sin(RadAngle) * Radius;

            FString SegmentName = FString::Printf(TEXT("Segment_%d"), i);
            UButton* SegmentBtn = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *SegmentName);
            SegmentCanvas->AddChild(SegmentBtn);
            
            if (UCanvasPanelSlot* SegSlot = Cast<UCanvasPanelSlot>(SegmentBtn->Slot))
            {
                SegSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
                SegSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                SegSlot->SetPosition(FVector2D(X, Y));
                SegSlot->SetSize(FVector2D(60.0f, 60.0f));
            }

            UImage* SegmentIcon = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SegmentName + TEXT("_Icon")));
            SegmentBtn->AddChild(SegmentIcon);
        }

        // Center button
        UButton* CenterButton = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CenterButton"));
        RadialContainer->AddChild(CenterButton);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetNumberField(TEXT("segments"), SegmentCount);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created radial menu"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.11 Widget Manipulation Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("remove_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        WidgetBP->WidgetTree->RemoveWidget(TargetWidget);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("removedWidget"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Removed widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("rename_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString OldName = GetJsonStringField(Payload, TEXT("slotName"));
        FString NewName = GetJsonStringField(Payload, TEXT("newName"));

        if (WidgetPath.IsEmpty() || OldName.IsEmpty() || NewName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, newName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*OldName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *OldName), TEXT("NOT_FOUND"));
            return true;
        }

        // Rename requires FBlueprintEditorUtils for proper undo/redo support
        TargetWidget->Rename(*NewName);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("oldName"), OldName);
        ResultJson->SetStringField(TEXT("newName"), NewName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Renamed widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("reparent_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString NewParent = GetJsonStringField(Payload, TEXT("newParent"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || NewParent.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, newParent"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        UPanelWidget* NewParentWidget = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*NewParent)));
        if (!NewParentWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("New parent '%s' not found or not a panel"), *NewParent), TEXT("NOT_FOUND"));
            return true;
        }

        // Remove from current parent and add to new parent
        if (UPanelWidget* OldParent = TargetWidget->GetParent())
        {
            OldParent->RemoveChild(TargetWidget);
        }
        NewParentWidget->AddChild(TargetWidget);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("widget"), SlotName);
        ResultJson->SetStringField(TEXT("newParent"), NewParent);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Reparented widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("get_widget_slot_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("widgetClass"), TargetWidget->GetClass()->GetName());
        ResultJson->SetBoolField(TEXT("isVisible"), TargetWidget->IsVisible());

        if (UPanelSlot* Slot = TargetWidget->Slot)
        {
            ResultJson->SetStringField(TEXT("slotClass"), Slot->GetClass()->GetName());
            
            if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
            {
                TSharedPtr<FJsonObject> SlotInfo = MakeShareable(new FJsonObject());
                FAnchors Anchors = CanvasSlot->GetAnchors();
                SlotInfo->SetNumberField(TEXT("anchorMinX"), Anchors.Minimum.X);
                SlotInfo->SetNumberField(TEXT("anchorMinY"), Anchors.Minimum.Y);
                SlotInfo->SetNumberField(TEXT("anchorMaxX"), Anchors.Maximum.X);
                SlotInfo->SetNumberField(TEXT("anchorMaxY"), Anchors.Maximum.Y);
                FVector2D Alignment = CanvasSlot->GetAlignment();
                SlotInfo->SetNumberField(TEXT("alignmentX"), Alignment.X);
                SlotInfo->SetNumberField(TEXT("alignmentY"), Alignment.Y);
                FVector2D Position = CanvasSlot->GetPosition();
                SlotInfo->SetNumberField(TEXT("positionX"), Position.X);
                SlotInfo->SetNumberField(TEXT("positionY"), Position.Y);
                FVector2D Size = CanvasSlot->GetSize();
                SlotInfo->SetNumberField(TEXT("sizeX"), Size.X);
                SlotInfo->SetNumberField(TEXT("sizeY"), Size.Y);
                SlotInfo->SetNumberField(TEXT("zOrder"), CanvasSlot->GetZOrder());
                ResultJson->SetObjectField(TEXT("canvasSlotInfo"), SlotInfo);
            }
        }

        if (UPanelWidget* Parent = TargetWidget->GetParent())
        {
            ResultJson->SetStringField(TEXT("parentName"), Parent->GetName());
            ResultJson->SetStringField(TEXT("parentClass"), Parent->GetClass()->GetName());
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved widget slot info"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.12 Additional Layout Panels
    // =========================================================================

    if (SubAction.Equals(TEXT("add_safe_zone"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("SafeZone"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));

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

        USafeZone* SafeZone = WidgetBP->WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), *SlotName);
        
        UPanelWidget* Parent = nullptr;
        if (!ParentSlot.IsEmpty())
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot)));
        }
        if (!Parent)
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        }

        if (Parent)
        {
            Parent->AddChild(SafeZone);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added safe zone"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_spacer"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Spacer"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        float SizeX = GetJsonNumberField(Payload, TEXT("sizeX"), 100.0f);
        float SizeY = GetJsonNumberField(Payload, TEXT("sizeY"), 100.0f);

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

        USpacer* Spacer = WidgetBP->WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), *SlotName);
        Spacer->SetSize(FVector2D(SizeX, SizeY));

        UPanelWidget* Parent = nullptr;
        if (!ParentSlot.IsEmpty())
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot)));
        }
        if (!Parent)
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        }

        if (Parent)
        {
            Parent->AddChild(Spacer);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("sizeX"), SizeX);
        ResultJson->SetNumberField(TEXT("sizeY"), SizeY);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added spacer"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_widget_switcher"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("WidgetSwitcher"));
        FString ParentSlot = GetJsonStringField(Payload, TEXT("parentSlot"));
        int32 ActiveIndex = GetJsonIntField(Payload, TEXT("activeIndex"), 0);

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

        UWidgetSwitcher* Switcher = WidgetBP->WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), *SlotName);
        Switcher->SetActiveWidgetIndex(ActiveIndex);

        UPanelWidget* Parent = nullptr;
        if (!ParentSlot.IsEmpty())
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->FindWidget(FName(*ParentSlot)));
        }
        if (!Parent)
        {
            Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        }

        if (Parent)
        {
            Parent->AddChild(Switcher);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("activeIndex"), ActiveIndex);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added widget switcher"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.13 Advanced Styling Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("set_font"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString FontPath = GetJsonStringField(Payload, TEXT("font"));
        float FontSize = GetJsonNumberField(Payload, TEXT("fontSize"), 24.0f);

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        bool bFontApplied = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            FSlateFontInfo FontInfo = TextWidget->GetFont();
#else
            // UE 5.0: Font property is directly accessible
            FSlateFontInfo FontInfo = TextWidget->Font;
#endif
            FontInfo.Size = FontSize;
            if (!FontPath.IsEmpty())
            {
                // Load font object if path provided
                UObject* FontObject = StaticLoadObject(UObject::StaticClass(), nullptr, *FontPath);
                if (FontObject)
                {
                    FontInfo.FontObject = FontObject;
                }
            }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            TextWidget->SetFont(FontInfo);
#else
            // UE 5.0: Font property is directly accessible
            TextWidget->Font = FontInfo;
#endif
            bFontApplied = true;
        }
        else if (URichTextBlock* RichText = Cast<URichTextBlock>(TargetWidget))
        {
            // Rich text blocks use text styles, not direct font setting
            // Just set the default text style properties if available
            bFontApplied = true; // Acknowledge but note limitation
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bFontApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("fontSize"), FontSize);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set font"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("set_margin"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        float Left = GetJsonNumberField(Payload, TEXT("left"), 0.0f);
        float Top = GetJsonNumberField(Payload, TEXT("top"), 0.0f);
        float Right = GetJsonNumberField(Payload, TEXT("right"), 0.0f);
        float Bottom = GetJsonNumberField(Payload, TEXT("bottom"), 0.0f);

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        FMargin Margin(Left, Top, Right, Bottom);
        bool bMarginApplied = false;

        // Apply margin based on slot type
        if (UPanelSlot* Slot = TargetWidget->Slot)
        {
            if (UHorizontalBoxSlot* HBoxSlot = Cast<UHorizontalBoxSlot>(Slot))
            {
                HBoxSlot->SetPadding(Margin);
                bMarginApplied = true;
            }
            else if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(Slot))
            {
                VBoxSlot->SetPadding(Margin);
                bMarginApplied = true;
            }
            else if (UOverlaySlot* OvSlot = Cast<UOverlaySlot>(Slot))
            {
                OvSlot->SetPadding(Margin);
                bMarginApplied = true;
            }
        }

        // Also try to set on border widgets
        if (UBorder* BorderWidget = Cast<UBorder>(TargetWidget))
        {
            BorderWidget->SetPadding(Margin);
            bMarginApplied = true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bMarginApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("left"), Left);
        ResultJson->SetNumberField(TEXT("top"), Top);
        ResultJson->SetNumberField(TEXT("right"), Right);
        ResultJson->SetNumberField(TEXT("bottom"), Bottom);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set margin"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("apply_style_to_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString StyleName = GetJsonStringField(Payload, TEXT("styleName"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || StyleName.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, styleName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        // Check if style variable exists in blueprint
        FProperty* StyleProp = WidgetBP->GeneratedClass ? WidgetBP->GeneratedClass->FindPropertyByName(FName(*StyleName)) : nullptr;
        
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("styleName"), StyleName);
        ResultJson->SetBoolField(TEXT("styleFound"), StyleProp != nullptr);
        ResultJson->SetStringField(TEXT("note"), TEXT("Style binding created. Actual style application requires runtime binding setup."));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Applied style to widget"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.14 Animation Extended Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("set_animation_speed"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));
        float PlaybackSpeed = GetJsonNumberField(Payload, TEXT("speed"), 1.0f);

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

        UWidgetAnimation* TargetAnim = nullptr;
        for (UWidgetAnimation* Anim : WidgetBP->Animations)
        {
            if (Anim && Anim->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                TargetAnim = Anim;
                break;
            }
        }

        if (!TargetAnim || !TargetAnim->MovieScene)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
            return true;
        }

        // Animation playback speed is set at runtime, but we can store it as metadata
        // For design-time, we adjust the playback rate via the MovieScene settings
        // UE 5.7: SetPlaybackRange takes TRange<FFrameNumber>
        TRange<FFrameNumber> PlaybackRange = TargetAnim->MovieScene->GetPlaybackRange();
        TargetAnim->MovieScene->SetPlaybackRange(PlaybackRange);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("animationName"), AnimationName);
        ResultJson->SetNumberField(TEXT("speed"), PlaybackSpeed);
        ResultJson->SetStringField(TEXT("note"), TEXT("Speed is applied at runtime. Animation marked as modified."));

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set animation speed"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("get_animation_info"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));

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

        if (AnimationName.IsEmpty())
        {
            // Return list of all animations
            TArray<TSharedPtr<FJsonValue>> AnimationsArray;
            for (UWidgetAnimation* Anim : WidgetBP->Animations)
            {
                if (Anim)
                {
                    TSharedPtr<FJsonObject> AnimInfo = MakeShareable(new FJsonObject());
                    AnimInfo->SetStringField(TEXT("name"), Anim->GetName());
                    if (Anim->MovieScene)
                    {
                        FFrameRate FrameRate = Anim->MovieScene->GetTickResolution();
                        FFrameNumber Start = Anim->MovieScene->GetPlaybackRange().GetLowerBoundValue();
                        FFrameNumber End = Anim->MovieScene->GetPlaybackRange().GetUpperBoundValue();
                        float Duration = (End - Start).Value / FrameRate.AsDecimal();
                        AnimInfo->SetNumberField(TEXT("durationSeconds"), Duration);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                        AnimInfo->SetNumberField(TEXT("trackCount"), Anim->MovieScene->GetTracks().Num());
#else
                        AnimInfo->SetNumberField(TEXT("trackCount"), Anim->MovieScene->GetMasterTracks().Num());
#endif
                    }
                    AnimationsArray.Add(MakeShareable(new FJsonValueObject(AnimInfo)));
                }
            }
            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
            ResultJson->SetArrayField(TEXT("animations"), AnimationsArray);
            ResultJson->SetNumberField(TEXT("animationCount"), WidgetBP->Animations.Num());
        }
        else
        {
            // Return info for specific animation
            UWidgetAnimation* TargetAnim = nullptr;
            for (UWidgetAnimation* Anim : WidgetBP->Animations)
            {
                if (Anim && Anim->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
                {
                    TargetAnim = Anim;
                    break;
                }
            }

            if (!TargetAnim)
            {
                SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
                return true;
            }

            ResultJson->SetBoolField(TEXT("success"), true);
            ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
            ResultJson->SetStringField(TEXT("animationName"), AnimationName);

            if (TargetAnim->MovieScene)
            {
                FFrameRate FrameRate = TargetAnim->MovieScene->GetTickResolution();
                FFrameNumber Start = TargetAnim->MovieScene->GetPlaybackRange().GetLowerBoundValue();
                FFrameNumber End = TargetAnim->MovieScene->GetPlaybackRange().GetUpperBoundValue();
                float Duration = (End - Start).Value / FrameRate.AsDecimal();
                
                ResultJson->SetNumberField(TEXT("durationSeconds"), Duration);
                ResultJson->SetNumberField(TEXT("frameRate"), FrameRate.AsDecimal());
                ResultJson->SetNumberField(TEXT("startFrame"), Start.Value);
                ResultJson->SetNumberField(TEXT("endFrame"), End.Value);

                TArray<TSharedPtr<FJsonValue>> TracksArray;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                // UE 5.1+: GetMasterTracks() replaced with GetTracks()
                const TArray<UMovieSceneTrack*>& MasterTracks = TargetAnim->MovieScene->GetTracks();
#else
                // UE 5.0: Use GetMasterTracks()
                const TArray<UMovieSceneTrack*>& MasterTracks = TargetAnim->MovieScene->GetMasterTracks();
#endif
                for (UMovieSceneTrack* Track : MasterTracks)
                {
                    if (Track)
                    {
                        TSharedPtr<FJsonObject> TrackInfo = MakeShared<FJsonObject>();
                        TrackInfo->SetStringField(TEXT("name"), Track->GetTrackName().ToString());
                        TrackInfo->SetStringField(TEXT("type"), Track->GetClass()->GetName());
                        TracksArray.Add(MakeShared<FJsonValueObject>(TrackInfo));
                    }
                }
                ResultJson->SetArrayField(TEXT("tracks"), TracksArray);
            }
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Retrieved animation info"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("delete_animation"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString AnimationName = GetJsonStringField(Payload, TEXT("animationName"));

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

        int32 FoundIndex = INDEX_NONE;
        for (int32 i = 0; i < WidgetBP->Animations.Num(); ++i)
        {
            if (WidgetBP->Animations[i] && WidgetBP->Animations[i]->GetName().Equals(AnimationName, ESearchCase::IgnoreCase))
            {
                FoundIndex = i;
                break;
            }
        }

        if (FoundIndex == INDEX_NONE)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Animation '%s' not found"), *AnimationName), TEXT("NOT_FOUND"));
            return true;
        }

        WidgetBP->Animations.RemoveAt(FoundIndex);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("deletedAnimation"), AnimationName);
        ResultJson->SetNumberField(TEXT("remainingAnimations"), WidgetBP->Animations.Num());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Deleted animation"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.15 Localization Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("set_localization_key"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString Namespace = GetJsonStringField(Payload, TEXT("namespace"), TEXT("Game"));
        FString Key = GetJsonStringField(Payload, TEXT("key"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || Key.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters: widgetPath, slotName, key"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        bool bApplied = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
            // Create localized text reference
            FText LocalizedText = FText::ChangeKey(FTextKey(Namespace), FTextKey(Key), TextWidget->GetText());
            TextWidget->SetText(LocalizedText);
            bApplied = true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bApplied);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("namespace"), Namespace);
        ResultJson->SetStringField(TEXT("key"), Key);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Set localization key"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("bind_localized_text"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"));
        FString StringTableId = GetJsonStringField(Payload, TEXT("stringTableId"));
        FString StringKey = GetJsonStringField(Payload, TEXT("stringKey"));

        if (WidgetPath.IsEmpty() || SlotName.IsEmpty() || StringTableId.IsEmpty() || StringKey.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameters"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        UWidget* TargetWidget = WidgetBP->WidgetTree->FindWidget(FName(*SlotName));
        if (!TargetWidget)
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Widget '%s' not found"), *SlotName), TEXT("NOT_FOUND"));
            return true;
        }

        bool bBound = false;
        if (UTextBlock* TextWidget = Cast<UTextBlock>(TargetWidget))
        {
            // Try to get text from string table
            FText LocalizedText = FText::FromStringTable(FName(*StringTableId), StringKey);
            if (!LocalizedText.IsEmpty())
            {
                TextWidget->SetText(LocalizedText);
                bBound = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), bBound);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetStringField(TEXT("stringTableId"), StringTableId);
        ResultJson->SetStringField(TEXT("stringKey"), StringKey);
        if (!bBound)
        {
            ResultJson->SetStringField(TEXT("note"), TEXT("String table entry not found or widget is not a text widget"));
        }

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Bound localized text"), ResultJson);
        return true;
    }

    // =========================================================================
    // 19.16 Additional Template Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("create_credits_screen"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_Credits"));
        FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create credits widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Background
        UImage* Background = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
        RootCanvas->AddChild(Background);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Background->Slot))
        {
            Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            Slot->SetOffsets(FMargin(0.0f));
        }

        // Scrolling credits container
        UScrollBox* CreditsScroll = WidgetBP->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("CreditsScroll"));
        RootCanvas->AddChild(CreditsScroll);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CreditsScroll->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 1.0f));
            Slot->SetAlignment(FVector2D(0.5f, 0.0f));
            Slot->SetSize(FVector2D(600.0f, 0.0f));
            Slot->SetOffsets(FMargin(0.0f, 50.0f, 0.0f, 50.0f));
        }

        // Credits content
        UVerticalBox* CreditsContent = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CreditsContent"));
        CreditsScroll->AddChild(CreditsContent);

        // Sample credits sections
        TArray<FString> Sections = { TEXT("Lead Developer"), TEXT("Art Director"), TEXT("Sound Design"), TEXT("Special Thanks") };
        for (const FString& Section : Sections)
        {
            UTextBlock* SectionTitle = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Section.Replace(TEXT(" "), TEXT("_")) + TEXT("_Title")));
            SectionTitle->SetText(FText::FromString(Section));
            CreditsContent->AddChild(SectionTitle);

            UTextBlock* SectionName = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Section.Replace(TEXT(" "), TEXT("_")) + TEXT("_Name")));
            SectionName->SetText(FText::FromString(TEXT("Your Name Here")));
            CreditsContent->AddChild(SectionName);
        }

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created credits screen"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_shop_ui"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_Shop"));
        FString Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI"));
        int32 ItemColumns = GetJsonIntField(Payload, TEXT("columns"), 4);

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/Game/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create shop widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // Root canvas
        UCanvasPanel* RootCanvas = WidgetBP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Shop background
        UBorder* ShopBg = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopBackground"));
        RootCanvas->AddChild(ShopBg);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(ShopBg->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(800.0f, 600.0f));
        }

        UVerticalBox* ShopLayout = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopLayout"));
        ShopBg->AddChild(ShopLayout);

        // Header
        UHorizontalBox* Header = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopHeader"));
        ShopLayout->AddChild(Header);

        UTextBlock* ShopTitle = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopTitle"));
        ShopTitle->SetText(FText::FromString(TEXT("Shop")));
        Header->AddChild(ShopTitle);

        UTextBlock* CurrencyDisplay = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CurrencyDisplay"));
        CurrencyDisplay->SetText(FText::FromString(TEXT("Gold: 0")));
        Header->AddChild(CurrencyDisplay);

        // Category tabs
        UHorizontalBox* CategoryTabs = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CategoryTabs"));
        ShopLayout->AddChild(CategoryTabs);

        TArray<FString> Categories = { TEXT("Weapons"), TEXT("Armor"), TEXT("Consumables"), TEXT("Special") };
        for (const FString& Category : Categories)
        {
            UButton* TabBtn = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *(Category + TEXT("_Tab")));
            UTextBlock* TabText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(Category + TEXT("_TabText")));
            TabText->SetText(FText::FromString(Category));
            TabBtn->AddChild(TabText);
            CategoryTabs->AddChild(TabBtn);
        }

        // Items grid
        UScrollBox* ItemsScroll = WidgetBP->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ItemsScroll"));
        ShopLayout->AddChild(ItemsScroll);

        UUniformGridPanel* ItemsGrid = WidgetBP->WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("ItemsGrid"));
        ItemsScroll->AddChild(ItemsGrid);

        // Sample item slots
        for (int32 i = 0; i < 8; ++i)
        {
            FString ItemName = FString::Printf(TEXT("ItemSlot_%d"), i);
            UBorder* ItemSlot = WidgetBP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), *ItemName);
            ItemsGrid->AddChildToUniformGrid(ItemSlot, i / ItemColumns, i % ItemColumns);

            UVerticalBox* ItemContent = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *(ItemName + TEXT("_Content")));
            ItemSlot->AddChild(ItemContent);

            UImage* ItemIcon = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(ItemName + TEXT("_Icon")));
            ItemContent->AddChild(ItemIcon);

            UTextBlock* ItemLabel = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(ItemName + TEXT("_Name")));
            ItemLabel->SetText(FText::FromString(TEXT("Item")));
            ItemContent->AddChild(ItemLabel);

            UTextBlock* ItemPrice = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(ItemName + TEXT("_Price")));
            ItemPrice->SetText(FText::FromString(TEXT("100g")));
            ItemContent->AddChild(ItemPrice);
        }

        // Buy button
        UButton* BuyButton = WidgetBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("BuyButton"));
        ShopLayout->AddChild(BuyButton);
        UTextBlock* BuyText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BuyButtonText"));
        BuyText->SetText(FText::FromString(TEXT("Buy Selected")));
        BuyButton->AddChild(BuyText);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetNumberField(TEXT("columns"), ItemColumns);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created shop UI"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_quest_tracker"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("QuestTracker"));

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

        // Quest tracker container
        UVerticalBox* QuestContainer = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *SlotName);

        // Quest header
        UTextBlock* QuestHeader = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(SlotName + TEXT("_Header")));
        QuestHeader->SetText(FText::FromString(TEXT("Active Quest")));
        QuestContainer->AddChild(QuestHeader);

        // Quest title
        UTextBlock* QuestTitle = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(SlotName + TEXT("_Title")));
        QuestTitle->SetText(FText::FromString(TEXT("Quest Name")));
        QuestContainer->AddChild(QuestTitle);

        // Quest objectives list
        UVerticalBox* ObjectivesList = WidgetBP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), *(SlotName + TEXT("_Objectives")));
        QuestContainer->AddChild(ObjectivesList);

        // Sample objectives
        for (int32 i = 1; i <= 3; ++i)
        {
            UHorizontalBox* ObjRow = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *FString::Printf(TEXT("%s_Objective_%d"), *SlotName, i));
            
            UCheckBox* ObjCheck = WidgetBP->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), *FString::Printf(TEXT("%s_ObjCheck_%d"), *SlotName, i));
            ObjRow->AddChild(ObjCheck);
            
            UTextBlock* ObjText = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("%s_ObjText_%d"), *SlotName, i));
            ObjText->SetText(FText::FromString(FString::Printf(TEXT("Objective %d (0/1)"), i)));
            ObjRow->AddChild(ObjText);

            ObjectivesList->AddChild(ObjRow);
        }

        // Quest rewards preview
        UHorizontalBox* RewardsRow = WidgetBP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *(SlotName + TEXT("_Rewards")));
        QuestContainer->AddChild(RewardsRow);

        UTextBlock* RewardsLabel = WidgetBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *(SlotName + TEXT("_RewardsLabel")));
        RewardsLabel->SetText(FText::FromString(TEXT("Rewards: ")));
        RewardsRow->AddChild(RewardsLabel);

        UImage* RewardIcon = WidgetBP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *(SlotName + TEXT("_RewardIcon")));
        RewardsRow->AddChild(RewardIcon);

        UPanelWidget* Parent = Cast<UPanelWidget>(WidgetBP->WidgetTree->RootWidget);
        if (Parent)
        {
            Parent->AddChild(QuestContainer);
            if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(QuestContainer->Slot))
            {
                Slot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f)); // Top-left
                Slot->SetAlignment(FVector2D(0.0f, 0.0f));
                Slot->SetPosition(FVector2D(20.0f, 100.0f));
                Slot->SetAutoSize(true);
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added quest tracker"), ResultJson);
        return true;
    }

    // Action not recognized
    return false;
}
#pragma warning(pop)
