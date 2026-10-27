// system_control save_recipe / run_recipe / list_recipes (S14; see McpRecipes.h).
#include "Domains/Scripts/McpRecipes.h"

#include "Domains/Scripts/McpScript.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace McpRecipes
{
FString Dir()
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EngineWire"), TEXT("Recipes"));
}

namespace RecipeParts
{
constexpr int32 Schema = 1;
constexpr int32 MaxParams = 16;

// A recipe's name is its file name: letters, digits and underscores only, so it can never leave the folder.
bool ValidName(const FString& Name)
{
    if (Name.IsEmpty() || Name.Len() > 64)
    {
        return false;
    }
    for (const TCHAR Char : Name)
    {
        if (!FChar::IsAlnum(Char) && Char != TEXT('_'))
        {
            return false;
        }
    }
    return true;
}

FString PathOf(const FString& Name)
{
    return FPaths::Combine(Dir(), Name + TEXT(".json"));
}

bool Store(const FString& Name, const TSharedPtr<FJsonObject>& Recipe)
{
    FString Text;
    FJsonSerializer::Serialize(Recipe.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
    IFileManager::Get().MakeDirectory(*Dir(), true);
    return FFileHelper::SaveStringToFile(Text, *PathOf(Name));
}

// A JSON value as the script language writes it: numbers and true/false as they are, text quoted, arrays as (list ...),
// objects as (obj :key value ...).
FString ToScript(const TSharedPtr<FJsonValue>& Value)
{
    switch (Value.IsValid() ? Value->Type : EJson::Null)
    {
    case EJson::Number:
        return FString::SanitizeFloat(Value->AsNumber());
    case EJson::Boolean:
        return Value->AsBool() ? TEXT("true") : TEXT("false");
    case EJson::String:
        return TEXT("\"") + Value->AsString().Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")) + TEXT("\"");
    case EJson::Array:
    {
        FString Out = TEXT("(list");
        for (const TSharedPtr<FJsonValue>& Item : Value->AsArray())
        {
            Out += TEXT(" ") + ToScript(Item);
        }
        return Out + TEXT(")");
    }
    case EJson::Object:
    {
        FString Out = TEXT("(obj");
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Value->AsObject()->Values)
        {
            Out += FString::Printf(TEXT(" :%s %s"), *FString(Field.Key), *ToScript(Field.Value));
        }
        return Out + TEXT(")");
    }
    default:
        return TEXT("0");
    }
}

// (let name value) for each parameter, all on one line placed before the script's first line, so its line numbers hold.
FString Bound(const TArray<FString>& Params, const TSharedPtr<FJsonObject>& Args)
{
    FString Out;
    for (const FString& Param : Params)
    {
        const TSharedPtr<FJsonValue> Value = Args.IsValid() ? Args->TryGetField(Param) : nullptr;
        Out += FString::Printf(TEXT("(let %s %s) "), *Param, Value.IsValid() ? *ToScript(Value) : TEXT("0"));
    }
    return Out;
}
} // namespace RecipeParts

using namespace RecipeParts;

TSharedPtr<FJsonObject> Load(const FString& Name)
{
    FString Text;
    TSharedPtr<FJsonObject> Recipe;
    if (ValidName(Name) && FFileHelper::LoadFileToString(Text, *PathOf(Name)))
    {
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe);
    }
    return Recipe;
}

bool HandleSave(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                const TSharedPtr<FMcpBridgeWebSocket>& Socket)
{
    auto Refuse = [&](const TCHAR* Code, const FString& Message)
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    const FString Script = GetJsonStringField(Payload, TEXT("script"));
    const TArray<FString> Params = McpHandlerUtils::GetStringArrayField(Payload, TEXT("paramNames"));
    if (!ValidName(Name) || Params.Num() > MaxParams || Params.ContainsByPredicate([](const FString& P) { return !ValidName(P); }))
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), TEXT("name and each of paramNames are 1-64 letters, digits or underscores (at most 16 of them)."));
    }
    if (GetJsonBoolField(Payload, TEXT("remove"), false))
    {
        if (!IFileManager::Get().Delete(*PathOf(Name), true, false, true)) // RequireExists: a missing file read as deleted
        {
            return Refuse(TEXT("RECIPE_NOT_FOUND"), FString::Printf(TEXT("No recipe is named %s."), *Name));
        }
        TSharedPtr<FJsonObject> Removed = McpHandlerUtils::CreateResultObject();
        Removed->SetStringField(TEXT("name"), Name);
        Removed->SetBoolField(TEXT("removed"), true);
        Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Recipe %s removed."), *Name), Removed);
        return true;
    }
    if (Script.TrimStartAndEnd().IsEmpty())
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), TEXT("script is the recipe to save (only remove goes without one)."));
    }
    if (FPaths::FileExists(PathOf(Name)) && !GetJsonBoolField(Payload, TEXT("overwrite"), false))
    {
        return Refuse(TEXT("RECIPE_EXISTS"), FString::Printf(TEXT("A recipe named %s exists: pass overwrite true to replace it."), *Name));
    }
    // The same check run_script makes, the parameters bound (their values do not matter to it).
    TArray<McpGraphCode::FSExpr> Forms;
    FString Error;
    FString Code;
    TSet<FString> Allowed;
    for (const FString& Id : McpHandlerUtils::GetStringArrayField(Payload, TEXT("destructive")))
    {
        const FString Resolved = McpScript::ResolveCapability(Id);
        if (Resolved.IsEmpty())
        {
            return Refuse(TEXT("SCRIPT_INVALID"), FString::Printf(TEXT("destructive names %s, which is no capability."), *Id));
        }
        Allowed.Add(Resolved);
    }
    if (!McpGraphCode::ParseSExpr(Bound(Params, nullptr) + Script, Forms, Error) || Forms.Num() == 0)
    {
        return Refuse(TEXT("SCRIPT_PARSE_ERROR"), Error.IsEmpty() ? FString(TEXT("The script holds no statement.")) : Error);
    }
    if (!McpScript::Check(Forms, Allowed, Error, Code))
    {
        return Refuse(*Code, Error);
    }
    TSharedPtr<FJsonObject> Recipe = MakeShared<FJsonObject>();
    Recipe->SetNumberField(TEXT("schema"), Schema);
    Recipe->SetStringField(TEXT("name"), Name);
    Recipe->SetStringField(TEXT("summary"), GetJsonStringField(Payload, TEXT("summary")));
    Recipe->SetArrayField(TEXT("paramNames"), McpHandlerUtils::ToJsonStringArray(Params));
    Recipe->SetArrayField(TEXT("destructive"), McpHandlerUtils::ToJsonStringArray(Allowed.Array()));
    Recipe->SetStringField(TEXT("script"), Script);
    if (!Store(Name, Recipe))
    {
        return Refuse(TEXT("RECIPE_WRITE_FAILED"), FString::Printf(TEXT("The recipe could not be written to %s."), *PathOf(Name)));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("name"), Name);
    Result->SetStringField(TEXT("path"), FPaths::ConvertRelativePathToFull(PathOf(Name)));
    Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Recipe %s saved; run_recipe runs it."), *Name), Result);
    return true;
}

