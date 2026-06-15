#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace UnrealAgent::OpenCodeAcp
{
namespace
{
    bool IsCommandFieldName(const FString& FieldName)
    {
        const FString LowerFieldName = FieldName.ToLower();
        return LowerFieldName.Contains(TEXT("command"))
            || LowerFieldName.Contains(TEXT("cmd"))
            || LowerFieldName.Contains(TEXT("script"))
            || LowerFieldName.Contains(TEXT("arg"))
            || LowerFieldName == TEXT("binary")
            || LowerFieldName == TEXT("executable")
            || LowerFieldName == TEXT("program")
            || LowerFieldName == TEXT("run")
            || LowerFieldName == TEXT("exec")
            || LowerFieldName == TEXT("execute")
            || LowerFieldName == TEXT("shell")
            || LowerFieldName == TEXT("invocation")
            || LowerFieldName == TEXT("invoke");
    }

    int32 CommandFieldPriority(FString FieldName)
    {
        FieldName.ToLowerInline();
        FieldName.ReplaceInline(TEXT("-"), TEXT(""));
        FieldName.ReplaceInline(TEXT("_"), TEXT(""));
        if (FieldName == TEXT("binary")
            || FieldName == TEXT("cmd")
            || FieldName == TEXT("command")
            || FieldName == TEXT("exec")
            || FieldName == TEXT("executable")
            || FieldName == TEXT("execute")
            || FieldName == TEXT("invocation")
            || FieldName == TEXT("invoke")
            || FieldName == TEXT("program")
            || FieldName == TEXT("run"))
        {
            return 0;
        }
        if (FieldName.Contains(TEXT("arg")))
        {
            return 1;
        }
        return FieldName == TEXT("script") || FieldName == TEXT("shell") ? 2 : 3;
    }

    void CollectCommandFragments(
        const TSharedPtr<FJsonValue>& Value,
        const bool bCommandContext,
        TArray<FString>& OutFragments)
    {
        if (!Value.IsValid())
        {
            return;
        }
        if (Value->Type == EJson::String)
        {
            if (bCommandContext)
            {
                OutFragments.Add(Value->AsString());
            }
            return;
        }
        if (Value->Type == EJson::Array)
        {
            for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
            {
                CollectCommandFragments(Element, bCommandContext, OutFragments);
            }
            return;
        }
        if (Value->Type != EJson::Object || !Value->AsObject().IsValid())
        {
            return;
        }

        TArray<TPair<FString, TSharedPtr<FJsonValue>>> Fields;
        Fields.Reserve(Value->AsObject()->Values.Num());
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Value->AsObject()->Values)
        {
            Fields.Add(Field);
        }
        Fields.Sort([](
            const TPair<FString, TSharedPtr<FJsonValue>>& Left,
            const TPair<FString, TSharedPtr<FJsonValue>>& Right)
        {
            const int32 LeftPriority = CommandFieldPriority(Left.Key);
            const int32 RightPriority = CommandFieldPriority(Right.Key);
            return LeftPriority == RightPriority
                ? Left.Key < Right.Key
                : LeftPriority < RightPriority;
        });
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Fields)
        {
            CollectCommandFragments(
                Field.Value,
                bCommandContext || IsCommandFieldName(Field.Key),
                OutFragments);
        }
    }

    void CollectAllStringFragments(
        const TSharedPtr<FJsonValue>& Value,
        TArray<FString>& OutFragments)
    {
        if (!Value.IsValid())
        {
            return;
        }
        if (Value->Type == EJson::String)
        {
            OutFragments.Add(Value->AsString());
            return;
        }
        if (Value->Type == EJson::Array)
        {
            for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
            {
                CollectAllStringFragments(Element, OutFragments);
            }
            return;
        }
        if (Value->Type != EJson::Object || !Value->AsObject().IsValid())
        {
            return;
        }
        TArray<FString> Keys;
        Value->AsObject()->Values.GetKeys(Keys);
        Keys.Sort();
        for (const FString& Key : Keys)
        {
            CollectAllStringFragments(
                Value->AsObject()->Values.FindChecked(Key),
                OutFragments);
        }
    }
}

FString GetLocalCommandText(const TSharedPtr<FJsonValue>& RawInputValue)
{
    TArray<FString> Fragments;
    const bool bRootCommandContext =
        RawInputValue.IsValid() && RawInputValue->Type != EJson::Object;
    CollectCommandFragments(RawInputValue, bRootCommandContext, Fragments);
    return FString::Join(Fragments, TEXT(" "));
}

FString GetPotentialLocalCommandText(
    const TSharedPtr<FJsonValue>& RawInputValue)
{
    const FString StructuredCommand = GetLocalCommandText(RawInputValue);
    TArray<FString> AllFragments;
    CollectAllStringFragments(RawInputValue, AllFragments);

    TArray<FString> Fragments;
    if (!StructuredCommand.IsEmpty())
    {
        Fragments.Add(StructuredCommand);
    }
    for (const FString& Fragment : AllFragments)
    {
        if (Fragment != StructuredCommand)
        {
            Fragments.Add(Fragment);
        }
    }
    return FString::Join(Fragments, TEXT("\n"));
}

bool ContainsAliasedPythonImport(
    const FString& LowerValue,
    const TCHAR* const* ImportedNames,
    const int32 ImportedNameCount)
{
    FString Tokenizable = LowerValue;
    for (const TCHAR Separator : FString(TEXT("(),\\\"'")))
    {
        Tokenizable.ReplaceCharInline(Separator, TEXT(' '));
    }
    TArray<FString> Tokens;
    Tokenizable.ParseIntoArrayWS(Tokens);

    bool bSawFrom = false;
    bool bInImportList = false;
    for (int32 Index = 0; Index < Tokens.Num(); ++Index)
    {
        const FString& Token = Tokens[Index];
        if (Token == TEXT("from"))
        {
            bSawFrom = true;
            bInImportList = false;
            continue;
        }
        if (bSawFrom && Token == TEXT("import"))
        {
            bInImportList = true;
            continue;
        }
        if (!bInImportList
            || Index + 1 >= Tokens.Num()
            || Tokens[Index + 1] != TEXT("as"))
        {
            continue;
        }
        for (int32 NameIndex = 0;
            NameIndex < ImportedNameCount;
            ++NameIndex)
        {
            if (Token == ImportedNames[NameIndex])
            {
                return true;
            }
        }
    }
    return false;
}
}
