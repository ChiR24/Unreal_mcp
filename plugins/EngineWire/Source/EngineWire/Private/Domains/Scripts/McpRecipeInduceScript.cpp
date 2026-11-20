// propose_recipe's script (McpRecipeInduce.h): the two rules of workflow induction over the calls in order.

#include "Domains/Scripts/McpRecipeInduce.h"

#include "Domains/Scripts/McpRecipes.h"
#include "Domains/Scripts/McpScript.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "MCP/Execute/McpNativeGatewayCanonicalRecords.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

namespace McpRecipeInduceDetail
{
constexpr int32 MaxParams = 16; // as save_recipe takes

// A field that shapes the call: what it makes or edits, never which one. It stays as written.
bool IsStructural(const FString& Key)
{
    static const TSet<FString> Words = {TEXT("className"), TEXT("parentClass"), TEXT("classPath"), TEXT("parentClassPath"),
        TEXT("propertyName"), TEXT("pinName"), TEXT("functionName"), TEXT("variableName"), TEXT("variableType"),
        TEXT("nodeType"), TEXT("type"), TEXT("kind"), TEXT("mode"), TEXT("subAction"), TEXT("action"), TEXT("operation")};
    return Words.Contains(Key);
}

// A target path, a name or label, a place or a written value: what another run of the same steps changes.
bool IsInput(const FString& Key)
{
    static const TSet<FString> Words = {TEXT("name"), TEXT("actorName"), TEXT("label"), TEXT("newName"), TEXT("location"),
        TEXT("rotation"), TEXT("scale"), TEXT("value"), TEXT("text"), TEXT("color"), TEXT("path")};
    return !IsStructural(Key) && (Words.Contains(Key) || Key.EndsWith(TEXT("Path"), ESearchCase::CaseSensitive));
}

// Every text in a reply with the script path that reads it (s2.assetPath, s2.actors.0.name), the first one kept.
void Leaves(const TSharedPtr<FJsonValue>& Value, const FString& Path, int32 Depth, TMap<FString, FString>& Out)
{
    if (!Value.IsValid() || Depth > 4)
    {
        return;
    }
    if (Value->Type == EJson::String && Value->AsString().Len() >= 4 && !Out.Contains(Value->AsString()))
    {
        Out.Add(Value->AsString(), Path);
    }
    else if (Value->Type == EJson::Object)
    {
        for (const auto& Field : Value->AsObject()->Values)
        {
            Leaves(Field.Value, Path + TEXT(".") + McpJsonKey(Field.Key), Depth + 1, Out);
        }
    }
    else if (Value->Type == EJson::Array)
    {
        const TArray<TSharedPtr<FJsonValue>>& Items = Value->AsArray();
        for (int32 Index = 0; Index < FMath::Min(Items.Num(), 4); ++Index)
        {
            Leaves(Items[Index], FString::Printf(TEXT("%s.%d"), *Path, Index), Depth + 1, Out);
        }
    }
}

FString Var(int32 Step)
{
    return FString::Printf(TEXT("s%d"), Step + 1);
}

// The text a param of a later call takes from an earlier reply, or null.
const FString* Taken(const FString& Key, const TSharedPtr<FJsonValue>& Value, const TMap<FString, FString>& Produced)
{
    return Value.IsValid() && Value->Type == EJson::String && !IsStructural(Key) ? Produced.Find(Value->AsString()) : nullptr;
}

// Which calls stay: all but a read whose reply no later call takes a value from.
TArray<bool> Kept(const TArray<McpRecipeInduce::FCall>& Calls)
{
    TArray<bool> Keep;
    Keep.Init(false, Calls.Num());
    TMap<FString, FString> Produced;
    for (int32 Step = 0; Step < Calls.Num(); ++Step)
    {
        for (const auto& Field : Calls[Step].Params->Values)
        {
            if (const FString* From = Taken(McpJsonKey(Field.Key), Field.Value, Produced))
            {
                int32 Earlier = INDEX_NONE;
                LexFromString(Earlier, *From->Mid(1, From->Find(TEXT(".")) - 1));
                Keep[FMath::Clamp(Earlier - 1, 0, Step)] = true;
            }
        }
        const FMcpCapabilityRecord* Record = FMcpCanonicalRecordIndex::Get().FindById(Calls[Step].Capability);
        Keep[Step] |= !Record || Record->Effect != TEXT("read");
        if (Calls[Step].Data.IsValid())
        {
            Leaves(MakeShared<FJsonValueObject>(Calls[Step].Data), Var(Step), 0, Produced);
        }
    }
    return Keep;
}
} // namespace McpRecipeInduceDetail

namespace McpRecipeInduce
{
using namespace McpRecipeInduceDetail;

TSharedPtr<FJsonObject> Induce(const TArray<FCall>& Calls)
{
    const TArray<bool> Keep = Kept(Calls);
    TMap<FString, FString> Produced;     // text an earlier kept call returned -> the script path that reads it
    TMap<FString, FString> ParamOfValue; // a value as script text -> the parameter that carries it
    TArray<FString> ParamNames, Lines;
    TArray<FString> Destructive;
    TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
    for (int32 Step = 0; Step < Calls.Num(); ++Step)
    {
        if (!Keep[Step])
        {
            continue;
        }
        const FCall& Call = Calls[Step];
        FString Line = FString::Printf(TEXT("(let %s (%s"), *Var(Step), *Call.Capability);
        for (const auto& Field : Call.Params->Values)
        {
            const FString Key = McpJsonKey(Field.Key);
            const FString Literal = McpRecipes::RecipeParts::ToScript(Field.Value);
            const FString* From = Taken(Key, Field.Value, Produced);
            FString Text = From ? *From : Literal;
            if (!From && IsInput(Key))
            {
                const FString* Param = ParamOfValue.Find(Literal);
                if (!Param && ParamNames.Num() < MaxParams)
                {
                    FString Name = Key;
                    for (int32 Suffix = 2; ParamNames.Contains(Name); ++Suffix)
                    {
                        Name = FString::Printf(TEXT("%s%d"), *Key, Suffix);
                    }
                    ParamNames.Add(Name);
                    Args->SetField(Name, Field.Value);
                    Param = &ParamOfValue.Add(Literal, Name);
                }
                Text = Param ? *Param : Literal;
            }
            Line += FString::Printf(TEXT(" :%s %s"), *Key, *Text);
        }
        Lines.Add(Line + TEXT("))"));
        if (McpScript::AsksConsent(Call.Capability))
        {
            Destructive.AddUnique(Call.Capability);
        }
        if (Call.Data.IsValid())
        {
            Leaves(MakeShared<FJsonValueObject>(Call.Data), Var(Step), 0, Produced);
        }
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("script"), FString::Join(Lines, TEXT("\n")));
    TArray<TSharedPtr<FJsonValue>> Names, Listed;
    for (const FString& Name : ParamNames)
    {
        Names.Add(MakeShared<FJsonValueString>(Name));
    }
    for (const FString& Id : Destructive)
    {
        Listed.Add(MakeShared<FJsonValueString>(Id));
    }
    Result->SetArrayField(TEXT("paramNames"), Names);
    Result->SetObjectField(TEXT("args"), Args);
    Result->SetArrayField(TEXT("destructive"), Listed);
    Result->SetNumberField(TEXT("stepCount"), Lines.Num());
    return Result;
}
} // namespace McpRecipeInduce
