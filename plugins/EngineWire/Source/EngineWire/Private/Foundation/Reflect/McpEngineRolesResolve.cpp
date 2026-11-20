// McpEngineRolesResolve.cpp — the stages for one role (see McpEngineRolesMatch.h). Same order and margin as
// .compat/check-roles.mjs, which runs them over the engine header trees.
#include "Foundation/Reflect/McpEngineRolesMatch.h"

#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "UObject/Class.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

using McpEngineRoles::FRoleHit;

namespace
{
constexpr float WordMargin = 0.15f;

FRoleHit Found(const FString& Stage, FString Evidence)
{
    FRoleHit Hit;
    Hit.Stage = Stage;
    Hit.Evidence = MoveTemp(Evidence);
    return Hit;
}

TArray<FString> WantedWords(const TSharedPtr<FJsonObject>& Row, const FString& Name)
{
    TArray<FString> Want;
    const TArray<TSharedPtr<FJsonValue>>* Words = nullptr;
    if (Row->TryGetArrayField(TEXT("words"), Words) && Words)
    {
        for (const TSharedPtr<FJsonValue>& Word : *Words)
        {
            Want.Add(Word.IsValid() ? Word->AsString().ToLower() : FString());
        }
    }
    return Want.Num() > 0 ? Want : McpEngineRolesMatch::Words(Name);
}

FRoleHit ResolveFunction(UStruct* OwnerType, const FString& Name, FName Renamed)
{
    UClass* Class = Cast<UClass>(OwnerType);
    for (const FName Candidate : {FName(*Name), Renamed})
    {
        if (UFunction* Function = Class ? Class->FindFunctionByName(Candidate) : nullptr)
        {
            FRoleHit Hit = Found(Candidate == FName(*Name) ? TEXT("exact") : TEXT("redirect"), Candidate.ToString());
            Hit.Function = Function;
            return Hit;
        }
    }
    return Found(FString(), FString::Printf(TEXT("%s has no function %s"), *OwnerType->GetName(), *Name));
}
}

namespace McpEngineRolesMatch
{
FRoleHit ResolveType(const TSharedPtr<FJsonObject>& Row, bool bClass)
{
    const FString Path = GetJsonStringField(Row, TEXT("path"), TEXT(""));
    auto Fits = [bClass](UStruct* Type) { return Type && (bClass ? Type->IsA<UClass>() : Type->IsA<UScriptStruct>()); };
    UStruct* Type = Path.IsEmpty() ? nullptr : FindObject<UStruct>(nullptr, *Path);
    if (Fits(Type))
    {
        FRoleHit Hit = Found(TEXT("exact"), Path);
        Hit.Type = Type;
        return Hit;
    }
    const FCoreRedirectObjectName Old(Path);
    const FCoreRedirectObjectName New =
        FCoreRedirects::GetRedirectedName(bClass ? ECoreRedirectFlags::Type_Class : ECoreRedirectFlags::Type_Struct, Old);
    Type = New == Old ? nullptr : FindObject<UStruct>(nullptr, *New.ToString());
    if (Fits(Type))
    {
        FRoleHit Hit = Found(TEXT("redirect"), FString::Printf(TEXT("%s, renamed from %s"), *New.ToString(), *Path));
        Hit.Type = Type;
        return Hit;
    }
    return Found(FString(), FString::Printf(TEXT("no %s at %s (its plugin may be off) and no rename of it"),
                                            bClass ? TEXT("class") : TEXT("struct"), *Path));
}

FRoleHit ResolveMember(const TSharedPtr<FJsonObject>& Row, const FString& Kind)
{
    const FString OwnerId = GetJsonStringField(Row, TEXT("owner"), TEXT(""));
    const FString Name = GetJsonStringField(Row, TEXT("name"), TEXT(""));
    const FRoleHit Owner = McpEngineRoles::Resolve(FName(*OwnerId));
    if (!Owner.Type)
    {
        return Found(FString(), FString::Printf(TEXT("its owner %s is not here: %s"), *OwnerId, *Owner.Evidence));
    }
    UStruct* OwnerType = Owner.Type;
    const FCoreRedirectObjectName Old(FName(*Name), OwnerType->GetFName(), OwnerType->GetOutermost()->GetFName());
    if (Kind == TEXT("function"))
    {
        return ResolveFunction(OwnerType, Name, FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Function, Old).ObjectName);
    }
    const TSharedPtr<FJsonObject>* SpecPtr = nullptr;
    const TSharedPtr<FJsonObject> Spec = Row->TryGetObjectField(TEXT("type"), SpecPtr) && SpecPtr ? *SpecPtr : nullptr;
    auto FitsSpec = [&Spec](const FProperty* P)
    { return McpEngineRolesMatch::Fits(Spec, P, [](FName Id) { return McpEngineRoles::TypeOf(Id); }); };
    const FName Renamed = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Property, Old).ObjectName;
    for (const FName Candidate : {FName(*Name), Renamed})
    {
        FProperty* Property = FindFProperty<FProperty>(OwnerType, Candidate);
        if (Property && FitsSpec(Property))
        {
            FRoleHit Hit = Found(Candidate == FName(*Name) ? TEXT("exact") : TEXT("redirect"), Candidate.ToString());
            Hit.Property = Property;
            return Hit;
        }
    }
    TArray<FProperty*> Typed;
    for (TFieldIterator<FProperty> It(OwnerType); Spec.IsValid() && It; ++It)
    {
        if (FitsSpec(*It))
        {
            Typed.Add(*It);
        }
    }
    if (Typed.Num() == 1)
    {
        FRoleHit Hit = Found(TEXT("shape"), FString::Printf(TEXT("%s, the one member of %s whose type fits"), *Typed[0]->GetName(), *OwnerType->GetName()));
        Hit.Property = Typed[0];
        return Hit;
    }
    const TArray<FString> Want = WantedWords(Row, Name);
    Typed.Sort([&Want](const FProperty& A, const FProperty& B) { return Overlap(A.GetName(), Want) > Overlap(B.GetName(), Want); });
    if (Typed.Num() > 1 && Overlap(Typed[0]->GetName(), Want) - Overlap(Typed[1]->GetName(), Want) >= WordMargin)
    {
        FRoleHit Hit = Found(TEXT("words"), FString::Printf(TEXT("%s, whose name is closest of the %d that fit"), *Typed[0]->GetName(), Typed.Num()));
        Hit.Property = Typed[0];
        return Hit;
    }
    TArray<FString> Names;
    for (const FProperty* Property : Typed)
    {
        Names.Add(Property->GetName());
    }
    return Found(FString(), Typed.Num() > 1
        ? FString::Printf(TEXT("AMBIGUOUS: %s all fit and none is clearly closer"), *FString::Join(Names, TEXT(", ")))
        : FString::Printf(TEXT("%s has no %s and no member whose type fits"), *OwnerType->GetName(), *Name));
}
}
