#include "Domains/BlueprintGraph/Code/McpGraphCode.h"

#include "EdGraphSchema_K2.h"
#include "EditorCategoryUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"

namespace McpGraphCode
{
// The Category a function or variable declares for itself (a Blueprint's own, a game module's): the editor shows it as
// written in every language, so a part it names is the user's text even when it reads like a translated common
// category (a variable in "Juego" is not Game). The one exception is the Default category, which the Blueprint
// compiler writes from its text in the editor's language (Predeterminado in Spanish): InvariantCategory still maps it.
FString MemberCategory(const UFunction* Function, const FProperty* Property)
{
    return Function ? Function->GetMetaData(FBlueprintMetadata::MD_FunctionCategory)
                    : Property ? Property->GetMetaData(TEXT("Category")) : FString();
}

FString MemberCategoryOf(const UEdGraphNode* Node)
{
    const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
    const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node);
    return MemberCategory(Call ? Call->GetTargetFunction() : nullptr, Variable ? Variable->GetPropertyForVariable() : nullptr);
}

// A menu category as an English editor shows it, whatever language this editor shows, so one menu id names the same
// node in every editor. A localized text keeps its source (BuildSourceString), but the editor's common categories
// (Input, Utilities, Flow Control...) and the Default variable category reach a node as display strings only
// (FEditorCategoryUtils keeps them as FText::FromString), so each |-separated part showing one of them is put back
// to the name an English editor shows for it, unless the member's own Category names that part.
FString InvariantCategory(const FText& Category, const FString& OwnCategory)
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
    const FString DefaultShown = UEdGraphSchema_K2::VR_DefaultCategory.ToString().Replace(TEXT(" "), TEXT(""));
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
        Shown.Add(DefaultShown, TEXT("Default"));
    }
    TArray<FString> Own; // FString's == ignores case, as the display string capitalizes what the user typed
    OwnCategory.Replace(TEXT(" "), TEXT("")).ParseIntoArray(Own, TEXT("|"));
    Own.Remove(DefaultShown);
    TArray<FString> Parts;
    Category.BuildSourceString().ParseIntoArray(Parts, TEXT("|"), false);
    for (FString& Part : Parts)
    {
        const FString Bare = Part.Replace(TEXT(" "), TEXT(""));
        if (const FString* Name = Own.Contains(Bare) ? nullptr : Shown.Find(Bare))
        {
            Part = *Name;
        }
    }
    return FString::Join(Parts, TEXT("|"));
}
} // namespace McpGraphCode
