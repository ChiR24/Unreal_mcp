// find_functions: the Blueprint-callable functions that do what a few plain words say ("game time", "random number in
// range", "distance between actors"), each with the memberName, memberClass and pins create_node takes. A caller had to
// know a function's name and class already: a guess came back FUNCTION_NOT_FOUND with near spellings only, and words
// that share no letters with the name (game time for GetGameTimeInSeconds) found nothing.
#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "EdGraphSchema_K2.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "UObject/UObjectIterator.h"

namespace McpBlueprintGraphHandlers
{
namespace
{
struct FFoundFunction
{
    UFunction* Function = nullptr;
    FString Path;
    int32 Matched = 0;
    int32 Score = 0;
    int32 NameWords = 0;
};

// Lower-case words of a name or a sentence; a capital after a lower-case letter starts one (GameTime: game, time).
void SplitFinderWords(const FString& Text, TArray<FString>& OutWords)
{
    FString Word;
    for (int32 Index = 0; Index <= Text.Len(); ++Index)
    {
        const TCHAR Char = Index < Text.Len() ? Text[Index] : TEXT(' ');
        const bool bAlnum = FChar::IsAlnum(Char);
        if ((!bAlnum || (FChar::IsUpper(Char) && Index > 0 && FChar::IsLower(Text[Index - 1]))) && !Word.IsEmpty())
        {
            OutWords.Add(Word.ToLower());
            Word.Reset();
        }
        if (bAlnum)
        {
            Word.AppendChar(Char);
        }
    }
}

// 2 when Words holds Wanted, 1 when one of them is the other with up to three more letters ("second" and "seconds",
// "random" and "randomly"; "game" and "gameplay" are other words), else 0. A short word must match whole (in, to).
int32 FinderWordHits(const TArray<FString>& Words, const FString& Wanted)
{
    int32 Best = 0;
    for (const FString& Word : Words)
    {
        if (Word == Wanted)
        {
            return 2;
        }
        const FString& Shorter = Word.Len() < Wanted.Len() ? Word : Wanted;
        const FString& Longer = Word.Len() < Wanted.Len() ? Wanted : Word;
        if (Shorter.Len() >= 4 && Longer.Len() - Shorter.Len() <= 3 && Longer.StartsWith(Shorter))
        {
            Best = 1;
        }
    }
    return Best;
}

// Words a request carries that say nothing about what the node does.
bool IsFinderStopWord(const FString& Word)
{
    static const TSet<FString> Stop = {TEXT("a"), TEXT("an"), TEXT("the"), TEXT("of"), TEXT("to"), TEXT("for"),
        TEXT("in"), TEXT("on"), TEXT("at"), TEXT("by"), TEXT("with"), TEXT("from"), TEXT("and"), TEXT("or"),
        TEXT("is"), TEXT("it"), TEXT("my"), TEXT("that"), TEXT("which"), TEXT("node"), TEXT("function"),
        TEXT("blueprint"), TEXT("few"), TEXT("some")};
    return Stop.Contains(Word);
}

bool IsFinderCandidate(const UFunction& Function)
{
    return Function.HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_BlueprintPure) &&
           !Function.HasAnyFunctionFlags(FUNC_EditorOnly) && !Function.HasMetaData(TEXT("BlueprintInternalUseOnly")) &&
           !Function.HasMetaData(TEXT("DeprecatedFunction"));
}

// How well a function's name, keywords, category and description hold the wanted words: a name word counts most, a
// whole word more than a near one, and each place that says it adds (Get Game Time in Seconds says time in its name,
// its category and its description; Get Game Time Since Creation only in the first and last).
void ScoreFoundFunction(FFoundFunction& Found, const TArray<FString>& Wanted)
{
    const UFunction& Function = *Found.Function;
    TArray<FString> Name, Keywords, Category, Tip;
    SplitFinderWords(Function.GetName(), Name);
    SplitFinderWords(Function.GetDisplayNameText().ToString(), Name);
    Found.NameWords = Name.Num();
    SplitFinderWords(Function.GetMetaData(TEXT("Keywords")), Keywords);
    SplitFinderWords(Function.GetMetaData(TEXT("Category")), Category);
    // The tooltip without the names after @param: InItem made every getter that takes an item key match "item".
    TArray<FString> TipTokens;
    Function.GetToolTipText().ToString().ParseIntoArrayWS(TipTokens);
    for (int32 Index = 0; Index < TipTokens.Num(); ++Index)
    {
        if (TipTokens[Index] == TEXT("@param"))
        {
            ++Index;
            continue;
        }
        SplitFinderWords(TipTokens[Index], Tip);
    }
    for (const FString& Word : Wanted)
    {
        const int32 InName = FinderWordHits(Name, Word), InKeywords = FinderWordHits(Keywords, Word);
        const int32 InCategory = FinderWordHits(Category, Word), InTip = FinderWordHits(Tip, Word);
        Found.Matched += InName + InKeywords + InCategory + InTip > 0 ? 1 : 0;
        Found.Score += InName * 4 + InKeywords * 3 + InCategory * 2 + (InTip == 2 ? 1 : 0);
    }
}

bool IsFinderClass(const UClass& Class)
{
    const FString Name = Class.GetName();
    return !Class.HasAnyClassFlags(CLASS_CompiledFromBlueprint | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Interface) &&
           !Name.StartsWith(TEXT("SKEL_")) && !Name.StartsWith(TEXT("REINST_")) && !Name.StartsWith(TEXT("TRASHCLASS_")) &&
           !Name.StartsWith(TEXT("HOTRELOADED_")) && !Class.GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly);
}

