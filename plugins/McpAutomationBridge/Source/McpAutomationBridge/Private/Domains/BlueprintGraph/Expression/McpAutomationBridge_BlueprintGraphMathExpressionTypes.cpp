#include "Domains/BlueprintGraph/Expression/McpAutomationBridge_BlueprintGraphMathExpression.h"

#include "Engine/Blueprint.h"
#include "UObject/UnrealType.h"

// A Math Expression node parses a true/false value used as a number, such as (Speed > 0) * Gain, and only the Blueprint
// compile refuses it: a 134-step batch was applied before that compile named the node. The parse cannot see it, so this
// reads the operands of each arithmetic operator before the batch runs.
namespace McpBlueprintMathExpression
{
namespace
{
bool IsNameToken(const FString& Token)
{
  return Token.Len() > 0 && (FChar::IsAlpha(Token[0]) || Token[0] == TEXT('_'));
}

// The bracket that closes (Step 1) or opens (Step -1) the one at Index; INDEX_NONE when unbalanced.
int32 MatchingBracket(const TArray<FString>& Parts, int32 Index, int32 Step)
{
  int32 Depth = 0;
  for (int32 At = Index; At >= 0 && At < Parts.Num(); At += Step)
  {
    Depth += Parts[At] == TEXT("(") ? Step : Parts[At] == TEXT(")") ? -Step : 0;
    if (Depth == 0)
    {
      return At;
    }
  }
  return INDEX_NONE;
}

// Whether Parts[First..Last] is true/false: a bool variable, or a bracket whose own level compares or joins. A call's
// result is never judged; its type is not known here.
bool IsTrueFalseOperand(const TArray<FString>& Parts, int32 First, int32 Last, const UClass* Members,
                        const TSet<FName>& ExtraBools)
{
  if (First == Last)
  {
    const FName Name(*Parts[First]);
    return CastField<FBoolProperty>(Members ? Members->FindPropertyByName(Name) : nullptr) != nullptr ||
           ExtraBools.Contains(Name);
  }
  static const TArray<FString> TrueFalse = {TEXT("=="), TEXT("!="), TEXT("<"), TEXT(">"), TEXT("<="), TEXT(">="),
                                            TEXT("&&"), TEXT("||")};
  int32 Depth = 0;
  for (int32 At = First + 1; At < Last; ++At)
  {
    Depth += Parts[At] == TEXT("(") ? 1 : Parts[At] == TEXT(")") ? -1 : 0;
    if (Depth == 0 && TrueFalse.Contains(Parts[At]))
    {
      return true;
    }
  }
  return false;
}
}

FString DescribeBoolArithmetic(const UBlueprint* Blueprint, const FString& Expression, const TSet<FName>& ExtraBools)
{
  const TArray<FString> Parts = TokenizeExpression(Expression);
  const UClass* Members = Blueprint ? Blueprint->SkeletonGeneratedClass.Get() : nullptr;
  static const TArray<FString> Arithmetic = {TEXT("+"), TEXT("-"), TEXT("*"), TEXT("/"), TEXT("%")};
  TArray<FString> Notes;
  TArray<FString> Named; // an operand is named once, at its first operator
  for (int32 Index = 1; Index + 1 < Parts.Num(); ++Index)
  {
    if (!Arithmetic.Contains(Parts[Index]))
    {
      continue;
    }
    // Left: a lone name, or a bracket that is no call's argument list.
    int32 LeftFirst = IsNameToken(Parts[Index - 1]) ? Index - 1 : INDEX_NONE;
    if (Parts[Index - 1] == TEXT(")"))
    {
      LeftFirst = MatchingBracket(Parts, Index - 1, -1);
      LeftFirst = LeftFirst > 0 && IsNameToken(Parts[LeftFirst - 1]) ? INDEX_NONE : LeftFirst;
    }
    // Right: a bracket, or a name that is not a call.
    int32 RightLast = INDEX_NONE;
    if (Parts[Index + 1] == TEXT("("))
    {
      RightLast = MatchingBracket(Parts, Index + 1, 1);
    }
    else if (IsNameToken(Parts[Index + 1]) && (Index + 2 >= Parts.Num() || Parts[Index + 2] != TEXT("(")))
    {
      RightLast = Index + 1;
    }
    for (const TPair<int32, int32>& Operand : {TPair<int32, int32>(LeftFirst, Index - 1), TPair<int32, int32>(Index + 1, RightLast)})
    {
      if (Operand.Key == INDEX_NONE || Operand.Value == INDEX_NONE ||
          !IsTrueFalseOperand(Parts, Operand.Key, Operand.Value, Members, ExtraBools))
      {
        continue;
      }
      FString Text;
      for (int32 At = Operand.Key; At <= Operand.Value; ++At)
      {
        Text += (At > Operand.Key && Parts[At] != TEXT(")") && Parts[At - 1] != TEXT("(") ? TEXT(" ") : TEXT("")) + Parts[At];
      }
      if (Named.Contains(Text))
      {
        continue;
      }
      Named.Add(Text);
      Notes.Add(FString::Printf(TEXT("'%s' is true/false, but %s takes numbers and the node turns no bool into a "
                                           "number: write SelectFloat(1, 0, %s) for 1 or 0."),
                                      *Text, *Parts[Index], *Text));
    }
  }
  return FString::Join(Notes, TEXT(" "));
}
}
