#include "Acp/Client/McpOpenCodeAcpClientPermissionPaths.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionPathResolution.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace UnrealAgent::OpenCodeAcp
{
namespace PermissionPaths
{
bool IsPathBearingField(const FString& FieldName)
{
    const FString LowerFieldName = FieldName.ToLower();
    return LowerFieldName.Contains(TEXT("path"))
        || LowerFieldName.Contains(TEXT("file"))
        || LowerFieldName.Contains(TEXT("command"))
        || LowerFieldName.Contains(TEXT("cmd"))
        || LowerFieldName.Contains(TEXT("script"))
        || LowerFieldName.Contains(TEXT("arg"))
        || LowerFieldName == TEXT("run")
        || LowerFieldName == TEXT("exec")
        || LowerFieldName == TEXT("execute")
        || LowerFieldName == TEXT("shell")
        || LowerFieldName == TEXT("invocation")
        || LowerFieldName == TEXT("invoke")
        || LowerFieldName.Contains(TEXT("patch"))
        || LowerFieldName.Contains(TEXT("source"))
        || LowerFieldName.Contains(TEXT("target"))
        || LowerFieldName.Contains(TEXT("destination"))
        || LowerFieldName.Contains(TEXT("location"))
        || LowerFieldName.Contains(TEXT("directory"))
        || LowerFieldName.Contains(TEXT("cwd"))
        || LowerFieldName.Contains(TEXT("root"))
        || LowerFieldName.Contains(TEXT("input"))
        || LowerFieldName.Contains(TEXT("output"));
}

FString CleanCandidate(FString Candidate)
{
    Candidate.TrimStartAndEndInline();
    Candidate = Candidate.TrimQuotes();
    while (!Candidate.IsEmpty()
        && FString(TEXT("),]}:;")).Contains(FString::Chr(Candidate[Candidate.Len() - 1])))
    {
        Candidate.LeftChopInline(1);
    }
    return Candidate;
}

void AddQuotedCandidates(const FString& Value, const TCHAR Quote, TArray<FString>& OutCandidates)
{
    int32 Start = INDEX_NONE;
    for (int32 Index = 0; Index < Value.Len(); ++Index)
    {
        if (Value[Index] != Quote)
        {
            continue;
        }
        if (Start == INDEX_NONE)
        {
            Start = Index + 1;
        }
        else
        {
            OutCandidates.Add(CleanCandidate(Value.Mid(Start, Index - Start)));
            Start = INDEX_NONE;
        }
    }
}

void AddPathCandidates(const FString& Value, TArray<FString>& OutCandidates)
{
    OutCandidates.Add(CleanCandidate(Value));
    TArray<FString> Tokens;
    Value.ParseIntoArrayWS(Tokens);
    for (FString Token : Tokens)
    {
        OutCandidates.Add(CleanCandidate(MoveTemp(Token)));
    }
    AddQuotedCandidates(Value, TEXT('"'), OutCandidates);
    AddQuotedCandidates(Value, TEXT('\''), OutCandidates);
}

bool JsonReferencesResolvedPath(
    const TSharedPtr<FJsonValue>& Value,
    const FString& WorkingDirectory,
    const bool bPathContext,
    bool (*Predicate)(const FString&, const FString&))
{
    if (!Value.IsValid())
    {
        return false;
    }
    if (Value->Type == EJson::String)
    {
        if (!bPathContext)
        {
            return false;
        }
        TArray<FString> Candidates;
        AddPathCandidates(Value->AsString(), Candidates);
        return Candidates.ContainsByPredicate(
            [&WorkingDirectory, Predicate](const FString& Candidate)
        {
            return Predicate(Candidate, WorkingDirectory);
        });
    }
    if (Value->Type == EJson::Array)
    {
        for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
        {
            if (JsonReferencesResolvedPath(
                    Element,
                    WorkingDirectory,
                    bPathContext,
                    Predicate))
            {
                return true;
            }
        }
        return false;
    }
    if (Value->Type != EJson::Object || !Value->AsObject().IsValid())
    {
        return false;
    }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Value->AsObject()->Values)
    {
        if (JsonReferencesResolvedPath(
                Field.Value,
                WorkingDirectory,
                bPathContext || IsPathBearingField(Field.Key),
                Predicate))
        {
            return true;
        }
    }
    return false;
}
}

bool JsonReferencesResolvedUnrealBinaryAsset(
    const TSharedPtr<FJsonValue>& Value,
    const FString& WorkingDirectory,
    const bool bTreatAllStringsAsPaths)
{
    return PermissionPaths::JsonReferencesResolvedPath(
        Value,
        WorkingDirectory,
        bTreatAllStringsAsPaths
            || (Value.IsValid() && Value->Type != EJson::Object),
        PermissionPaths::ResolvesToUnrealBinaryAsset);
}

bool JsonReferencesResolvedUnrealContent(
    const TSharedPtr<FJsonValue>& Value,
    const FString& WorkingDirectory,
    const bool bTreatAllStringsAsPaths)
{
    return PermissionPaths::JsonReferencesResolvedPath(
        Value,
        WorkingDirectory,
        bTreatAllStringsAsPaths
            || (Value.IsValid() && Value->Type != EJson::Object),
        PermissionPaths::ResolvesToUnrealContent);
}

bool JsonReferencesResolvedUnrealProjectState(
    const TSharedPtr<FJsonValue>& Value,
    const FString& WorkingDirectory,
    const bool bTreatAllStringsAsPaths)
{
    return PermissionPaths::JsonReferencesResolvedPath(
        Value,
        WorkingDirectory,
        bTreatAllStringsAsPaths
            || (Value.IsValid() && Value->Type != EJson::Object),
        PermissionPaths::ResolvesToUnrealProjectState);
}

bool JsonReferencesLinkedPath(
    const TSharedPtr<FJsonValue>& Value,
    const FString& WorkingDirectory,
    const bool bTreatAllStringsAsPaths)
{
    return PermissionPaths::JsonReferencesResolvedPath(
        Value,
        WorkingDirectory,
        bTreatAllStringsAsPaths
            || (Value.IsValid() && Value->Type != EJson::Object),
        PermissionPaths::TraversesSymbolicLink);
}
}
