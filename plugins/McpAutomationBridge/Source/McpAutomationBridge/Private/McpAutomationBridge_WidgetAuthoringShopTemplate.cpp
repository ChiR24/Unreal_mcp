#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
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

bool HandleWidgetAuthoringShopTemplate(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    if (SubAction.Equals(TEXT("create_shop_ui"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_Shop"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI"); }
        int32 ItemColumns = GetJsonIntField(Payload, TEXT("columns"), 4);

        FString FullPath = Folder / Name;
        if (!FullPath.StartsWith(TEXT("/")))
        {
            FullPath = TEXT("/Game/") + FullPath;
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
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create shop widget"), TEXT("CREATION_ERROR"));
            return true;
        }

        // CRITICAL: Use CreateAndRegisterWidget to register GUIDs IMMEDIATELY after creation.
        // This prevents ensure failures if compilation is triggered during widget creation.
        // The compiler's ValidateAndFixUpVariableGuids() expects all widgets to be in the GUID map.

        // Root canvas
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("RootCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Shop background
        UBorder* ShopBg = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, TEXT("ShopBackground"));
        RootCanvas->AddChild(ShopBg);
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(ShopBg->Slot))
        {
            Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
            Slot->SetAlignment(FVector2D(0.5f, 0.5f));
            Slot->SetSize(FVector2D(800.0f, 600.0f));
        }

        UVerticalBox* ShopLayout = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("ShopLayout"));
        ShopBg->AddChild(ShopLayout);

        // Header
        UHorizontalBox* Header = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("ShopHeader"));
        ShopLayout->AddChild(Header);

        UTextBlock* ShopTitle = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("ShopTitle"));
        ShopTitle->SetText(FText::FromString(TEXT("Shop")));
        Header->AddChild(ShopTitle);

        UTextBlock* CurrencyDisplay = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("CurrencyDisplay"));
        CurrencyDisplay->SetText(FText::FromString(TEXT("Gold: 0")));
        Header->AddChild(CurrencyDisplay);

        // Category tabs
        UHorizontalBox* CategoryTabs = CreateAndRegisterWidget<UHorizontalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("CategoryTabs"));
        ShopLayout->AddChild(CategoryTabs);

        TArray<FString> Categories = { TEXT("Weapons"), TEXT("Armor"), TEXT("Consumables"), TEXT("Special") };
        for (const FString& Category : Categories)
        {
            UButton* TabBtn = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, *(Category + TEXT("_Tab")));
            UTextBlock* TabText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(Category + TEXT("_TabText")));
            TabText->SetText(FText::FromString(Category));
            TabBtn->AddChild(TabText);
            CategoryTabs->AddChild(TabBtn);
        }

        // Items grid
        UScrollBox* ItemsScroll = CreateAndRegisterWidget<UScrollBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("ItemsScroll"));
        ShopLayout->AddChild(ItemsScroll);

        UUniformGridPanel* ItemsGrid = CreateAndRegisterWidget<UUniformGridPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("ItemsGrid"));
        ItemsScroll->AddChild(ItemsGrid);

        // Sample item slots
        for (int32 i = 0; i < 8; ++i)
        {
            FString ItemName = FString::Printf(TEXT("ItemSlot_%d"), i);
            UBorder* ItemSlot = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, *ItemName);
            ItemsGrid->AddChildToUniformGrid(ItemSlot, i / ItemColumns, i % ItemColumns);

            UVerticalBox* ItemContent = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, *(ItemName + TEXT("_Content")));
            ItemSlot->AddChild(ItemContent);

            UImage* ItemIcon = CreateAndRegisterWidget<UImage>(WidgetBP, WidgetBP->WidgetTree, *(ItemName + TEXT("_Icon")));
            ItemContent->AddChild(ItemIcon);

            UTextBlock* ItemLabel = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(ItemName + TEXT("_Name")));
            ItemLabel->SetText(FText::FromString(TEXT("Item")));
            ItemContent->AddChild(ItemLabel);

            UTextBlock* ItemPrice = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, *(ItemName + TEXT("_Price")));
            ItemPrice->SetText(FText::FromString(TEXT("100g")));
            ItemContent->AddChild(ItemPrice);
        }

        // Buy button
        UButton* BuyButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("BuyButton"));
        ShopLayout->AddChild(BuyButton);
        UTextBlock* BuyText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("BuyButtonText"));
        BuyText->SetText(FText::FromString(TEXT("Buy Selected")));
        BuyButton->AddChild(BuyText);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetNumberField(TEXT("columns"), ItemColumns);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created shop UI"), ResultJson);
        return true;
    }

    return false;
}
}
