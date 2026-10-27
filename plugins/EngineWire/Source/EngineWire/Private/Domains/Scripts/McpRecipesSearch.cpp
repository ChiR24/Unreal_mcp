// The saved recipes the gateway's search lists beside its capabilities (S14; see McpRecipes.h). Search is answered on
// the socket thread, so this reads files and JSON only: no editor API.
#include "Domains/Scripts/McpRecipes.h"

#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "MCP/Gateway/McpNativeGatewaySearchMatch.h"
#include "Misc/Paths.h"

namespace McpRecipes
{
TArray<TSharedPtr<FJsonValue>> SearchRows(const TArray<FString>& ContentWords, int32 Max)
{
    struct FFound
    {
        int32 Matched = 0;
        FString Name;
        FString Summary;
        TSharedPtr<FJsonObject> Recipe;
    };
    const TCHAR* const Copied[] = {TEXT("paramNames"), TEXT("verified")};
    TArray<TSharedPtr<FJsonValue>> Rows;
    if (ContentWords.Num() == 0)
    {
        return Rows;
    }
    // ponytail: every search reads every recipe file; an index kept from save_recipe if a project keeps hundreds.
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir(), TEXT("*.json")), true, false);
    TArray<FFound> Found;
    for (const FString& File : Files)
    {
        FFound Entry;
        Entry.Name = FPaths::GetBaseFilename(File);
        Entry.Recipe = Load(Entry.Name); // null while save_recipe is writing it
        if (!Entry.Recipe.IsValid() || !Entry.Recipe->TryGetStringField(TEXT("summary"), Entry.Summary))
        {
            continue;
        }
        TArray<FString> Words; // "lift_crates" splits at the underscore, and "crates" folds to "crate"
        McpSearchWords(Entry.Name + TEXT(" ") + Entry.Summary, Words);
        for (const FString& Word : ContentWords)
        {
            Entry.Matched += Words.Contains(Word) ? 1 : 0;
        }
        if (Entry.Matched == ContentWords.Num() || Entry.Matched >= 2)
        {
            Found.Add(MoveTemp(Entry));
        }
    }
    Found.Sort([](const FFound& L, const FFound& R)
               { return L.Matched != R.Matched ? L.Matched > R.Matched : L.Name.Compare(R.Name, ESearchCase::CaseSensitive) < 0; });
    for (int32 Index = 0; Index < Found.Num() && Index < Max; ++Index)
    {
        const TSharedPtr<FJsonObject>& Recipe = Found[Index].Recipe;
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Found[Index].Name);
        Row->SetStringField(TEXT("summary"), Found[Index].Summary);
        for (const TCHAR* Field : Copied)
        {
            if (const TSharedPtr<FJsonValue> Value = Recipe->TryGetField(Field))
            {
                Row->SetField(Field, Value);
            }
        }
        // run_recipe asks for consent, so the next step is describe (it hands over the grant), never a bare execute.
        TSharedPtr<FJsonObject> Next = MakeShared<FJsonObject>();
        Next->SetStringField(TEXT("operation"), TEXT("describe"));
        Next->SetStringField(TEXT("capability"), TEXT("system_control.run_recipe"));
        Row->SetObjectField(TEXT("nextCall"), Next);
        Rows.Add(MakeShared<FJsonValueObject>(Row));
    }
    return Rows;
}
} // namespace McpRecipes
