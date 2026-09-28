// Recipe text -> checked plan: placeholders, the recipe's own fields, step ids,
// and the order the steps run in. Recipes are data files under the plugin's
// Resources/Recipes (a TEXT() literal of that size breaks MSVC's C2026 limit).
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace McpBlueprintBehaviour
{
namespace
{
bool IsRecipeToken(const FString& Text)
{
    for (const TCHAR Char : Text)
    {
        if (!FChar::IsAlnum(Char) && Char != TEXT('_'))
        {
            return false;
        }
    }
    return !Text.IsEmpty();
}

// "{{Name}}" is replaced by Values[Name] as a whole JSON value; any other use of
// braces is a recipe bug, so it fails instead of leaving "{{" in an asset path.
bool FillPlaceholders(TSharedPtr<FJsonValue>& Value, const TMap<FString, TSharedPtr<FJsonValue>>& Values,
                      FString& OutError)
{
    if (!Value.IsValid())
    {
        return true;
    }
    if (Value->Type == EJson::String)
    {
        const FString Text = Value->AsString();
        if (!Text.Contains(TEXT("{{")) && !Text.Contains(TEXT("}}")))
        {
            return true;
        }
        const FString Name = Text.Mid(2, Text.Len() - 4);
        const TSharedPtr<FJsonValue>* Found = Values.Find(Name);
        if (!Text.StartsWith(TEXT("{{")) || !Text.EndsWith(TEXT("}}")) || !IsRecipeToken(Name))
        {
            OutError = FString::Printf(TEXT("'%s' is not a placeholder: a placeholder is a whole string \"{{Name}}\"."), *Text);
            return false;
        }
        if (!Found || !Found->IsValid())
        {
            OutError = FString::Printf(TEXT("placeholder {{%s}} has no value; pass one (an empty string for a section "
                                            "the handler removes)."), *Name);
            return false;
        }
        Value = *Found;
        return true;
    }
    if (Value->Type == EJson::Array)
    {
        TArray<TSharedPtr<FJsonValue>> Items = Value->AsArray();
        for (TSharedPtr<FJsonValue>& Item : Items)
        {
            if (!FillPlaceholders(Item, Values, OutError))
            {
                return false;
            }
        }
        Value = MakeShared<FJsonValueArray>(Items);
        return true;
    }
    if (Value->Type != EJson::Object)
    {
        return true;
    }
    const TSharedPtr<FJsonObject> Object = Value->AsObject();
    TArray<FString> Keys;
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
    {
        Keys.Add(Pair.Key);
    }
    for (const FString& Key : Keys)
    {
        TSharedPtr<FJsonValue> Field = Object->TryGetField(Key);
        if (!FillPlaceholders(Field, Values, OutError))
        {
            return false;
        }
        Object->SetField(Key, Field);
    }
    return true;
}
} // namespace

TSharedPtr<FJsonObject> ParseRecipe(const FString& RecipeJson, const TMap<FString, TSharedPtr<FJsonValue>>& Values,
                                    FString& OutError)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RecipeJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("The recipe is not a JSON object.");
        return nullptr;
    }
    TSharedPtr<FJsonValue> Value = MakeShared<FJsonValueObject>(Root);
    if (!FillPlaceholders(Value, Values, OutError))
    {
        OutError = TEXT("Recipe: ") + OutError;
        return nullptr;
    }
    return Root;
}

TSharedPtr<FJsonObject> LoadRecipe(const FString& Domain, const FString& Name,
                                   const TMap<FString, TSharedPtr<FJsonValue>>& Values, FString& OutError)
{
    if (!IsRecipeToken(Domain) || !IsRecipeToken(Name))
    {
        OutError = TEXT("A recipe domain and name are letters, digits and underscores.");
        return nullptr;
    }
    // ponytail: text cached for the editor session; an edited recipe file needs an editor restart.
    static TMap<FString, FString> Cache;
    const FString Key = Domain + TEXT(".") + Name;
    if (!Cache.Contains(Key))
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("McpAutomationBridge"));
        const FString File = Plugin.IsValid()
            ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("Recipes"), Domain, Name + TEXT(".json"))
            : FString();
        FString Text;
        if (File.IsEmpty() || !FFileHelper::LoadFileToString(Text, *File))
        {
            OutError = FString::Printf(TEXT("Recipe '%s' of domain %s is missing: no file at %s."), *Name, *Domain, *File);
            return nullptr;
        }
        Cache.Add(Key, Text);
    }
    return ParseRecipe(Cache[Key], Values, OutError);
}