bool HandleRun(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
               const TSharedPtr<FMcpBridgeWebSocket>& Socket)
{
    const FString Name = GetJsonStringField(Payload, TEXT("name"));
    const TSharedPtr<FJsonObject> Recipe = Load(Name);
    if (!Recipe.IsValid())
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("No recipe is named %s: list_recipes lists them."), *Name),
                                      nullptr, TEXT("RECIPE_NOT_FOUND"));
        return true;
    }
    const TArray<FString> Params = McpHandlerUtils::GetStringArrayField(Recipe, TEXT("paramNames"));
    const TSharedPtr<FJsonObject>* Args = nullptr;
    Payload->TryGetObjectField(TEXT("args"), Args);
    TArray<FString> Missing;
    for (const FString& Param : Params)
    {
        if (!Args || !(*Args)->HasField(Param))
        {
            Missing.Add(Param);
        }
    }
    if (Missing.Num() > 0)
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, FString::Printf(TEXT("args lacks %s."), *FString::Join(Missing, TEXT(", "))),
                                      nullptr, TEXT("RECIPE_ARGS"));
        return true;
    }
    // Run exactly as run_script runs a script: the same parse, check, per-step gate, consent list and rollback.
    TSharedPtr<FJsonObject> Run = MakeShared<FJsonObject>();
    // A recipe without parameters runs without args: Args is then null.
    Run->SetStringField(TEXT("script"), Bound(Params, Args ? *Args : nullptr) + GetJsonStringField(Recipe, TEXT("script")));
    Run->SetArrayField(TEXT("destructive"), McpHandlerUtils::ToJsonStringArray(McpHandlerUtils::GetStringArrayField(Recipe, TEXT("destructive"))));
    for (const TCHAR* Limit : {TEXT("maxSteps"), TEXT("deadlineMs")})
    {
        double Value = 0.0;
        if (Payload->TryGetNumberField(Limit, Value))
        {
            Run->SetNumberField(Limit, Value);
        }
    }
    bool bRan = false;
    McpScript::HandleRunScript(Bridge, RequestId, Run, Socket, &bRan);
    if (bRan)
    {
        TSharedPtr<FJsonObject> Verified = MakeShared<FJsonObject>();
        Verified->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString(EVersionComponent::Patch));
        Verified->SetStringField(TEXT("at"), FDateTime::UtcNow().ToIso8601());
        Recipe->SetObjectField(TEXT("verified"), Verified);
        Store(Name, Recipe);
    }
    return true;
}

bool HandleList(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                const TSharedPtr<FMcpBridgeWebSocket>& Socket)
{
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir(), TEXT("*.json")), true, false);
    TArray<FString> Words;
    GetJsonStringField(Payload, TEXT("query")).ToLower().ParseIntoArrayWS(Words);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for (const FString& File : Files)
    {
        const TSharedPtr<FJsonObject> Recipe = Load(FPaths::GetBaseFilename(File));
        const FString Text = Recipe.IsValid() ? (GetJsonStringField(Recipe, TEXT("name")) + TEXT(" ") + GetJsonStringField(Recipe, TEXT("summary"))).ToLower() : FString();
        if (Recipe.IsValid() && !Words.ContainsByPredicate([&Text](const FString& Word) { return !Text.Contains(Word); }))
        {
            Recipe->RemoveField(TEXT("script"));
            Rows.Add(MakeShared<FJsonValueObject>(Recipe));
        }
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("recipes"), Rows);
    Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("%d recipe(s)."), Rows.Num()), Result);
    return true;
}
} // namespace McpRecipes
