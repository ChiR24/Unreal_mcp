#include "Acp/Validation/UnrealAgentStudioKitAgentPermissionParser.h"

#include "Acp/Validation/UnrealAgentStudioKitAgentPermissionYaml.h"
#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"

#include "Misc/FileHelper.h"

namespace UnrealAgent::Validation
{
namespace
{
void AddUnsafePermissionError(
    FUnrealAgentValidationResult& Result,
    const FString& Key,
    const FString& AgentPath)
{
    Result.bPassed = false;
    Result.Errors.Add(FString::Printf(
        TEXT("Unsafe OpenCode agent auto-allows protected tool pattern '%s': %s"),
        *Key,
        *AgentPath));
}

bool ParsePermissionFlowMap(
    FUnrealAgentValidationResult& Result,
    FString Value,
    const FString& AgentPath)
{
    Value.TrimStartAndEndInline();
    if (!Value.StartsWith(TEXT("{")) || !Value.EndsWith(TEXT("}")))
    {
        return false;
    }
    TArray<FString> Fields;
    Yaml::SplitFlowFields(Value.Mid(1, Value.Len() - 2), Fields);
    for (const FString& Field : Fields)
    {
        FString Key;
        FString PermissionValue;
        if (Yaml::HasUnsupportedKeySyntax(Field)
            || !Yaml::SplitField(Field, Key, PermissionValue))
        {
            return false;
        }
        if (Yaml::HasUnsupportedScalarSyntax(PermissionValue))
        {
            return false;
        }
        if (IsProtectedOpenCodePermissionPattern(Key)
            && Yaml::ContainsAllowValue(PermissionValue))
        {
            AddUnsafePermissionError(Result, Key, AgentPath);
        }
    }
    return true;
}

bool FindFrontMatterBounds(
    const TArray<FString>& Lines,
    int32& OutStart,
    int32& OutEnd)
{
    OutStart = INDEX_NONE;
    OutEnd = INDEX_NONE;
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        if (Lines[Index].TrimStartAndEnd() != TEXT("---"))
        {
            continue;
        }
        if (OutStart == INDEX_NONE)
        {
            OutStart = Index + 1;
        }
        else
        {
            OutEnd = Index;
            return true;
        }
    }
    return false;
}

bool ParsePermissionBlock(
    FUnrealAgentValidationResult& Result,
    const TArray<FString>& Lines,
    int32& Index,
    const int32 FrontMatterEnd,
    const FString& AgentPath)
{
    const int32 PermissionIndent = Yaml::CountIndent(Lines[Index]);
    int32 ChildIndent = INDEX_NONE;
    bool bSawEntry = false;
    for (++Index; Index < FrontMatterEnd; ++Index)
    {
        const FString Trimmed = Lines[Index].TrimStartAndEnd();
        if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("#")))
        {
            continue;
        }
        const int32 Indent = Yaml::CountIndent(Lines[Index]);
        if (Indent <= PermissionIndent)
        {
            break;
        }
        if (ChildIndent == INDEX_NONE)
        {
            ChildIndent = Indent;
        }
        if (Indent != ChildIndent)
        {
            continue;
        }
        bSawEntry = true;
        FString Key;
        FString PermissionValue;
        if (Yaml::HasUnsupportedKeySyntax(Trimmed)
            || !Yaml::SplitField(Trimmed, Key, PermissionValue))
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("OpenCode agent permission entry is invalid: %s"),
                *AgentPath));
            continue;
        }
        if (Yaml::HasUnsupportedScalarSyntax(PermissionValue))
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("OpenCode agent permission entry uses unsupported YAML syntax: %s"),
                *AgentPath));
            continue;
        }
        if (IsProtectedOpenCodePermissionPattern(Key)
            && (PermissionValue.IsEmpty() || Yaml::ContainsAllowValue(PermissionValue)))
        {
            AddUnsafePermissionError(Result, Key, AgentPath);
        }
    }
    return bSawEntry;
}
}

void AddAgentFrontMatterPermissionErrors(
    FUnrealAgentValidationResult& Result,
    const FString& AgentPath,
    const bool bRequireExplicitPolicy)
{
    FString AgentText;
    if (!FFileHelper::LoadFileToString(AgentText, *AgentPath))
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(TEXT("OpenCode agent is unreadable: %s"), *AgentPath));
        return;
    }

    TArray<FString> Lines;
    AgentText.ParseIntoArrayLines(Lines, false);
    int32 FrontMatterStart = INDEX_NONE;
    int32 FrontMatterEnd = INDEX_NONE;
    if (!FindFrontMatterBounds(Lines, FrontMatterStart, FrontMatterEnd))
    {
        if (bRequireExplicitPolicy)
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("OpenCode agent frontmatter is missing: %s"),
                *AgentPath));
        }
        return;
    }

    int32 RootIndent = MAX_int32;
    for (int32 Index = FrontMatterStart; Index < FrontMatterEnd; ++Index)
    {
        const FString Trimmed = Lines[Index].TrimStartAndEnd();
        if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT("#")))
        {
            continue;
        }
        if (Yaml::HasUnsupportedReferenceSyntax(Trimmed))
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("OpenCode agent frontmatter uses unsupported YAML anchors, aliases, tags, or merge keys: %s"),
                *AgentPath));
            return;
        }
        RootIndent = FMath::Min(RootIndent, Yaml::CountIndent(Lines[Index]));
    }

    for (int32 Index = FrontMatterStart; Index < FrontMatterEnd; ++Index)
    {
        if (Yaml::CountIndent(Lines[Index]) != RootIndent)
        {
            continue;
        }
        FString Key;
        FString Value;
        const FString RootField = Lines[Index].TrimStart();
        if (Yaml::HasUnsupportedKeySyntax(RootField))
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("OpenCode agent root field uses unsupported YAML syntax: %s"),
                *AgentPath));
            return;
        }
        if (!Yaml::SplitField(RootField, Key, Value)
            || !Key.Equals(TEXT("permission"), ESearchCase::IgnoreCase))
        {
            continue;
        }
        if (Value.IsEmpty())
        {
            if (!ParsePermissionBlock(Result, Lines, Index, FrontMatterEnd, AgentPath))
            {
                Result.bPassed = false;
                Result.Errors.Add(FString::Printf(
                    TEXT("OpenCode agent permission policy is empty: %s"),
                    *AgentPath));
            }
        }
        else if (!ParsePermissionFlowMap(Result, Value, AgentPath))
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("OpenCode agent permission policy has unsupported shape: %s"),
                *AgentPath));
        }
        return;
    }

    if (bRequireExplicitPolicy)
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("OpenCode agent has no explicit permission policy: %s"),
            *AgentPath));
    }
}
}
