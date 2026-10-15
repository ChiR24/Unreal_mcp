#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UWidgetBlueprint;

// create_widget_template: each screen is a spec builder (payload -> a RootCanvas spec,
// see ...Spec.h) plus an optional step that runs once the tree exists.
namespace WidgetAuthoringScreens
{
using FBuildScreen = TSharedPtr<FJsonObject> (*)(const TSharedPtr<FJsonObject>& Payload, FString& OutError);
using FFinishScreen = void (*)(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Payload,
                               const TSharedPtr<FJsonObject>& Result);

struct FScreenTemplate
{
    const TCHAR* Action;
    const TCHAR* DefaultName;
    const TCHAR* Label;
    FBuildScreen Build;
    FFinishScreen Finish;
};

// {type, name} plus optional text; the building blocks every screen is written in.
TSharedPtr<FJsonObject> Node(const TCHAR* Type, const FString& Name, const FString& Text = FString());
TSharedPtr<FJsonObject> Append(const TSharedPtr<FJsonObject>& Parent, const TSharedPtr<FJsonObject>& Child);
TSharedPtr<FJsonObject> SetSlot(const TSharedPtr<FJsonObject>& Target, const TCHAR* Json);
TSharedPtr<FJsonObject> Numbers(const TSharedPtr<FJsonObject>& Target, const TCHAR* Field, std::initializer_list<double> Values);
// A dark rounded button whose label is Name + "_Text".
TSharedPtr<FJsonObject> Button(const FString& Name, const FString& Label, double FontSize = 22.0);
// "Main Menu" -> "MainMenu": a label turned into a widget-name stem.
FString NameStem(const FString& Label);
// A full-screen near-black backdrop; every screen starts from one so it reads on its own.
TSharedPtr<FJsonObject> RootWithBackdrop(double Alpha);

// The screen table, split by file: menus (main, pause, settings, loading, HUD) and panels
// (dialog, inventory, radial, credits, shop, in ...ScreensMore.cpp).
const TArray<FScreenTemplate>& MenuScreens();
const TArray<FScreenTemplate>& PanelScreens();
}
