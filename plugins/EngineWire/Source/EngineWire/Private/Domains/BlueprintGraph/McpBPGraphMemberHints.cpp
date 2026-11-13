// Why a member a node names was not found: the reflected names near it, that it is a property, the other classes that
// declare it, or a memberClass that resolved nothing.
#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

namespace McpBlueprintGraphHandlers
{
FString SuggestMemberFix(UClass* Class, const FString& Wanted)
{
    if (!Class || Wanted.IsEmpty())
    {
        return FString();
    }
    FString Bare = Wanted;
    if (Bare.StartsWith(TEXT("Set")) || Bare.StartsWith(TEXT("Get")))
    {
        Bare = Bare.RightChop(3);
    }
    for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
    {
        const FString PropName = PropIt->GetName();
        if (PropName.Equals(Wanted, ESearchCase::IgnoreCase) ||
            PropName.Equals(Bare, ESearchCase::IgnoreCase) ||
            PropName.Equals(TEXT("b") + Bare, ESearchCase::IgnoreCase))
        {
            return FString::Printf(
                TEXT(" '%s' is a PROPERTY on %s, not a function - create it with "
                     "nodeType VariableGet or VariableSet and memberName '%s'."),
                *PropName, *Class->GetName(), *PropName);
        }
    }
    // Underscores do not count: VInterpToConstant asked for VInterpTo_Constant and was told nothing.
    const FString WantedKey = Wanted.Replace(TEXT("_"), TEXT("")).ToLower();
    const FString BareKey = Bare.Replace(TEXT("_"), TEXT("")).ToLower();
    TArray<FString> Close;
    for (TFieldIterator<UFunction> FuncIt(Class); FuncIt; ++FuncIt)
    {
        const FString Name = FuncIt->GetName();
        const FString Key = Name.Replace(TEXT("_"), TEXT("")).ToLower();
        if (Key.Contains(WantedKey) ||
            (Bare.Len() >= 4 && Key.Contains(BareKey)))
        {
            Close.AddUnique(Name);
        }
    }
    if (Close.Num() == 0)
    {
        return DescribeDeclaringClasses(Class, Wanted);
    }
    Close.Sort();
    if (Close.Num() > 8)
    {
        Close.SetNum(8);
    }
    return FString::Printf(TEXT(" Closest reflected names on %s: %s."),
                           *Class->GetName(), *FString::Join(Close, TEXT(", ")));
}

// "Function not found" hid the usual cause: the memberClass itself resolved nothing.
FString DescribeMissingFunction(UBlueprint* Blueprint, const FString& MemberName,
                                const FString& MemberClass, UClass* ResolvedClass)
{
    if (!MemberClass.IsEmpty() && !ResolvedClass)
    {
        return FString::Printf(TEXT("memberClass '%s' is not a class: pass a native class name (KismetMathLibrary) "
                                    "or a Blueprint asset path (/Game/Folder/BP_Name)."), *MemberClass);
    }
    UClass* HintClass = ResolvedClass ? ResolvedClass : (Blueprint ? Blueprint->GeneratedClass.Get() : nullptr);
    FString Hint = SuggestMemberFix(HintClass, MemberName);
    // Without a memberClass the stock libraries are searched too, so their near names are the likely fix.
    UClass* Libraries[] = {UKismetMathLibrary::StaticClass(), UKismetSystemLibrary::StaticClass(), UGameplayStatics::StaticClass()};
    for (int32 Index = 0; Hint.IsEmpty() && MemberClass.IsEmpty() && Index < UE_ARRAY_COUNT(Libraries); ++Index)
    {
        Hint = SuggestMemberFix(Libraries[Index], MemberName);
    }
    return FString::Printf(TEXT("Function '%s' not found.%s To find a function by what it does: inspect_graph info "
                                "functions, words saying what the node should do."), *MemberName, *Hint);
}

// The name may be exactly right and live on another class: a function library
// (RemoveAllWidgets is on UWidgetLayoutLibrary while every neighbouring widget
// helper is on UWidgetBlueprintLibrary) or a component class. Only libraries were
// searched, so SetScalarParameterValueOnMaterials asked of PrimitiveComponent came
// back a bare "not found" although MeshComponent declares it.
FString DescribeDeclaringClasses(UClass* Class, const FString& Wanted)
{
    const FName WantedName(*Wanted);
    TArray<UClass*> Declarers;
    for (TObjectIterator<UClass> It; It && Declarers.Num() < 4; ++It)
    {
        if (*It == Class || It->HasAnyClassFlags(CLASS_Deprecated) || McpIsCompileDebrisClass(*It))
        {
            continue;
        }
        const UFunction* Found = It->FindFunctionByName(WantedName, EIncludeSuperFlag::ExcludeSuper);
        if (Found && Found->HasAnyFunctionFlags(FUNC_BlueprintCallable))
        {
            Declarers.Add(*It);
        }
    }
    if (Declarers.Num() == 1)
    {
        return FString::Printf(TEXT(" '%s' is not on %s, but %s declares it - retry with memberClass '%s'."),
                               *Wanted, *Class->GetName(), *Declarers[0]->GetName(), *Declarers[0]->GetPathName());
    }
    TArray<FString> Paths;
    for (const UClass* Declarer : Declarers)
    {
        Paths.Add(Declarer->GetPathName());
    }
    return Paths.Num() == 0 ? FString() : FString::Printf(
        TEXT(" '%s' is not on %s, but these declare it: %s - retry with the memberClass of the object the node acts on."),
        *Wanted, *Class->GetName(), *FString::Join(Paths, TEXT(", ")));
}
}
