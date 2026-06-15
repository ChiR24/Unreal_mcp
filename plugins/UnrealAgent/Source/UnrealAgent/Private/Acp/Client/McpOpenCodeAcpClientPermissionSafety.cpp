#include "Acp/Client/McpOpenCodeAcpClientPermissionSafety.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionPaths.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionShellMutation.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"

namespace UnrealAgent::OpenCodeAcp
{
namespace
{
    TArray<FString> GetNormalizedPathCandidates(const FString& Value)
    {
        FString Normalized = NormalizeShellForSafety(Value).ToLower();
        Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
        TArray<FString> Candidates = {Normalized};
        Normalized.ParseIntoArrayWS(Candidates);
        for (FString& Candidate : Candidates)
        {
            Candidate.TrimStartAndEndInline();
            Candidate = Candidate.TrimQuotes();
            while (!Candidate.IsEmpty()
                && FString(TEXT("),;:]}")).Contains(
                    FString::Chr(Candidate[Candidate.Len() - 1])))
            {
                Candidate.LeftChopInline(1);
            }
            if (Candidate.StartsWith(TEXT("./")))
            {
                Candidate.RightChopInline(2);
            }
        }
        return Candidates;
    }

    FString GetCommandPathOperand(const FString& Candidate)
    {
        static const FString DdOutputPrefix = TEXT("of=");
        return Candidate.StartsWith(DdOutputPrefix)
            ? Candidate.RightChop(DdOutputPrefix.Len())
            : Candidate;
    }

    bool HasUnrealProjectStatePath(const FString& Value)
    {
        for (const FString& Candidate : GetNormalizedPathCandidates(Value))
        {
            const FString PathOperand = GetCommandPathOperand(Candidate);
            if (PathOperand == TEXT("config")
                || PathOperand.StartsWith(TEXT("config/"))
                || PathOperand == TEXT(".opencode")
                || PathOperand.StartsWith(TEXT(".opencode/"))
                || PathOperand.EndsWith(TEXT(".uproject")))
            {
                return true;
            }
        }
        return false;
    }

    bool HasUnrealContentOrPackagePath(const FString& Value)
    {
        for (const FString& Candidate : GetNormalizedPathCandidates(Value))
        {
            const FString PathOperand = GetCommandPathOperand(Candidate);
            if (PathOperand == TEXT("content")
                || PathOperand.StartsWith(TEXT("content/"))
                || PathOperand == TEXT("/game")
                || PathOperand.StartsWith(TEXT("/game/"))
                || PathOperand == TEXT("/engine")
                || PathOperand.StartsWith(TEXT("/engine/")))
            {
                return true;
            }
            TArray<FString> Segments;
            PathOperand.ParseIntoArray(Segments, TEXT("/"), true);
            if (Segments.Num() >= 3
                && Segments[0] == TEXT("plugins")
                && Segments[2] == TEXT("content"))
            {
                return true;
            }
        }
        return false;
    }

    bool IsPathBearingRawInputField(const FString& FieldName)
    {
        return PermissionPaths::IsPathBearingField(FieldName);
    }

    bool JsonValueContainsPathMatching(
        const TSharedPtr<FJsonValue>& Value,
        const bool bPathContext,
        bool (*Predicate)(const FString&))
    {
        if (!Value.IsValid())
        {
            return false;
        }

        if (Value->Type == EJson::String)
        {
            return bPathContext && Predicate(Value->AsString());
        }

        if (Value->Type == EJson::Array)
        {
            for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
            {
                if (JsonValueContainsPathMatching(Element, bPathContext, Predicate))
                {
                    return true;
                }
            }
            return false;
        }

        if (Value->Type == EJson::Object)
        {
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            if (!Object.IsValid())
            {
                return false;
            }

            for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Object->Values)
            {
                const bool bChildPathContext = bPathContext || IsPathBearingRawInputField(Field.Key);
                if (JsonValueContainsPathMatching(Field.Value, bChildPathContext, Predicate))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool LaunchesUnrealEditorExecutable(const TSharedPtr<FJsonValue>& RawInputValue)
    {
        FString Command = NormalizeShellForSafety(
            GetPotentialLocalCommandText(RawInputValue));
        TArray<FString> Tokens;
        FString Current;
        TCHAR Quote = TEXT('\0');
        for (const TCHAR Character : Command)
        {
            if (Quote != TEXT('\0'))
            {
                if (Character == Quote)
                {
                    Quote = TEXT('\0');
                }
                else
                {
                    Current.AppendChar(Character);
                }
                continue;
            }
            if (Character == TEXT('\'') || Character == TEXT('"'))
            {
                Quote = Character;
                continue;
            }
            if (FChar::IsWhitespace(Character))
            {
                if (!Current.IsEmpty())
                {
                    Tokens.Add(MoveTemp(Current));
                    Current.Reset();
                }
                continue;
            }
            Current.AppendChar(Character);
        }
        if (!Current.IsEmpty())
        {
            Tokens.Add(MoveTemp(Current));
        }
        for (FString Token : Tokens)
        {
            Token.ReplaceInline(TEXT("\\"), TEXT("/"));
            Token = FPaths::GetCleanFilename(Token.TrimQuotes()).ToLower();
            if (Token == TEXT("unrealeditor")
                || Token == TEXT("unrealeditor-cmd")
                || Token == TEXT("unrealeditor.exe")
                || Token == TEXT("unrealeditor-cmd.exe"))
            {
                return true;
            }
        }
        return false;
    }

}

bool LooksLikeDirectUnrealProjectStateFileWrite(
    const FString& ToolTitle,
    const FString& ToolKind,
    const TSharedPtr<FJsonValue>& RawInputValue,
    const FString& WorkingDirectory)
{
    if (!RawInputValue.IsValid() || !LooksLikeLocalMutation(ToolTitle, ToolKind, RawInputValue))
    {
        return false;
    }

    const bool bRootPathContext = RawInputValue->Type != EJson::Object
        || ShouldTreatAllLocalStringsAsMutationPaths(
            ToolTitle,
            ToolKind,
            RawInputValue);
    return JsonValueContainsPathMatching(
            RawInputValue,
            bRootPathContext,
            HasUnrealProjectStatePath)
        || JsonReferencesResolvedUnrealProjectState(
            RawInputValue,
            WorkingDirectory,
            bRootPathContext);
}

bool LooksLikeDirectUnrealContentMutation(
    const FString& ToolTitle,
    const FString& ToolKind,
    const TSharedPtr<FJsonValue>& RawInputValue,
    const FString& WorkingDirectory)
{
    if (!RawInputValue.IsValid() || !LooksLikeLocalMutation(ToolTitle, ToolKind, RawInputValue))
    {
        return false;
    }

    const bool bRootPathContext = RawInputValue->Type != EJson::Object
        || ShouldTreatAllLocalStringsAsMutationPaths(
            ToolTitle,
            ToolKind,
            RawInputValue);
    return JsonValueContainsPathMatching(
            RawInputValue,
            bRootPathContext,
            HasUnrealContentOrPackagePath)
        || JsonReferencesResolvedUnrealContent(
            RawInputValue,
            WorkingDirectory,
            bRootPathContext);
}

bool LooksLikeDirectUnrealEditorStateMutation(
    const FString& ToolTitle,
    const FString& ToolKind,
    const TSharedPtr<FJsonValue>& RawInputValue)
{
    return RawInputValue.IsValid()
        && (LaunchesUnrealEditorExecutable(RawInputValue)
            || LooksLikeLocalUnrealSemanticMutation(ToolTitle, ToolKind, RawInputValue));
}

}
