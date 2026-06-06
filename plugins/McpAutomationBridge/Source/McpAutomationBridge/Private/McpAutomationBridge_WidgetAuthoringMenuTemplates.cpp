#include "McpAutomationBridge_WidgetAuthoringActions.h"
#include "McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "McpAutomationBridge_WidgetAuthoringGuidRegistry.h"
#include "McpAutomationBridge_WidgetAuthoringTreeMutation.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/Overlay.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Styling/SlateTypes.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringHandlers
{
using namespace WidgetAuthoringHelpers;

bool HandleWidgetAuthoringMenuTemplates(
    UMcpAutomationBridgeSubsystem& Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    TSharedPtr<FJsonObject> ResultJson)
{
    // =========================================================================
    // 19.7 UI Templates - Real Implementation (creates composite widget structures)
    // =========================================================================

    if (SubAction.Equals(TEXT("create_main_menu"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));
        FString Title = GetJsonStringField(Payload, TEXT("title"), TEXT("Main Menu"));

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

        // CRITICAL: Clear the entire widget tree for a complete rebuild.
        // This removes ALL widgets and clears the GUID map, preventing orphaned widgets
        // from triggering ensure failures during compilation.
        // See: ForEachObjectWithOuter in WidgetBlueprintCompiler.cpp line 792
        ClearWidgetTreeForRebuild(WidgetBP);

        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // This prevents "Widget was added but did not get a GUID" ensure failures

        // Create Canvas Panel as root
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("MainMenuCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // Create vertical box for menu items
        UVerticalBox* MenuBox = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("MenuVerticalBox"));
        RootCanvas->AddChild(MenuBox);

        // Add title text
        UTextBlock* TitleText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("TitleText"));
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
        UButton* PlayButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("PlayButton"));
        UTextBlock* PlayText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("PlayButtonText"));
        PlayText->SetText(FText::FromString(TEXT("Play")));
        PlayButton->AddChild(PlayText);
        MenuBox->AddChild(PlayButton);

        // Add Settings button
        UButton* SettingsButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("SettingsButton"));
        UTextBlock* SettingsText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("SettingsButtonText"));
        SettingsText->SetText(FText::FromString(TEXT("Settings")));
        SettingsButton->AddChild(SettingsText);
        MenuBox->AddChild(SettingsButton);

        // Add Quit button
        UButton* QuitButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("QuitButton"));
        UTextBlock* QuitText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("QuitButtonText"));
        QuitText->SetText(FText::FromString(TEXT("Quit")));
        QuitButton->AddChild(QuitText);
        MenuBox->AddChild(QuitButton);

        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        // Keeping it for safety in case any edge case widgets were missed
        RegisterAllWidgetGuids(WidgetBP);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("title"), Title);

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Main menu created"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_pause_menu"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));

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

        // CRITICAL: Clear the entire widget tree for a complete rebuild.
        // This removes ALL widgets and clears the GUID map, preventing orphaned widgets
        // from triggering ensure failures during compilation.
        ClearWidgetTreeForRebuild(WidgetBP);

        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // This prevents "Widget was added but did not get a GUID" ensure failures

        // Create overlay for semi-transparent background
        UOverlay* RootOverlay = CreateAndRegisterWidget<UOverlay>(WidgetBP, WidgetBP->WidgetTree, TEXT("PauseMenuOverlay"));
        WidgetBP->WidgetTree->RootWidget = RootOverlay;

        // Add background border with color
        UBorder* Background = CreateAndRegisterWidget<UBorder>(WidgetBP, WidgetBP->WidgetTree, TEXT("Background"));
        Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
        RootOverlay->AddChild(Background);

        // Add menu vertical box
        UVerticalBox* MenuBox = CreateAndRegisterWidget<UVerticalBox>(WidgetBP, WidgetBP->WidgetTree, TEXT("PauseMenuBox"));
        RootOverlay->AddChild(MenuBox);

        // Add PAUSED title
        UTextBlock* TitleText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("PausedTitle"));
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
        UButton* ResumeButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("ResumeButton"));
        UTextBlock* ResumeText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("ResumeText"));
        ResumeText->SetText(FText::FromString(TEXT("Resume")));
        ResumeButton->AddChild(ResumeText);
        MenuBox->AddChild(ResumeButton);

        // Add Main Menu button
        UButton* MainMenuButton = CreateAndRegisterWidget<UButton>(WidgetBP, WidgetBP->WidgetTree, TEXT("MainMenuButton"));
        UTextBlock* MainMenuText = CreateAndRegisterWidget<UTextBlock>(WidgetBP, WidgetBP->WidgetTree, TEXT("MainMenuText"));
        MainMenuText->SetText(FText::FromString(TEXT("Main Menu")));
        MainMenuButton->AddChild(MainMenuText);
        MenuBox->AddChild(MainMenuButton);

        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Pause menu created"), ResultJson);
        return true;
    }

    if (SubAction.Equals(TEXT("create_hud_widget"), ESearchCase::IgnoreCase))
    {
        FString WidgetPath = GetJsonStringField(Payload, TEXT("widgetPath"));

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

        // CRITICAL: Clear the entire widget tree for a complete rebuild.
        // This removes ALL widgets and clears the GUID map, preventing orphaned widgets
        // from triggering ensure failures during compilation.
        ClearWidgetTreeForRebuild(WidgetBP);

        // CRITICAL: Use CreateAndRegisterWidget to register GUID immediately after creation
        // This prevents "Widget was added but did not get a GUID" ensure failures

        // Create Canvas Panel as root for HUD
        UCanvasPanel* RootCanvas = CreateAndRegisterWidget<UCanvasPanel>(WidgetBP, WidgetBP->WidgetTree, TEXT("HUDCanvas"));
        WidgetBP->WidgetTree->RootWidget = RootCanvas;

        // RegisterAllWidgetGuids is now optional cleanup - all widgets already registered
        RegisterAllWidgetGuids(WidgetBP);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBP);
        McpSafeAssetSave(WidgetBP);

        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("widgetPath"), WidgetBP->GetPathName());
        ResultJson->SetStringField(TEXT("note"), TEXT("HUD canvas created. Use add_health_bar, add_crosshair, add_ammo_counter to add HUD elements."));

        Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("HUD widget created"), ResultJson);
        return true;
    }

    return false;
}
}
