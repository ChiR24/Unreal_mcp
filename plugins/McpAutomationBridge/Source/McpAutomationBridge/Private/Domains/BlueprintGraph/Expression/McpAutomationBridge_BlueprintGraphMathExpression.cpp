#include "Domains/BlueprintGraph/Expression/McpAutomationBridge_BlueprintGraphMathExpression.h"

#include "Dom/JsonValue.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/UObjectIterator.h"

namespace McpBlueprintMathExpression
{
namespace
{
// The functions the node files under aliases instead of their titles ({function, alias}; a null alias is an operator
// symbol a call cannot use), mirrored from UK2Node_MathExpression's GetOperatorAliases in 5.8 (5.0 lacks the Deg and
// Rad rows). A missing row only makes a hint less exact; the node itself decides what parses.
const TCHAR* const AliasRows[][2] = {
  {TEXT("BooleanAND"), nullptr}, {TEXT("BooleanOR"), nullptr}, {TEXT("BooleanXOR"), nullptr},
  {TEXT("Not_PreBool"), nullptr}, {TEXT("Square"), TEXT("SQUARE")}, {TEXT("FClamp"), TEXT("CLAMP")},
  {TEXT("MultiplyMultiply_FloatFloat"), TEXT("POWER")}, {TEXT("MultiplyMultiply_FloatFloat"), TEXT("POW")},
  {TEXT("FCEIL"), TEXT("FCEIL")}, {TEXT("FCEIL"), TEXT("CEIL")}, {TEXT("ASin"), TEXT("ASIN")}, {TEXT("ASin"), TEXT("ARCSIN")},
  {TEXT("ACos"), TEXT("ACOS")}, {TEXT("ACos"), TEXT("ARCCOS")}, {TEXT("ATan"), TEXT("ATAN")}, {TEXT("ATan"), TEXT("ARCTAN")},
  {TEXT("DegAtan"), TEXT("DEGATAN")}, {TEXT("DegAtan"), TEXT("DEGARCTAN")}, {TEXT("DegAtan2"), TEXT("DEGATAN2")},
  {TEXT("DegAtan2"), TEXT("DEGARCTAN2")}, {TEXT("DegreesToRadians"), TEXT("DEGTORAD")}, {TEXT("DegreesToRadians"), TEXT("D2R")},
  {TEXT("RadiansToDegrees"), TEXT("RADTODEG")}, {TEXT("RadiansToDegrees"), TEXT("R2D")}, {TEXT("ATan2"), TEXT("ATAN2")},
  {TEXT("ATan2"), TEXT("ARCTAN2")}, {TEXT("MakeVector"), TEXT("VECTOR")}, {TEXT("MakeVector"), TEXT("VEC")},
  {TEXT("MakeVector"), TEXT("VECT")}, {TEXT("MakeVector2D"), TEXT("VECTOR2D")}, {TEXT("MakeVector2D"), TEXT("VEC2D")},
  {TEXT("MakeVector2D"), TEXT("VECT2D")}, {TEXT("MakeRotator"), TEXT("ROTATOR")}, {TEXT("MakeRotator"), TEXT("ROT")},
  {TEXT("MakeTransform"), TEXT("TRANSFORM")}, {TEXT("MakeTransform"), TEXT("XFORM")}, {TEXT("MakeColor"), TEXT("COLOR")},
  {TEXT("MakeColor"), TEXT("LINEARCOLOR")}, {TEXT("MakeColor"), TEXT("COLOUR")}, {TEXT("RandomFloat"), TEXT("RandomFloat")},
  {TEXT("RandomFloat"), TEXT("RAND")}, {TEXT("RandomFloat"), TEXT("RANDOM")}, {TEXT("Dot_VectorVector"), TEXT("Dot")},
  {TEXT("Cross_VectorVector"), TEXT("Cross")}};

// Name -> the library function it reaches, filed as UK2Node_MathExpression's operator table files it: every pure
// library function with a result, under its aliases, else its compact title (max, abs), else its display name, else
// its name (Fraction, SelectFloat), spaces removed. FString keys compare without case, as the node's table does.
TMap<FString, FString> CallableNames()
{
  TMap<FString, FString> Names;
  TSet<FString> Aliased;
  for (const auto& Row : AliasRows)
  {
    Aliased.Add(Row[0]);
    if (Row[1])
    {
      Names.Add(Row[1], Row[0]);
    }
  }
  for (TObjectIterator<UClass> It; It; ++It)
  {
    if (!It->IsChildOf(UBlueprintFunctionLibrary::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract))
    {
      continue;
    }
    for (TFieldIterator<UFunction> Function(*It, EFieldIteratorFlags::ExcludeSuper); Function; ++Function)
    {
      if (!Function->HasAnyFunctionFlags(FUNC_BlueprintPure) || !Function->GetReturnProperty() ||
          Aliased.Contains(Function->GetName()))
      {
        continue;
      }
      FString Key = Function->GetName();
      if (Function->HasMetaData(FBlueprintMetadata::MD_CompactNodeTitle))
      {
        Key = Function->GetMetaData(FBlueprintMetadata::MD_CompactNodeTitle);
      }
      else if (Function->HasMetaData(FBlueprintMetadata::MD_DisplayName))
      {
        Key = Function->GetMetaData(FBlueprintMetadata::MD_DisplayName);
      }
      Key.ReplaceInline(TEXT(" "), TEXT(""));
      Names.Add(Key, Function->GetName());
    }
  }
  return Names;
}

// The names in Expression: OutCalls are followed by '(' (functions), OutNames are not (inputs or variables).
void SplitNames(const FString& Expression, TArray<FString>& OutCalls, TArray<FString>& OutNames)
{
  int32 Index = 0;
  while (Index < Expression.Len())
  {
    const TCHAR Char = Expression[Index];
    if (FChar::IsDigit(Char) || Char == TEXT('.'))
    {
      // A number, exponent included: 1e5 holds no name e5.
      while (Index < Expression.Len() && (FChar::IsAlnum(Expression[Index]) || Expression[Index] == TEXT('.')))
      {
        ++Index;
      }
      continue;
    }
    if (!FChar::IsAlpha(Char) && Char != TEXT('_'))
    {
      ++Index;
      continue;
    }
    const int32 Start = Index;
    while (Index < Expression.Len() && (FChar::IsAlnum(Expression[Index]) || Expression[Index] == TEXT('_')))
    {
      ++Index;
    }
    int32 Next = Index;
    while (Next < Expression.Len() && FChar::IsWhitespace(Expression[Next]))
    {
      ++Next;
    }
    const bool bCall = Next < Expression.Len() && Expression[Next] == TEXT('(');
    (bCall ? OutCalls : OutNames).AddUnique(Expression.Mid(Start, Index - Start));
  }
}

// Expression as tokens: names, numbers, two-character operators (&& || == != <= >=) and single characters.
TArray<FString> Tokens(const FString& Expression)
{
  TArray<FString> Out;
  int32 Index = 0;
  while (Index < Expression.Len())
  {
    const TCHAR Char = Expression[Index];
    int32 End = Index + 1;
    if (FChar::IsAlnum(Char) || Char == TEXT('_') || Char == TEXT('.'))
    {
      while (End < Expression.Len() && (FChar::IsAlnum(Expression[End]) || Expression[End] == TEXT('_') ||
                                        Expression[End] == TEXT('.')))
      {
        ++End;
      }
    }
    else if (TArray<FString>({TEXT("&&"), TEXT("||"), TEXT("=="), TEXT("!="), TEXT("<="), TEXT(">=")})
                 .Contains(Expression.Mid(Index, 2)))
    {
      ++End;
    }
    if (!FChar::IsWhitespace(Char))
    {
      Out.Add(Expression.Mid(Index, End - Index));
    }
    Index = End;
  }
  return Out;
}
}

FString DescribeOperatorMisuse(const UBlueprint* Blueprint, const FString& Expression)
{
  const TArray<FString> Parts = Tokens(Expression);
  const UClass* Members = Blueprint ? Blueprint->SkeletonGeneratedClass.Get() : nullptr;
  const TArray<FString> Comparisons = {TEXT("=="), TEXT("!="), TEXT("<"), TEXT(">"), TEXT("<="), TEXT(">=")};
  TArray<FString> Notes;
  for (int32 Index = 0; Index < Parts.Num(); ++Index)
  {
    const FString Prev = Index > 0 ? Parts[Index - 1] : FString();
    const FString Next = Index + 1 < Parts.Num() ? Parts[Index + 1] : FString();
    const bool bAfterOperand = FChar::IsAlnum(Prev.Len() ? Prev[0] : TEXT(' ')) || Prev == TEXT(")") ||
                               Prev == TEXT("_") || Prev.StartsWith(TEXT("."));
    if (Parts[Index] == TEXT("!"))
    {
      Notes.AddUnique(TEXT("There is no unary !: write (x == false)."));
    }
    else if (Parts[Index] == TEXT("-") && !bAfterOperand)
    {
      Notes.AddUnique(TEXT("There is no unary minus: write 0 - x."));
    }
    const TCHAR First = Parts[Index][0];
    const bool bName = (FChar::IsAlpha(First) || First == TEXT('_')) && Next != TEXT("(");
    const bool bLogical = Prev == TEXT("&&") || Prev == TEXT("||") || Next == TEXT("&&") || Next == TEXT("||");
    if (!bName || !bLogical || Comparisons.Contains(Prev) || Comparisons.Contains(Next))
    {
      continue;
    }
    const FProperty* Member = Members ? Members->FindPropertyByName(FName(*Parts[Index])) : nullptr;
    if (!CastField<FBoolProperty>(Member))
    {
      Notes.AddUnique(FString::Printf(TEXT("'%s' is a number here (a name that is not a bool variable becomes a "
                                           "number input), but && and || take true/false: compare it (%s > 0), "
                                           "name a bool variable, or wire a BooleanAND node."),
                                      *Parts[Index], *Parts[Index]));
    }
  }
  return FString::Join(Notes, TEXT(" "));
}

FString DescribeUnknownFunctions(const FString& Expression)
{
  TArray<FString> Calls;
  TArray<FString> Names;
  SplitNames(Expression, Calls, Names);
  const TMap<FString, FString> Callable = CallableNames();
  TArray<FString> Unknown;
  for (const FString& Call : Calls)
  {
    if (Callable.Contains(Call))
    {
      continue;
    }
    // FMax and FClamp are C++ names; the node files them as Max and Clamp. frac is a shader name for Fraction.
    TArray<FString> Try;
    const FString Bare = Call.Mid(1);
    if (Call.StartsWith(TEXT("F"), ESearchCase::CaseSensitive) && Callable.Contains(Bare))
    {
      Try.Add(Bare);
    }
    // The shortest names that start with it first: frac means Fraction, not FractionalBeatInBar.
    TArray<FString> Starting;
    for (const TPair<FString, FString>& Pair : Callable)
    {
      if (Pair.Key.StartsWith(Call) || Pair.Value.StartsWith(Call))
      {
        Starting.AddUnique(Pair.Key);
      }
    }
    Starting.Sort([](const FString& A, const FString& B) { return A.Len() < B.Len(); });
    for (int32 Index = 0; Index < Starting.Num() && Try.Num() < 2; ++Index)
    {
      Try.AddUnique(Starting[Index]);
    }
    Unknown.Add(Try.Num() > 0 ? FString::Printf(TEXT("%s (try %s)"), *Call, *FString::Join(Try, TEXT(" or "))) : Call);
  }
  return Unknown.Num() > 0 ? FString::Printf(TEXT("No math function is called %s."), *FString::Join(Unknown, TEXT(", ")))
                           : FString();
}

void DescribeInputs(const UBlueprint* Blueprint, const UEdGraphNode& Node, const FString& Expression,
                    const TSharedPtr<FJsonObject>& Result)
{
  TArray<TSharedPtr<FJsonValue>> Inputs;
  for (const UEdGraphPin* Pin : Node.Pins)
  {
    if (Pin && Pin->Direction == EGPD_Input && !Pin->bOrphanedPin)
    {
      Inputs.Add(MakeShared<FJsonValueString>(Pin->PinName.ToString()));
    }
  }
  Result->SetArrayField(TEXT("inputPins"), Inputs);
  TArray<FString> Calls;
  TArray<FString> Names;
  SplitNames(Expression, Calls, Names);
  const UClass* Members = Blueprint ? Blueprint->SkeletonGeneratedClass.Get() : nullptr;
  TArray<TSharedPtr<FJsonValue>> Bound;
  for (const FString& Name : Names)
  {
    if (Members && !Node.FindPin(Name, EGPD_Input) && Members->FindPropertyByName(FName(*Name)))
    {
      Bound.Add(MakeShared<FJsonValueString>(Name));
    }
  }
  if (Bound.Num() > 0)
  {
    Result->SetArrayField(TEXT("boundToMembers"), Bound);
  }
}
}
