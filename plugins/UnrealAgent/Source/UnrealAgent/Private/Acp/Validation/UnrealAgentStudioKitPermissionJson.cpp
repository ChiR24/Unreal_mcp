#include "Acp/Validation/UnrealAgentStudioKitPermissionJson.h"

namespace UnrealAgent::Validation
{
FString NormalizeOpenCodeJsonText(const FString& Text)
{
    FString WithoutComments;
    bool bInString = false;
    bool bEscaped = false;
    bool bLineComment = false;
    bool bBlockComment = false;
    for (int32 Index = 0; Index < Text.Len(); ++Index)
    {
        const TCHAR Character = Text[Index];
        const TCHAR Next = Index + 1 < Text.Len() ? Text[Index + 1] : TEXT('\0');
        if (bLineComment)
        {
            if (Character == TEXT('\n'))
            {
                bLineComment = false;
                WithoutComments.AppendChar(Character);
            }
            continue;
        }
        if (bBlockComment)
        {
            if (Character == TEXT('*') && Next == TEXT('/'))
            {
                bBlockComment = false;
                ++Index;
            }
            else if (Character == TEXT('\n'))
            {
                WithoutComments.AppendChar(Character);
            }
            continue;
        }
        if (!bInString && Character == TEXT('/') && Next == TEXT('/'))
        {
            bLineComment = true;
            ++Index;
            continue;
        }
        if (!bInString && Character == TEXT('/') && Next == TEXT('*'))
        {
            bBlockComment = true;
            ++Index;
            continue;
        }
        WithoutComments.AppendChar(Character);
        if (bInString)
        {
            if (bEscaped)
            {
                bEscaped = false;
            }
            else if (Character == TEXT('\\'))
            {
                bEscaped = true;
            }
            else if (Character == TEXT('"'))
            {
                bInString = false;
            }
        }
        else if (Character == TEXT('"'))
        {
            bInString = true;
        }
    }

    FString Normalized;
    bInString = false;
    bEscaped = false;
    for (int32 Index = 0; Index < WithoutComments.Len(); ++Index)
    {
        const TCHAR Character = WithoutComments[Index];
        if (!bInString && Character == TEXT(','))
        {
            int32 LookAhead = Index + 1;
            while (LookAhead < WithoutComments.Len()
                && FChar::IsWhitespace(WithoutComments[LookAhead]))
            {
                ++LookAhead;
            }
            if (LookAhead < WithoutComments.Len()
                && (WithoutComments[LookAhead] == TEXT('}')
                    || WithoutComments[LookAhead] == TEXT(']')))
            {
                continue;
            }
        }
        Normalized.AppendChar(Character);
        if (bInString)
        {
            if (bEscaped)
            {
                bEscaped = false;
            }
            else if (Character == TEXT('\\'))
            {
                bEscaped = true;
            }
            else if (Character == TEXT('"'))
            {
                bInString = false;
            }
        }
        else if (Character == TEXT('"'))
        {
            bInString = true;
        }
    }
    return Normalized;
}
}