namespace Detail
{
bool CheckKeys(const TSharedPtr<FJsonObject>& Object, std::initializer_list<const TCHAR*> Allowed, const FString& Where,
               FString& OutError)
{
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
    {
        bool bKnown = false;
        FString List;
        for (const TCHAR* Key : Allowed)
        {
            bKnown |= Pair.Key == Key;
            List += (List.IsEmpty() ? TEXT("") : TEXT(", ")) + FString(Key);
        }
        if (!bKnown)
        {
            OutError = FString::Printf(TEXT("%s has an unknown field '%s'. Its fields: %s."), *Where, *Pair.Key, *List);
            return false;
        }
    }
    return true;
}

bool ObjectsIn(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<TSharedPtr<FJsonObject>>& Out,
               FString& OutError)
{
    const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
    if (!Object->HasField(Field))
    {
        return true;
    }
    if (!Object->TryGetArrayField(Field, Items))
    {
        OutError = FString::Printf(TEXT("'%s' must be an array."), Field);
        return false;
    }
    for (int32 Index = 0; Index < Items->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Item = nullptr;
        if (!(*Items)[Index].IsValid() || !(*Items)[Index]->TryGetObject(Item))
        {
            OutError = FString::Printf(TEXT("%s[%d] must be an object."), Field, Index);
            return false;
        }
        Out.Add(*Item);
    }
    return true;
}

TSharedPtr<FJsonObject> CopyObject(const TSharedPtr<FJsonObject>& Source)
{
    TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Source->Values)
    {
        Copy->SetField(Pair.Key, Pair.Value);
    }
    return Copy;
}

void AddOp(FPlan& Plan, const TSharedPtr<FJsonObject>& Step, const FString& Label)
{
    Plan.Ops.Add(MakeShared<FJsonValueObject>(Step));
    Plan.Labels.Add(Label);
}

bool BuildPlan(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, UBlueprint* Blueprint,
               const TSharedPtr<FJsonObject>& Recipe, FPlan& Plan, const TSharedPtr<FJsonObject>& Report,
               FString& OutError, FString& OutCode)
{
    OutCode = TEXT("INVALID_RECIPE");
    // Keep this list, and the section lists of PlanMembers, CheckHooks and PlanInput, in step with
    // tests/unit/plugin/behaviour_recipes.test.ts, which checks every recipe file against them.
    if (!CheckKeys(Recipe, {TEXT("description"), TEXT("tag"), TEXT("replace"), TEXT("placeholders"), TEXT("variables"),
                            TEXT("dispatchers"), TEXT("customEvents"), TEXT("functions"), TEXT("hooks"), TEXT("input"),
                            TEXT("eventGraph")},
                   TEXT("The recipe"), OutError))
    {
        return false;
    }
    Plan.Tag = GetJsonStringField(Recipe, TEXT("tag")).TrimStartAndEnd();
    Plan.bReplace = GetJsonBoolField(Recipe, TEXT("replace"), true);
    Report->SetStringField(TEXT("tag"), Plan.Tag);
    if (Plan.Tag.IsEmpty())
    {
        OutError = TEXT("The recipe has no tag: the owner name every node it makes is marked with.");
        return false;
    }
    TArray<TSharedPtr<FJsonObject>> EventGraph;
    if (!PlanMembers(Blueprint, Recipe, Plan, OutError, OutCode) || !ObjectsIn(Recipe, TEXT("hooks"), Plan.Hooks, OutError) ||
        !PlanInput(Blueprint, Recipe, Plan, Report, OutError, OutCode) ||
        !ObjectsIn(Recipe, TEXT("eventGraph"), EventGraph, OutError))
    {
        return false;
    }
    for (int32 Index = 0; Index < EventGraph.Num(); ++Index)
    {
        AddOp(Plan, CopyObject(EventGraph[Index]), FString::Printf(TEXT("eventGraph[%d]"), Index));
    }
    OutCode = TEXT("INVALID_RECIPE");
    if (Plan.Ops.Num() > MaxAuthorSteps)
    {
        OutError = FString::Printf(TEXT("The recipe flattens to %d steps; Author runs at most %d."), Plan.Ops.Num(),
                                   MaxAuthorSteps);
        return false;
    }
    if (!CheckRecipeIds(Plan, OutError) || !CheckHooks(Blueprint, Plan, OutError, OutCode))
    {
        return false;
    }
    const FString InUse = Plan.bReplace ? CheckRemovable(Blueprint, Plan.Tag, Plan.Rebuilt) : FString();
    if (!InUse.IsEmpty())
    {
        OutCode = TEXT("BEHAVIOUR_IN_USE");
        OutError = InUse;
        return false;
    }
    // Last: the only step of planning that writes anything (input assets).
    return CreateInputAssets(Bridge, RequestId, Plan, OutError, OutCode);
}
} // namespace Detail
} // namespace McpBlueprintBehaviour