// The pins create_node gives the function, by the names connect_pins takes; hidden ones (a world context, a latent
// action's info) and the Target (self) pin are left out: target names the class it acts on.
TSharedPtr<FJsonObject> DescribeFoundFunction(const UFunction& Function)
{
    const UClass* Owner = Function.GetOwnerClass();
    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetStringField(TEXT("memberName"), Function.GetName());
    Out->SetStringField(TEXT("memberClass"), Owner ? Owner->GetPathName() : FString());
    Out->SetStringField(TEXT("displayName"), Function.GetDisplayNameText().ToString());
    Out->SetStringField(TEXT("category"), Function.GetMetaData(TEXT("Category")));
    Out->SetBoolField(TEXT("pure"), Function.HasAnyFunctionFlags(FUNC_BlueprintPure));
    if (Function.HasMetaData(TEXT("Latent")))
    {
        Out->SetBoolField(TEXT("latent"), true);
    }
    if (Owner && !Function.HasAnyFunctionFlags(FUNC_Static))
    {
        Out->SetStringField(TEXT("target"), Owner->GetName());
    }
    FString Tip = Function.GetToolTipText(true).ToString();
    int32 LineEnd = INDEX_NONE;
    if (Tip.FindChar(TEXT('\n'), LineEnd))
    {
        Tip.LeftInline(LineEnd);
    }
    TArray<FString> TipWords;
    Tip.ParseIntoArrayWS(TipWords);
    Out->SetStringField(TEXT("description"), FString::Join(TipWords, TEXT(" ")).Left(160));
    TArray<FString> Hidden;
    Function.GetMetaData(TEXT("HidePin")).ParseIntoArray(Hidden, TEXT(","));
    for (FString& Name : Hidden)
    {
        Name.TrimStartAndEndInline();
    }
    Hidden.Add(Function.GetMetaData(TEXT("WorldContext")));
    Hidden.Add(Function.GetMetaData(TEXT("LatentInfo")));
    const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
    TArray<TSharedPtr<FJsonValue>> Inputs, Outputs;
    for (TFieldIterator<FProperty> It(&Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
    {
        if (Hidden.Contains(It->GetName()))
        {
            continue;
        }
        FEdGraphPinType PinType;
        Schema->ConvertPropertyToPinType(*It, PinType);
        TSharedPtr<FJsonObject> Pin = McpHandlerUtils::CreateResultObject();
        Pin->SetStringField(TEXT("pinName"), It->GetName());
        Pin->SetStringField(TEXT("pinType"), PinType.PinCategory.ToString());
        AddPinTypeFields(Pin, PinType);
        const bool bOutput = It->HasAnyPropertyFlags(CPF_ReturnParm) ||
                             (It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ReferenceParm));
        (bOutput ? Outputs : Inputs).Add(MakeShared<FJsonValueObject>(Pin));
    }
    Out->SetArrayField(TEXT("inputs"), Inputs);
    Out->SetArrayField(TEXT("outputs"), Outputs);
    return Out;
}
}

