// McpNestedPropElementsPick.cpp — the path parser and element picker of McpResolveElementPath (see McpNestedPropElements.h).

#include "Foundation/BridgeHelpers/Properties/McpNestedPropElementsPick.h"
#include "Foundation/BridgeHelpers/Security/McpSafeReflectionTarget.h"
#include "Core/Compatibility/McpVersionCompatibility.h"

namespace McpPropElems
{
namespace
{
constexpr int32 McpElementListedKeys = 20;

bool IsIndex(const FString& Text)
{
    for (const TCHAR Char : Text)
    {
        if (!FChar::IsDigit(Char))
        {
            return false;
        }
    }
    return !Text.IsEmpty();
}

FString TextOf(const FProperty* Property, const void* Container)
{
    FString Text;
    MCP_PROPERTY_EXPORT_TEXT(Property, Text, Property->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
    return Text;
}

// The text of a member path (ParameterInfo.Name) inside the element ElementProperty holds at Container; false when a
// part is no member, or when it crosses a reference the walk may not enter.
bool MemberText(FProperty* ElementProperty, void* Container, const FString& Field, FString& OutText)
{
    TArray<FString> Parts;
    Field.ParseIntoArray(Parts, TEXT("."), true);
    FProperty* At = ElementProperty;
    void* Holder = Container;
    for (const FString& Part : Parts)
    {
        UStruct* Scope = nullptr;
        void* Value = At->ContainerPtrToValuePtr<void>(Holder);
        if (const FStructProperty* Struct = CastField<FStructProperty>(At))
        {
            Scope = Struct->Struct;
        }
        else if (const FObjectProperty* Reference = CastField<FObjectProperty>(At))
        {
            UObject* Held = Reference->GetObjectPropertyValue(Value);
            if (!Held || !McpSafeReflectionTarget::IsAddressable(Held)) return false;
            Scope = Held->GetClass();
            Value = Held;
        }
        At = Scope ? FindFProperty<FProperty>(Scope, FName(*Part)) : nullptr;
        if (!At) return false;
        Holder = Value;
    }
    OutText = TextOf(At, Holder);
    return Parts.Num() > 0;
}

} // namespace

// Splits at the dots outside brackets. A bracket holds an index, a key or Field=Value; "..." quotes, and a backslash
// takes the next character as it is (\" \] \\).
bool Split(const FString& Path, TArray<FSegment>& OutSegments, FString& OutError)
{
    FSegment Segment;
    FString* Into = &Segment.Name;
    bool bInside = false, bQuoted = false, bClosed = false, bBad = false;
    const auto Close = [&]()
    {
        bBad |= Segment.Name.IsEmpty() || (Segment.bSelector && (Segment.Key.IsEmpty() || (Segment.bKeyed && Segment.Value.IsEmpty())));
        OutSegments.Add(Segment);
        Segment = FSegment();
        Into = &Segment.Name;
        bClosed = false;
    };
    for (int32 At = 0; At < Path.Len() && !bBad; ++At)
    {
        const TCHAR Char = Path[At];
        if ((bInside || bQuoted) && Char == TEXT('\\') && At + 1 < Path.Len())
        {
            Into->AppendChar(Path[++At]);
        }
        else if (bQuoted)
        {
            bQuoted = Char != TEXT('"');
            if (bQuoted) Into->AppendChar(Char);
        }
        else if (bInside)
        {
            if (Char == TEXT('"')) bQuoted = true;
            else if (Char == TEXT(']')) { bInside = false; bClosed = true; }
            else if (Char == TEXT('=') && !Segment.bKeyed) { Segment.bKeyed = true; Into = &Segment.Value; }
            else Into->AppendChar(Char);
        }
        else if (Char == TEXT('[') && !bClosed && !Segment.Name.IsEmpty())
        {
            bInside = Segment.bSelector = true;
            Into = &Segment.Key;
        }
        else if (Char == TEXT('.'))
        {
            Close();
        }
        else
        {
            bBad = bClosed || Char == TEXT('[') || Char == TEXT(']');
            Into->AppendChar(Char);
        }
    }
    bBad |= bInside || bQuoted;
    if (!bBad)
    {
        Close();
    }
    if (bBad)
    {
        OutError = FString::Printf(TEXT("'%s' is not a property path: each part is a name with at most one [element] after "
            "it ([2], [key] or [Field=Value], quoted with \"...\"), parts joined by dots"), *Path);
    }
    return !bBad;
}

// Picks one element of the list, map or set Property holds at Container; OutProperty and OutContainer become the
// element's. A set's element is its own key: bOutReadOnly.
bool Pick(FProperty* Property, void* Container, const FSegment& Segment, FProperty*& OutProperty, void*& OutContainer,
          bool& bOutReadOnly, FMcpElementPath& Out)
{
    const FString& Wanted = Segment.bKeyed ? Segment.Value : Segment.Key;
    const FString Where = Property->GetName();
    TArray<TPair<int32, uint8*>> Matches;
    TArray<FString> Present, CaseOnly;
    int32 Seen = 0;
    const auto Consider = [&](int32 Index, uint8* Element, const FString& Text)
    {
        if (Text.Equals(Wanted, ESearchCase::CaseSensitive)) Matches.Emplace(Index, Element);
        else if (Text.Equals(Wanted, ESearchCase::IgnoreCase)) CaseOnly.Add(Text);
        if (Present.Num() < McpElementListedKeys) Present.Add(Text);
        ++Seen;
    };
    if (FArrayProperty* Array = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper Helper(Array, Array->ContainerPtrToValuePtr<void>(Container));
        OutProperty = Array->Inner;
        if (!Segment.bKeyed)
        {
            if (!IsIndex(Segment.Key))
            {
                return Fail(Out, TEXT("PROPERTY_ELEMENT_NOT_FOUND"), FString::Printf(TEXT("%s is a list: name an entry by "
                    "its index (%s[0]) or by one of its fields (%s[Name=Value])"), *Where, *Where, *Where));
            }
            const int32 Index = FCString::Atoi(*Segment.Key);
            if (Index >= Helper.Num())
            {
                return Fail(Out, TEXT("PROPERTY_ELEMENT_NOT_FOUND"), FString::Printf(TEXT("%s has %d entries; index %d is "
                    "past them"), *Where, Helper.Num(), Index));
            }
            OutContainer = Helper.GetRawPtr(Index);
            return true;
        }
        for (int32 Index = 0; Index < Helper.Num(); ++Index)
        {
            FString Text;
            if (!MemberText(Array->Inner, Helper.GetRawPtr(Index), Segment.Key, Text))
            {
                return Fail(Out, TEXT("PROPERTY_ELEMENT_NOT_FOUND"), FString::Printf(TEXT("The entries of %s have no "
                    "field '%s' to match on"), *Where, *Segment.Key));
            }
            Consider(Index, Helper.GetRawPtr(Index), Text);
        }
    }
    else if (FMapProperty* Map = CastField<FMapProperty>(Property))
    {
        FScriptMapHelper Helper(Map, Map->ContainerPtrToValuePtr<void>(Container));
        OutProperty = Map->ValueProp;
        for (int32 Index = 0; !Segment.bKeyed && Index < Helper.GetMaxIndex(); ++Index)
        {
            if (Helper.IsValidIndex(Index)) Consider(Index, Helper.GetPairPtr(Index), TextOf(Map->KeyProp, Helper.GetPairPtr(Index)));
        }
    }
    else if (FSetProperty* Set = CastField<FSetProperty>(Property))
    {
        FScriptSetHelper Helper(Set, Set->ContainerPtrToValuePtr<void>(Container));
        OutProperty = Set->ElementProp;
        bOutReadOnly = true;
        for (int32 Index = 0; !Segment.bKeyed && Index < Helper.GetMaxIndex(); ++Index)
        {
            if (Helper.IsValidIndex(Index)) Consider(Index, Helper.GetElementPtr(Index), TextOf(Set->ElementProp, Helper.GetElementPtr(Index)));
        }
    }
    else
    {
        return Fail(Out, TEXT("PROPERTY_ELEMENT_NOT_FOUND"), FString::Printf(TEXT("%s is not a list, map or set, so it "
            "takes no [element]"), *Where));
    }
    if (Segment.bKeyed && !CastField<FArrayProperty>(Property))
    {
        return Fail(Out, TEXT("PROPERTY_ELEMENT_NOT_FOUND"), FString::Printf(TEXT("%s takes its key alone: %s[key]"), *Where, *Where));
    }
    if (Matches.Num() == 1)
    {
        OutContainer = Matches[0].Value;
        return true;
    }
    if (Matches.Num() > 1)
    {
        TArray<FString> Indices;
        for (const TPair<int32, uint8*>& Match : Matches) Indices.Add(FString::FromInt(Match.Key));
        return Fail(Out, TEXT("AMBIGUOUS_KEY_MATCH"), FString::Printf(TEXT("'%s' matches %d entries of %s (indices %s); "
            "nothing was read or written: name one by its index"), *Wanted, Matches.Num(), *Where, *FString::Join(Indices, TEXT(", "))));
    }
    return Fail(Out, TEXT("PROPERTY_ELEMENT_NOT_FOUND"), FString::Printf(TEXT("No entry of %s is '%s'. Present: %s%s%s"),
        *Where, *Wanted, Present.IsEmpty() ? TEXT("(none)") : *FString::Join(Present, TEXT(", ")),
        Seen > Present.Num() ? *FString::Printf(TEXT(" and %d more"), Seen - Present.Num()) : TEXT(""),
        CaseOnly.IsEmpty() ? TEXT("") : *(TEXT("; differs only in case: ") + FString::Join(CaseOnly, TEXT(", ")))));
}
} // namespace McpPropElems
