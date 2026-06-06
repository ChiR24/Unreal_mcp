#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringLoadingMinimapTemplates(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("create_loading_screen"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_LoadingScreen"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
        }

        // CRITICAL: Check if widget blueprint already exists to prevent engine assertion
        FString NewBPObjectPath = FullPath + TEXT(".") + Name;
        if (FindObject<UWidgetBlueprint>(nullptr, *NewBPObjectPath) != nullptr)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Widget blueprint '%s' already exists"), *Name),
                TEXT("ALREADY_EXISTS"));
            return true;
        }

        UPackage* Package = CreatePackage(*FullPath);
        if (!Package)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create package"), TEXT("PACKAGE_ERROR"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), Package, FName(*Name),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));

        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create loading screen widget"), TEXT("CREATION_ERROR"));
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

        // CRITICAL: Register all widget GUIDs and mark as user-created
        // This prevents ensure failures in WidgetBlueprintCompiler.cpp line 794
        RegisterAllWidgetGuids(WidgetBP);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("message"), TEXT("Created loading screen template"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created loading screen template"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_minimap"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Minimap"));
        float Size = GetJsonNumberField(Payload, TEXT("size"), 200.0f);

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
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

        // CRITICAL: Register all widget GUIDs to prevent ensure failures during compilation
        RegisterAllWidgetGuids(WidgetBP);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetPath);
        ResultJson->SetStringField(TEXT("slotName"), SlotName);
        ResultJson->SetNumberField(TEXT("size"), Size);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added minimap widget"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("add_compass"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString SlotName = GetJsonStringField(Payload, TEXT("slotName"), TEXT("Compass"));

        if (WidgetPath.IsEmpty())
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Missing required parameter: widgetPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UWidgetBlueprint* WidgetBP = LoadWidgetBlueprint(WidgetPath);
        if (!WidgetBP || !WidgetBP->WidgetTree)
        {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Widget blueprint not found"), TEXT("NOT_FOUND"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Create compass container
        UHorizontalBox* CompassContainer = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, *SlotName);

        // Create compass image (scrolling texture)
        UImage* CompassImage = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Image")));
        CompassContainer->AddChild(CompassImage);

        // Create direction indicator
        UImage* DirectionIndicator = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Indicator")));
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

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added compass widget"), ResultJson);
        return true;
    }

    return false;
}
}