bool HandleFindFunctions(FActionContext& Context)
{
    if (Context.SubAction != TEXT("find_functions"))
    {
        return false;
    }
    FString Request, MemberClass;
    Context.Payload->TryGetStringField(TEXT("words"), Request);
    Context.Payload->TryGetStringField(TEXT("memberClass"), MemberClass);
    TArray<FString> Words, Wanted;
    SplitFinderWords(Request, Words);
    for (const FString& Word : Words)
    {
        if (!IsFinderStopWord(Word))
        {
            Wanted.AddUnique(Word);
        }
    }
    if (Wanted.Num() == 0)
    {
        Context.SendError(TEXT("words: say what the node does in a few plain words, such as game time, random number "
                               "in range or distance between actors."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    // memberClass: only what an object of that class can call on itself (a Character's Jump), no library functions.
    UClass* Scope = nullptr;
    if (!MemberClass.IsEmpty())
    {
        Scope = ResolveUClass(MemberClass);
        Scope = Scope ? Scope : ResolveTargetClassFromString(MemberClass);
        if (!Scope)
        {
            Context.SendError(FString::Printf(TEXT("memberClass '%s' is not a class: pass a native class name "
                                                   "(Character) or a Blueprint asset path (/Game/Folder/BP_Name)."), *MemberClass),
                              TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }
    // Two thirds of up to three wanted words must hit (both of two), so one common word ("game") does not list
    // everything with it; half of a longer request, which carries words no function says ("repeat every few seconds").
    const int32 Needed = Wanted.Num() <= 3 ? (2 * Wanted.Num() + 2) / 3 : (Wanted.Num() + 1) / 2;
    TArray<FFoundFunction> Found;
    for (TObjectIterator<UClass> It; It; ++It)
    {
        if (!IsFinderClass(**It) || (Scope && !Scope->IsChildOf(*It)))
        {
            continue;
        }
        for (TFieldIterator<UFunction> FuncIt(*It, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
        {
            FFoundFunction Candidate;
            Candidate.Function = *FuncIt;
            if (!IsFinderCandidate(**FuncIt))
            {
                continue;
            }
            ScoreFoundFunction(Candidate, Wanted);
            if (Candidate.Matched >= Needed)
            {
                Candidate.Path = It->GetPathName() + TEXT(".") + FuncIt->GetName();
                Found.Add(MoveTemp(Candidate));
            }
        }
    }
    Found.Sort([](const FFoundFunction& A, const FFoundFunction& B)
    {
        if (A.Matched != B.Matched) return A.Matched > B.Matched;
        if (A.Score != B.Score) return A.Score > B.Score;
        if (A.NameWords != B.NameWords) return A.NameWords < B.NameWords;
        return FCString::Strcmp(*A.Path, *B.Path) < 0;
    });
    const int32 Count = FMath::Min(Found.Num(), 10);
    TArray<TSharedPtr<FJsonValue>> Functions;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Functions.Add(MakeShared<FJsonValueObject>(DescribeFoundFunction(*Found[Index].Function)));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("functions"), Functions);
    Result->SetBoolField(TEXT("hasMore"), Found.Num() > Count);
    Context.SendResponse(Count == 0
        ? FString::Printf(TEXT("No Blueprint function matched '%s'. Use other words for what the node does, or fewer."), *Request)
        : FString::Printf(TEXT("%d Blueprint functions for '%s', best first: %s on %s."), Count, *Request,
                          *Found[0].Function->GetDisplayNameText().ToString(), *Found[0].Function->GetOwnerClass()->GetName()),
        Result);
    return true;
}
}
