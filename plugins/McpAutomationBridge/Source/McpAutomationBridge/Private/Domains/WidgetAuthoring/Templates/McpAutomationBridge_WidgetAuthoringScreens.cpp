#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringScreens.h"

#include "Blueprint/WidgetTree.h"
#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringBlueprintLoading.h"
#include "Domains/WidgetAuthoring/Support/McpAutomationBridge_WidgetAuthoringValidation.h"
#include "Domains/WidgetAuthoring/Templates/McpAutomationBridge_WidgetAuthoringSpec.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "WidgetBlueprint.h"

namespace WidgetAuthoringScreens
{
using namespace WidgetAuthoringHelpers;

namespace
{
// Title + a column of buttons centred on a backdrop: the main and pause menus.
TSharedPtr<FJsonObject> BuildMenu(const TSharedPtr<FJsonObject>& Payload, const TCHAR* DefaultTitle,
                                  const TArray<FString>& DefaultButtons, double BackdropAlpha)
{
    TSharedPtr<FJsonObject> Root = RootWithBackdrop(BackdropAlpha);
    TSharedPtr<FJsonObject> Box = SetSlot(Append(Root, Node(TEXT("VerticalBox"), TEXT("MenuBox"))),
        TEXT(R"({"anchors":[0.5,0.5,0.5,0.5],"alignment":[0.5,0.5],"autoSize":true})"));
    TSharedPtr<FJsonObject> Title = SetSlot(Append(Box, Node(TEXT("TextBlock"), TEXT("TitleText"),
        GetJsonStringField(Payload, TEXT("title"), DefaultTitle))), TEXT(R"({"padding":[0,0,0,32],"hAlign":"center"})"));
    Title->SetNumberField(TEXT("fontSize"), 56);
    TSharedPtr<FJsonObject> Column = SetSlot(Append(Box, Node(TEXT("SizeBox"), TEXT("ButtonColumn"))), TEXT(R"({"hAlign":"center"})"));
    Column->SetNumberField(TEXT("width"), 360);
    TSharedPtr<FJsonObject> List = Append(Column, Node(TEXT("VerticalBox"), TEXT("ButtonList")));
    TArray<FString> Labels = McpGetStringListField(Payload, TEXT("buttons"), TEXT("buttons"));
    for (const FString& Label : Labels.Num() > 0 ? Labels : DefaultButtons)
    {
        Append(List, Button(NameStem(Label) + TEXT("Button"), Label));
    }
    return Root;
}

TSharedPtr<FJsonObject> BuildMainMenu(const TSharedPtr<FJsonObject>& Payload, FString&)
{
    return BuildMenu(Payload, TEXT("Main Menu"), { TEXT("Play"), TEXT("Settings"), TEXT("Quit") }, 1.0);
}

TSharedPtr<FJsonObject> BuildPauseMenu(const TSharedPtr<FJsonObject>& Payload, FString&)
{
    return BuildMenu(Payload, TEXT("PAUSED"), { TEXT("Resume"), TEXT("Settings"), TEXT("Main Menu") }, 0.6);
}

TSharedPtr<FJsonObject> BuildLoadingScreen(const TSharedPtr<FJsonObject>& Payload, FString&)
{
    TSharedPtr<FJsonObject> Root = RootWithBackdrop(1.0);
    TSharedPtr<FJsonObject> Text = SetSlot(Append(Root, Node(TEXT("TextBlock"), TEXT("LoadingText"), TEXT("Loading..."))),
        TEXT(R"({"anchors":[0.5,0.62,0.5,0.62],"alignment":[0.5,0.5],"autoSize":true})"));
    Text->SetNumberField(TEXT("fontSize"), 32);
    if (GetJsonBoolField(Payload, TEXT("includeProgressBar"), true))
    {
        TSharedPtr<FJsonObject> Bar = SetSlot(Append(Root, Node(TEXT("ProgressBar"), TEXT("LoadingProgressBar"))),
            TEXT(R"({"anchors":[0.5,0.7,0.5,0.7],"alignment":[0.5,0.5],"size":[480,16]})"));
        Numbers(Bar, TEXT("color"), { 1.0, 0.8, 0.2, 1.0 })->SetNumberField(TEXT("percent"), 0);
    }
    return Root;
}

void FinishLoadingScreen(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& Result)
{
    const double Fade = GetJsonNumberField(Payload, TEXT("fadeTime"), 0.0);
    if (Fade > 0.0 && McpAddOpacityAnimation(WidgetBP, TEXT("FadeIn"), WidgetBP->WidgetTree->RootWidget,
                                            { FVector2D(0.0, 0.0), FVector2D(Fade, 1.0) }))
    {
        Result->SetStringField(TEXT("animationName"), TEXT("FadeIn"));
    }
}

void SettingRow(const TSharedPtr<FJsonObject>& List, const FString& Label, const TSharedPtr<FJsonObject>& Control)
{
    const FString Stem = NameStem(Label);
    TSharedPtr<FJsonObject> Row = SetSlot(Append(List, Node(TEXT("HorizontalBox"), Stem + TEXT("Row"))), TEXT(R"({"padding":[0,4,0,4]})"));
    SetSlot(Append(Row, Node(TEXT("TextBlock"), Stem + TEXT("Label"), Label)), TEXT(R"({"fill":1,"vAlign":"center"})"))
        ->SetNumberField(TEXT("fontSize"), 18);
    TSharedPtr<FJsonObject> Box = SetSlot(Append(Row, Node(TEXT("SizeBox"), Stem + TEXT("Box"))), TEXT(R"({"vAlign":"center"})"));
    Box->SetNumberField(TEXT("width"), 240);
    Append(Box, Control);
}

TSharedPtr<FJsonObject> Valued(const TCHAR* Type, const FString& Name, const TCHAR* Field, double Value)
{
    TSharedPtr<FJsonObject> Result = Node(Type, Name);
    Result->SetNumberField(Field, Value);
    return Result;
}

void Section(const TSharedPtr<FJsonObject>& List, const FString& Title)
{
    TSharedPtr<FJsonObject> Text = SetSlot(Append(List, Node(TEXT("TextBlock"), NameStem(Title) + TEXT("Header"), Title)),
                                           TEXT(R"({"padding":[0,16,0,4]})"));
    Numbers(Text, TEXT("color"), { 1.0, 0.8, 0.2, 1.0 })->SetNumberField(TEXT("fontSize"), 20);
}

TSharedPtr<FJsonObject> BuildSettingsMenu(const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    const FString Kind = GetJsonStringField(Payload, TEXT("settingsType"), TEXT("all")).ToLower();
    if (Kind != TEXT("all") && Kind != TEXT("graphics") && Kind != TEXT("audio") && Kind != TEXT("controls"))
    {
        OutError = FString::Printf(TEXT("settingsType '%s' is not one of all, graphics, audio, controls"), *Kind);
        return nullptr;
    }
    TSharedPtr<FJsonObject> Root = RootWithBackdrop(0.85);
    TSharedPtr<FJsonObject> Panel = SetSlot(Append(Root, Numbers(Node(TEXT("Border"), TEXT("SettingsPanel")), TEXT("color"), { 0.06, 0.06, 0.08, 0.95 })),
        TEXT(R"({"anchors":[0.5,0.5,0.5,0.5],"alignment":[0.5,0.5],"autoSize":true})"));
    Panel->SetNumberField(TEXT("radius"), 12);
    Panel->SetNumberField(TEXT("padding"), 32);
    TSharedPtr<FJsonObject> Width = Append(Panel, Valued(TEXT("SizeBox"), TEXT("SettingsWidth"), TEXT("width"), 640));
    TSharedPtr<FJsonObject> List = Append(Width, Node(TEXT("VerticalBox"), TEXT("SettingsList")));
    Append(List, Valued(TEXT("TextBlock"), TEXT("TitleText"), TEXT("fontSize"), 40))->SetStringField(TEXT("text"), TEXT("Settings"));
    if (Kind == TEXT("all") || Kind == TEXT("graphics"))
    {
        Section(List, TEXT("Graphics"));
        TSharedPtr<FJsonObject> Quality = Node(TEXT("ComboBoxString"), TEXT("QualityCombo"));
        Quality->SetArrayField(TEXT("options"), { MakeShared<FJsonValueString>(TEXT("Low")), MakeShared<FJsonValueString>(TEXT("Medium")),
                                                   MakeShared<FJsonValueString>(TEXT("High")), MakeShared<FJsonValueString>(TEXT("Epic")) });
        Quality->SetStringField(TEXT("selected"), TEXT("High"));
        SettingRow(List, TEXT("Quality"), Quality);
        SettingRow(List, TEXT("Fullscreen"), Node(TEXT("CheckBox"), TEXT("FullscreenCheck")));
        TSharedPtr<FJsonObject> VSync = Node(TEXT("CheckBox"), TEXT("VSyncCheck"));
        VSync->SetBoolField(TEXT("checked"), true);
        SettingRow(List, TEXT("VSync"), VSync);
    }
    if (Kind == TEXT("all") || Kind == TEXT("audio"))
    {
        Section(List, TEXT("Audio"));
        SettingRow(List, TEXT("Master Volume"), Valued(TEXT("Slider"), TEXT("MasterVolumeSlider"), TEXT("value"), 1.0));
        SettingRow(List, TEXT("Music Volume"), Valued(TEXT("Slider"), TEXT("MusicVolumeSlider"), TEXT("value"), 0.8));
        SettingRow(List, TEXT("Effects Volume"), Valued(TEXT("Slider"), TEXT("EffectsVolumeSlider"), TEXT("value"), 0.8));
    }
    if (Kind == TEXT("all") || Kind == TEXT("controls"))
    {
        Section(List, TEXT("Controls"));
        SettingRow(List, TEXT("Mouse Sensitivity"), Valued(TEXT("Slider"), TEXT("MouseSensitivitySlider"), TEXT("value"), 0.5));
        SettingRow(List, TEXT("Invert Y"), Node(TEXT("CheckBox"), TEXT("InvertYCheck")));
    }
    TSharedPtr<FJsonObject> Footer = SetSlot(Append(List, Node(TEXT("HorizontalBox"), TEXT("Footer"))), TEXT(R"({"padding":[0,24,0,0]})"));
    SetSlot(Append(Footer, Button(TEXT("ApplyButton"), TEXT("Apply"))), TEXT(R"({"fill":1,"padding":[0,0,8,0]})"));
    SetSlot(Append(Footer, Button(TEXT("BackButton"), TEXT("Back"))), TEXT(R"({"fill":1,"padding":[8,0,0,0]})"));
    return Root;
}

const TArray<FString>& DefaultHudElements()
{
    static const TArray<FString> Elements = { TEXT("health_bar"), TEXT("crosshair"), TEXT("ammo_counter") };
    return Elements;
}

// The HUD screen is a canvas holding the same pieces add_game_widget adds, each under its
// default slot name so a later add_game_widget call can sit beside it.
TSharedPtr<FJsonObject> BuildHud(const TSharedPtr<FJsonObject>& Payload, FString& OutError)
{
    TSharedPtr<FJsonObject> Root = Node(TEXT("CanvasPanel"), TEXT("RootCanvas"));
    const bool bListed = Payload->HasField(TEXT("elements"));
    const TArray<FString> Elements = bListed ? McpGetStringListField(Payload, TEXT("elements"), TEXT("elements")) : DefaultHudElements();
    const TSharedPtr<FJsonObject> NoKnobs = MakeShared<FJsonObject>();
    for (const FString& Kind : Elements)
    {
        TSharedPtr<FJsonObject> Spec;
        FString Slot, Label;
        if (!McpResolveHudElement(TEXT("add_") + Kind, NoKnobs, Spec, Slot, Label, OutError))
        {
            OutError = FString::Printf(TEXT("'%s' is not a HUD element; use health_bar, ammo_counter, crosshair, minimap, "
                                            "compass, damage_indicator, interaction_prompt, objective_tracker or quest_tracker"), *Kind);
            return nullptr;
        }
        McpBindSpecSlot(Spec, Slot);
        Append(Root, Spec);
    }
    return Root;
}

void FinishHud(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& Result)
{
    const bool bListed = Payload->HasField(TEXT("elements"));
    for (const FString& Kind : bListed ? McpGetStringListField(Payload, TEXT("elements"), TEXT("elements")) : DefaultHudElements())
    {
        TSharedPtr<FJsonObject> Spec;
        FString Slot, Label, Error;
        McpResolveHudElement(TEXT("add_") + Kind, MakeShared<FJsonObject>(), Spec, Slot, Label, Error);
        const FString Animation = McpFinishHudElement(WidgetBP, TEXT("add_") + Kind, Slot, MakeShared<FJsonObject>());
        if (!Animation.IsEmpty())
        {
            Result->SetStringField(TEXT("animationName"), Animation);
        }
    }
}

}

const TArray<FScreenTemplate>& MenuScreens()
{
    static const TArray<FScreenTemplate> Screens = {
        { TEXT("create_main_menu"), TEXT("WBP_MainMenu"), TEXT("main menu"), &BuildMainMenu, nullptr },
        { TEXT("create_pause_menu"), TEXT("WBP_PauseMenu"), TEXT("pause menu"), &BuildPauseMenu, nullptr },
        { TEXT("create_settings_menu"), TEXT("WBP_SettingsMenu"), TEXT("settings menu"), &BuildSettingsMenu, nullptr },
        { TEXT("create_loading_screen"), TEXT("WBP_LoadingScreen"), TEXT("loading screen"), &BuildLoadingScreen, &FinishLoadingScreen },
        { TEXT("create_hud_widget"), TEXT("WBP_HUD"), TEXT("HUD"), &BuildHud, &FinishHud },
    };
    return Screens;
}
}
