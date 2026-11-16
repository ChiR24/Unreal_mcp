#include "Domains/BlueprintGraph/Code/McpGraphCode.h"

#include "EdGraphSchema_K2.h"
#include "EditorCategoryUtils.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace McpGraphCode
{
// A menu category as an English editor shows it, whatever language this editor shows, so one menu id names the same
// node in every editor. A localized text keeps its source (BuildSourceString), but the editor's common categories
// (Input, Utilities, Flow Control...) and the Default variable category reach a node as display strings only
// (FEditorCategoryUtils keeps them as FText::FromString), so each |-separated part showing one of them is put back
// to the name an English editor shows for it.
FString InvariantCategory(const FText& Category)
{
    // What an English editor shows for each common category: the LOCTEXT sources EditorCategoryUtils.cpp registers
    // (Gameplay shows as Game, Delegates as Event Dispatchers), the last part of the ones that are paths (FlowControl
    // is Utilities|Flow Control), spaces removed. Each is named with its value, so a renamed value fails to compile
    // instead of mapping the wrong name.
    using ECategory = FCommonEditorCategory::EValue;
    static const TPair<ECategory, const TCHAR*> English[] = {{ECategory::AI, TEXT("AI")},
        {ECategory::Animation, TEXT("Animation")}, {ECategory::Audio, TEXT("Audio")},
        {ECategory::Development, TEXT("Development")}, {ECategory::Effects, TEXT("Effects")},
        {ECategory::Gameplay, TEXT("Game")}, {ECategory::Input, TEXT("Input")}, {ECategory::Math, TEXT("Math")},
        {ECategory::Networking, TEXT("Networking")}, {ECategory::Pawn, TEXT("Pawn")},
        {ECategory::Physics, TEXT("Physics")}, {ECategory::Rendering, TEXT("Rendering")},
        {ECategory::Transformation, TEXT("Transformation")}, {ECategory::Utilities, TEXT("Utilities")},
        {ECategory::FlowControl, TEXT("FlowControl")}, {ECategory::UserInterface, TEXT("UserInterface")},
        {ECategory::AnimNotify, TEXT("AddAnimNotifyEvent")},
        {ECategory::BranchPoint, TEXT("AddMontageBranchingPointEvent")}, {ECategory::String, TEXT("String")},
        {ECategory::Text, TEXT("Text")}, {ECategory::Name, TEXT("Name")}, {ECategory::Enum, TEXT("Enum")},
        {ECategory::Struct, TEXT("Struct")}, {ECategory::Macro, TEXT("Macro")},
        {ECategory::Delegates, TEXT("EventDispatchers")}, {ECategory::Class, TEXT("Class")},
        {ECategory::Variables, TEXT("Variables")}};
    static TMap<FString, FString> Shown; // what this editor shows, spaces removed -> what an English editor shows
    static FString ShownCulture;
    const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
    if (Shown.Num() == 0 || ShownCulture != Culture)
    {
        Shown.Reset();
        ShownCulture = Culture;
        for (const TPair<ECategory, const TCHAR*>& Common : English)
        {
            FString Display = FEditorCategoryUtils::GetCommonCategory(Common.Key).ToString().Replace(TEXT(" "), TEXT(""));
            int32 Bar = INDEX_NONE;
            Display = Display.FindLastChar(TEXT('|'), Bar) ? Display.Mid(Bar + 1) : Display;
            if (!Display.IsEmpty()) // a category the editor never registered (Physics) shows nothing
            {
                Shown.Add(Display, Common.Value);
            }
        }
        Shown.Add(UEdGraphSchema_K2::VR_DefaultCategory.ToString().Replace(TEXT(" "), TEXT("")), TEXT("Default"));
    }
    TArray<FString> Parts;
    Category.BuildSourceString().ParseIntoArray(Parts, TEXT("|"), false);
    for (FString& Part : Parts)
    {
        if (const FString* Name = Shown.Find(Part.Replace(TEXT(" "), TEXT(""))))
        {
            Part = *Name;
        }
    }
    return FString::Join(Parts, TEXT("|"));
}
} // namespace McpGraphCode
