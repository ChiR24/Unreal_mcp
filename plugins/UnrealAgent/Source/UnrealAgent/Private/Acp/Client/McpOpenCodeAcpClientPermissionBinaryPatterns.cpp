#include "Acp/Client/McpOpenCodeAcpClientPermissionBinaryPatterns.h"

#include "Acp/Client/McpOpenCodeAcpClientPermissionBinaryGlob.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionCommandTokens.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionMutation.h"
#include "Acp/Client/McpOpenCodeAcpClientPermissionShellMutation.h"

namespace UnrealAgent::OpenCodeAcp::PermissionBinaryPatterns
{
namespace
{
bool HasUnrealBinaryAssetExtension(const FString& Value)
{
    const FString LowerValue = Value.ToLower();
    const auto HasTerminalExtension =
        [&LowerValue](const FString& Extension)
        {
            int32 ExtensionIndex = LowerValue.Find(Extension);
            while (ExtensionIndex != INDEX_NONE)
            {
                const int32 NextIndex = ExtensionIndex + Extension.Len();
                if (NextIndex == LowerValue.Len()
                    || LowerValue[NextIndex] == TEXT('?')
                    || LowerValue[NextIndex] == TEXT('#')
                    || FChar::IsWhitespace(LowerValue[NextIndex])
                    || FString(TEXT("\"'`),;:]}")).Contains(
                        FString::Chr(LowerValue[NextIndex])))
                {
                    return true;
                }
                ExtensionIndex = LowerValue.Find(
                    Extension,
                    ESearchCase::CaseSensitive,
                    ESearchDir::FromStart,
                    NextIndex);
            }
            return false;
        };
    return HasTerminalExtension(TEXT(".uasset"))
        || HasTerminalExtension(TEXT(".umap"));
}

FString RemoveShellEscapes(const FString& Value)
{
    FString Result;
    Result.Reserve(Value.Len());
    for (int32 Index = 0; Index < Value.Len(); ++Index)
    {
        if (Value[Index] == TEXT('\\') && Index + 1 < Value.Len())
        {
            ++Index;
        }
        Result.AppendChar(Value[Index]);
    }
    return Result;
}

bool NormalizeBinaryAssetShellSyntax(
    const FString& Value,
    FString& OutNormalized)
{
    const FString SafetyNormalized = NormalizeShellForSafety(Value);
    OutNormalized.Reset();
    OutNormalized.Reserve(SafetyNormalized.Len());
    for (int32 Index = 0; Index < SafetyNormalized.Len(); ++Index)
    {
        if (SafetyNormalized[Index] != TEXT('$')
            || Index + 1 >= SafetyNormalized.Len()
            || (SafetyNormalized[Index + 1] != TEXT('\'')
                && SafetyNormalized[Index + 1] != TEXT('"')))
        {
            OutNormalized.AppendChar(SafetyNormalized[Index]);
            continue;
        }

        const TCHAR Quote = SafetyNormalized[Index + 1];
        int32 CloseIndex = Index + 2;
        bool bEscaped = false;
        for (; CloseIndex < SafetyNormalized.Len(); ++CloseIndex)
        {
            const TCHAR Character = SafetyNormalized[CloseIndex];
            if (Character == TEXT('\\'))
            {
                bEscaped = true;
                ++CloseIndex;
                if (CloseIndex >= SafetyNormalized.Len())
                {
                    return false;
                }
                continue;
            }
            if (Character == Quote)
            {
                break;
            }
        }
        if (CloseIndex >= SafetyNormalized.Len())
        {
            return false;
        }
        if (bEscaped)
        {
            OutNormalized.AppendChar(TEXT('*'));
        }
        else
        {
            OutNormalized += SafetyNormalized.Mid(
                Index + 2,
                CloseIndex - Index - 2);
        }
        Index = CloseIndex;
    }
    return true;
}

bool TokenMayResolveToUnrealBinaryAsset(const FString& Token)
{
    if (!Token.Contains(TEXT(".")))
    {
        return false;
    }
    FString ShellNormalizedToken;
    if (!NormalizeBinaryAssetShellSyntax(Token, ShellNormalizedToken))
    {
        return true;
    }
    TArray<FString> ExpandedTokens;
    if (!PermissionBinaryGlob::ExpandBracePatterns(
            ShellNormalizedToken,
            ExpandedTokens))
    {
        return true;
    }
    for (const FString& ExpandedToken : ExpandedTokens)
    {
        TArray<FString> Candidates;
        FString SeparatorCandidate = ExpandedToken;
        SeparatorCandidate.ReplaceInline(TEXT("\\"), TEXT("/"));
        Candidates.Add(SeparatorCandidate);
        Candidates.AddUnique(RemoveShellEscapes(ExpandedToken));
        for (const FString& Candidate : Candidates)
        {
            if (HasUnrealBinaryAssetExtension(Candidate))
            {
                return true;
            }
            const int32 DotIndex = Candidate.Find(
                TEXT("."),
                ESearchCase::CaseSensitive,
                ESearchDir::FromEnd);
            if (DotIndex == INDEX_NONE)
            {
                continue;
            }
            const FString ExtensionPattern = Candidate.Mid(DotIndex);
            if ((ExtensionPattern.Contains(TEXT("*"))
                    || ExtensionPattern.Contains(TEXT("?"))
                    || ExtensionPattern.Contains(TEXT("[")))
                && (PermissionBinaryGlob::ShellPatternMayMatchLiteral(
                        ExtensionPattern,
                        TEXT(".uasset"))
                    || PermissionBinaryGlob::ShellPatternMayMatchLiteral(
                        ExtensionPattern,
                        TEXT(".umap"))))
            {
                return true;
            }
        }
    }
    return false;
}
}

bool HasUnrealBinaryAssetExtensionOrGlob(const FString& Value)
{
    if (HasUnrealBinaryAssetExtension(Value))
    {
        return true;
    }
    const FString Normalized = Value.ToLower();
    TArray<FString> Tokens;
    PermissionCommandTokens::TokenizeQuotedCommandPreservingSyntax(
        Normalized,
        Tokens);
    for (FString Token : Tokens)
    {
        if (Token.Len() >= 2
            && Token.StartsWith(TEXT("\""))
            && Token.EndsWith(TEXT("\"")))
        {
            Token = Token.Mid(1, Token.Len() - 2);
        }
        while (!Token.IsEmpty()
            && FString(TEXT(",;:)")).Contains(
                FString::Chr(Token[Token.Len() - 1])))
        {
            Token.LeftChopInline(1);
        }
        if (TokenMayResolveToUnrealBinaryAsset(Token))
        {
            return true;
        }
    }
    return false;
}

bool IsHarmlessBinaryExtensionMention(const FString& Value)
{
    const FString LowerValue = Value.ToLower().TrimStartAndEnd();
    const bool bSearchPattern =
        (LowerValue.StartsWith(TEXT("rg "))
            || LowerValue.StartsWith(TEXT("grep "))
            || LowerValue.StartsWith(TEXT("/bin/grep "))
            || LowerValue.StartsWith(TEXT("/usr/bin/grep ")))
        && (LowerValue.Contains(TEXT("\\.uasset"))
            || LowerValue.Contains(TEXT("\\.umap")));
    if (bSearchPattern)
    {
        TArray<FString> Tokens;
        PermissionCommandTokens::TokenizeQuotedCommandPreservingSyntax(
            LowerValue,
            Tokens);
        for (const FString& Token : Tokens)
        {
            if (!Token.Contains(TEXT("\\.uasset"))
                && !Token.Contains(TEXT("\\.umap"))
                && HasUnrealBinaryAssetExtensionOrGlob(Token))
            {
                return false;
            }
        }
    }
    const bool bPrintedLiteral =
        (LowerValue.StartsWith(TEXT("python "))
            || LowerValue.StartsWith(TEXT("python3 ")))
        && LowerValue.Contains(TEXT("print("))
        && !LowerValue.Contains(TEXT("open("))
        && !LowerValue.Contains(TEXT("read("));
    return bSearchPattern || bPrintedLiteral;
}
}
