#include "Domains/BlueprintGraph/Expression/McpBPGraphMathExpr.h"

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

// Whether Parts[At] bounds an operand of == or !=: the expression's edge, a bracket, a comma, or an operator that binds
// looser (the node parses with C precedence). A lone name between two such bounds is the whole operand.
bool IsEqualityEdge(const TArray<FString>& Parts, int32 At)
{
  static const TArray<FString> Looser = {TEXT("("), TEXT(")"), TEXT(","), TEXT("&&"), TEXT("||"), TEXT("&"), TEXT("|"),
                                         TEXT("^")};
  return !Parts.IsValidIndex(At) || Looser.Contains(Parts[At]);
}

// Whether Parts[First..Last] is true/false: a bool variable, or a bracket whose own level compares or joins. A call's
// result is never judged; its type is not known here.
bool IsTrueFalseOperand(const TArray<FString>& Parts, int32 First, int32 Last, const UBlueprint* Blueprint,
                        const TSet<FName>& ExtraBools)
{
  if (First == Last)
  {
    return IsBoolName(Blueprint, Parts[First], ExtraBools);
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

// Parts[First..Last] as the caller wrote it, a space between tokens except inside brackets.
FString JoinTokens(const TArray<FString>& Parts, int32 First, int32 Last)
{
  FString Text;
  for (int32 At = First; At <= Last; ++At)
  {
    Text += (At > First && Parts[At] != TEXT(")") && Parts[At - 1] != TEXT("(") ? TEXT(" ") : TEXT("")) + Parts[At];
  }
  return Text;
}

// The operand of the && or || at Index on the side Step points to (-1 left, 1 right): up to the next && || or comma on
// the same bracket level, or the bracket around it. Both operators take true/false, so every such operand must be one,
// whatever their precedence: T - Next < 0 || Ready is a comparison and a name, not the name T.
TPair<int32, int32> LogicalOperand(const TArray<FString>& Parts, int32 Index, int32 Step)
{
  int32 Depth = 0;
  int32 At = Index + Step;
  for (; Parts.IsValidIndex(At); At += Step)
  {
    Depth += Parts[At] == TEXT("(") ? Step : Parts[At] == TEXT(")") ? -Step : 0;
    if (Depth < 0 || (Depth == 0 && (Parts[At] == TEXT("&&") || Parts[At] == TEXT("||") || Parts[At] == TEXT(","))))
    {
      break;
    }
  }
  return Step > 0 ? TPair<int32, int32>(Index + 1, At - 1) : TPair<int32, int32>(At + 1, Index - 1);
}
}

bool IsBoolName(const UBlueprint* Blueprint, const FString& Name, const TSet<FName>& ExtraBools)
{
  const UClass* Members = Blueprint ? Blueprint->SkeletonGeneratedClass.Get() : nullptr;
  return Name.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Name.Equals(TEXT("false"), ESearchCase::IgnoreCase) ||
         ExtraBools.Contains(FName(*Name)) ||
         CastField<FBoolProperty>(Members ? Members->FindPropertyByName(FName(*Name)) : nullptr) != nullptr;
}

TArray<FString> DescribeNumberLogic(const UBlueprint* Blueprint, const TArray<FString>& Parts,
                                    const TSet<FName>& ExtraBools)
{
  // On the operand's own level these make it true/false, or a value not typed here (! has a note of its own).
  static const TArray<FString> NotJudged = {TEXT("=="), TEXT("!="), TEXT("<"), TEXT(">"), TEXT("<="), TEXT(">="),
                                            TEXT("&&"), TEXT("||"), TEXT("&"), TEXT("|"), TEXT("^"), TEXT("!")};
  TArray<FString> Notes;
  for (int32 Index = 0; Index < Parts.Num(); ++Index)
  {
    if (Parts[Index] != TEXT("&&") && Parts[Index] != TEXT("||"))
    {
      continue;
    }
    for (const int32 Step : {-1, 1})
    {
      const TPair<int32, int32> Operand = LogicalOperand(Parts, Index, Step);
      int32 First = Operand.Key;
      int32 Last = Operand.Value;
      while (First < Last && Parts[First] == TEXT("(") && MatchingBracket(Parts, First, 1) == Last)
      {
        ++First;
        --Last;
      }
      bool bNumber = First <= Last && !(First == Last && IsBoolName(Blueprint, Parts[First], ExtraBools));
      int32 Depth = 0;
      for (int32 At = First; At <= Last && bNumber; ++At)
      {
        Depth += Parts[At] == TEXT("(") ? 1 : Parts[At] == TEXT(")") ? -1 : 0;
        // A call's result is never judged, wherever it sits: its type is not known here.
        const bool bCall = IsNameToken(Parts[At]) && Parts.IsValidIndex(At + 1) && Parts[At + 1] == TEXT("(");
        bNumber = !bCall && !(Depth == 0 && NotJudged.Contains(Parts[At]));
      }
      if (!bNumber)
      {
        continue;
      }
      const FString Text = JoinTokens(Parts, First, Last);
      Notes.AddUnique(First == Last && IsNameToken(Text)
                          ? FString::Printf(TEXT("'%s' is a number here (a name that is not a bool variable becomes a "
                                                 "number input), but && and || take true/false: compare it (%s > 0), "
                                                 "name a bool variable, or join true/false values from other nodes "
                                                 "with BooleanAND or BooleanOR nodes."),
                                            *Text, *Text)
                          : FString::Printf(TEXT("'%s' is a number, but && and || take true/false: compare it (%s > 0) "
                                                 "or join true/false values from other nodes with BooleanAND or "
                                                 "BooleanOR nodes."),
                                            *Text, *Text));
    }
  }
  return Notes;
}

FString DescribeBoolArithmetic(const UBlueprint* Blueprint, const FString& Expression, const TSet<FName>& ExtraBools)
{
  const TArray<FString> Parts = TokenizeExpression(Expression);
  static const TArray<FString> Arithmetic = {TEXT("+"), TEXT("-"), TEXT("*"), TEXT("/"), TEXT("%")};
  TArray<FString> Notes;
  TArray<FString> Named; // an operand is named once, at its first operator
  for (int32 Index = 1; Index + 1 < Parts.Num(); ++Index)
  {
    const bool bEquality = Parts[Index] == TEXT("==") || Parts[Index] == TEXT("!=");
    if (!bEquality && !Arithmetic.Contains(Parts[Index]))
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
    // == and != take two numbers or two bools: a lone name that is no bool variable is a number input, and comparing it
    // with a true/false value does not parse. The bare "does not parse" hid that the name had become a number.
    if (bEquality)
    {
      const bool bWhole = LeftFirst != INDEX_NONE && RightLast != INDEX_NONE && IsEqualityEdge(Parts, LeftFirst - 1) &&
                          IsEqualityEdge(Parts, RightLast + 1);
      const bool bLeft = bWhole && IsTrueFalseOperand(Parts, LeftFirst, Index - 1, Blueprint, ExtraBools);
      const bool bRight = bWhole && IsTrueFalseOperand(Parts, Index + 1, RightLast, Blueprint, ExtraBools);
      const int32 Number = bRight && !bLeft && LeftFirst == Index - 1   ? Index - 1
                           : bLeft && !bRight && RightLast == Index + 1 ? Index + 1
                                                                        : INDEX_NONE;
      if (Number != INDEX_NONE)
      {
        Notes.AddUnique(FString::Printf(TEXT("'%s' is a number here (a name that is not a bool variable becomes a "
                                             "number input), but %s compares it with a true/false value: add a bool "
                                             "variable %s, compare it (%s > 0), or join true/false values from other "
                                             "nodes with BooleanAND, BooleanOR or BooleanXOR nodes."),
                                        *Parts[Number], *Parts[Index], *Parts[Number], *Parts[Number]));
      }
      continue;
    }
    for (const TPair<int32, int32>& Operand : {TPair<int32, int32>(LeftFirst, Index - 1), TPair<int32, int32>(Index + 1, RightLast)})
    {
      if (Operand.Key == INDEX_NONE || Operand.Value == INDEX_NONE ||
          !IsTrueFalseOperand(Parts, Operand.Key, Operand.Value, Blueprint, ExtraBools))
      {
        continue;
      }
      const FString Text = JoinTokens(Parts, Operand.Key, Operand.Value);
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
