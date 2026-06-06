#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
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

bool HandleWidgetAuthoringSettingsTemplate(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.10 Missing UI Template Actions
    // =========================================================================

    if (SubAction.Equals(TEXT("create_settings_menu"), ESearchCase::IgnoreCase))
    {
        FString Name = GetJsonStringField(Payload, TEXT("name"), TEXT("WBP_SettingsMenu"));
        FString Folder = GetJsonStringField(Payload, TEXT("path"));
        if (Folder.IsEmpty()) { Folder = GetJsonStringField(Payload, TEXT("folder"), TEXT("/Game/UI/Menus")); }
        FString RawFolder = Folder;
        Folder = SanitizeProjectRelativePath(Folder);
        if (Folder.IsEmpty() && !RawFolder.IsEmpty()) {
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Invalid folder path: path traversal or invalid characters detected"), TEXT("INVALID_PATH"));
            return true;
        }
        if (Folder.IsEmpty()) { Folder = TEXT("/Game/UI/Menus"); }

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
            Subsystem.SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create settings menu widget"), TEXT("CREATION_ERROR"));
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

        // CRITICAL: Register all widget GUIDs and mark as user-created
        // This prevents ensure failures in WidgetBlueprintCompiler.cpp line 794
        RegisterAllWidgetGuids(WidgetBP);

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(WidgetBP);
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("message"), TEXT("Created settings menu template"));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Created settings menu template"), ResultJson);
        return true;
    }

    return false;
}
}
