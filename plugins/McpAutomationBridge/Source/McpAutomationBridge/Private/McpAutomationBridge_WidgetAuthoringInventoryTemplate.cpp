#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
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

bool HandleWidgetAuthoringInventoryTemplate(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("create_inventory_ui"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_Inventory"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }
        int32 GridColumns = GetJsonIntField(Payload, TEXT("columns"), 6);
        int32 GridRows = GetJsonIntField(Payload, TEXT("rows"), 4);

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
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create inventory widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Create root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Background panel
        UBorder* BackgroundPanel = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, TEXT("InventoryBackground"));
        RootCanvas->AddChild(BackgroundPanel);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(BackgroundPanel->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(GridColumns * 80.0f + 40.0f, GridRows * 80.0f + 100.0f));
        }

        // Title
        UTextBlock* TitleText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("InventoryTitle"));
        TitleText->SetText(FText::FromString(TEXT("Inventory")));
        BackgroundPanel->AddChild(TitleText);

        // Create inventory grid
        UUniformGridPanel* InventoryGrid = CreateAndRegisterWidget<UUniformGridPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("InventoryGrid"));
        BackgroundPanel->AddChild(InventoryGrid);

        // Add visible inventory slot widgets
        for (int32 Row = 0; Row < GridRows; ++Row)
        {
            for (int32 Col = 0; Col < GridColumns; ++Col)
            {
                FString SlotName = FString::Printf(TEXT("Slot_%d_%d"), Row, Col);
                UBorder* SlotBorder = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, *SlotName);
                InventoryGrid->AddChildToUniformGrid(SlotBorder, Row, Col);

                UImage* SlotImage = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(SlotName + TEXT("_Image")));
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

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created inventory UI"), ResultJson);
        return true;
    }

    return false;
}
}
