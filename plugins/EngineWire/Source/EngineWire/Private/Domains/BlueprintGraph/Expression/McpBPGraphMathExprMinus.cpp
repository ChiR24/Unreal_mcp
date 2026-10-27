#include "Domains/BlueprintGraph/Expression/McpBPGraphMathExpr.h"

namespace McpBlueprintMathExpression
{
namespace
{
// One past the operand that starts at Index (after spaces): a bracketed group, a call, a name, a number, or another
// unary minus with its own operand. INDEX_NONE when none starts there.
int32 MinusOperandEnd(const FString& Text, int32 Index)
{
  while (Index < Text.Len() && FChar::IsWhitespace(Text[Index]))
  {
    ++Index;
  }
  if (Index >= Text.Len())
  {
    return INDEX_NONE;
  }
  const auto CloseOf = [&Text](int32 Open)
  {
    int32 Depth = 0;
    for (int32 At = Open; At < Text.Len(); ++At)
    {
      Depth += Text[At] == TEXT('(') ? 1 : (Text[At] == TEXT(')') ? -1 : 0);
      if (Depth == 0)
      {
        return At + 1;
      }
    }
    return static_cast<int32>(INDEX_NONE);
  };
  const TCHAR First = Text[Index];
  if (First == TEXT('('))
  {
    return CloseOf(Index);
  }
  if (First == TEXT('-'))
  {
    return MinusOperandEnd(Text, Index + 1);
  }
  if (!FChar::IsAlnum(First) && First != TEXT('_') && First != TEXT('.'))
  {
    return INDEX_NONE;
  }
  int32 End = Index;
  // A number keeps its exponent sign: 1e-5 is one operand.
  while (End < Text.Len() && (FChar::IsAlnum(Text[End]) || Text[End] == TEXT('_') || Text[End] == TEXT('.') ||
                              ((Text[End] == TEXT('-') || Text[End] == TEXT('+')) && FChar::IsDigit(First) &&
                               FChar::ToLower(Text[End - 1]) == TEXT('e'))))
  {
    ++End;
  }
  int32 Next = End;
  while (Next < Text.Len() && FChar::IsWhitespace(Text[Next]))
  {
    ++Next;
  }
  // A name followed by a bracket is a call: max(a, b) is one operand.
  return Next < Text.Len() && Text[Next] == TEXT('(') && !FChar::IsDigit(First) ? CloseOf(Next) : End;
}
}

FString RewriteUnaryMinus(const FString& Expression)
{
  FString Out;
  TCHAR Prev = 0;
  for (int32 Index = 0; Index < Expression.Len(); ++Index)
  {
    const TCHAR Char = Expression[Index];
    // A minus is unary at the start, after an opening bracket or a comma, and after another operator.
    const bool bUnary = Char == TEXT('-') && (Prev == 0 || FCString::Strchr(TEXT("(,+-*/%<>=!&|"), Prev));
    const int32 End = bUnary ? MinusOperandEnd(Expression, Index + 1) : static_cast<int32>(INDEX_NONE);
    if (End == INDEX_NONE)
    {
      Out.AppendChar(Char);
      Prev = FChar::IsWhitespace(Char) ? Prev : Char;
      continue;
    }
    const FString Operand = Expression.Mid(Index + 1, End - Index - 1).TrimStartAndEnd();
    Out += FString::Printf(TEXT("(0 - %s)"), *RewriteUnaryMinus(Operand));
    Prev = TEXT(')');
    Index = End - 1;
  }
  return Out;
}
}
