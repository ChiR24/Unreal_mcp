// Copyright (c) 2024 EngineWire Contributors
//
// What a changed property held, as exported text: before the first change the journal saw, and after the latest one. An
// edit in the editor's Details panel announces the property before it changes (OnPreObjectPropertyChanged, through
// PreEditChange with its property chain, inside the transaction it opened first); a change made in code announces only
// after, so it has no before. The cost stays bounded on the user's editing path: a slider drag's every-frame
// interactive changes are skipped (the drag ends with a value set), a container is kept as its item count and a large
// struct (post-process settings and the like) by name only.

#include "Core/Journal/McpChangeJournalState.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace McpChangeJournal
{
namespace
{
constexpr int32 MaxJournalValues = 8;
constexpr int32 MaxValueChars = 96;
constexpr int32 MaxStructBytes = 256;

FDelegateHandle GPreChangeHandle;

// False when the value is not kept: a struct too large to export on every edit.
bool ValueText(UObject* Object, const FProperty* Member, FString& OutText)
{
    const void* Value = Member->ContainerPtrToValuePtr<void>(Object);
    int32 Items = INDEX_NONE;
    if (const FArrayProperty* Array = CastField<FArrayProperty>(Member))
    {
        Items = FScriptArrayHelper(Array, Value).Num();
    }
    else if (const FSetProperty* Set = CastField<FSetProperty>(Member))
    {
        Items = FScriptSetHelper(Set, Value).Num();
    }
    else if (const FMapProperty* Map = CastField<FMapProperty>(Member))
    {
        Items = FScriptMapHelper(Map, Value).Num();
    }
    else if (CastField<FStructProperty>(Member) && Member->GetSize() > MaxStructBytes)
    {
        return false;
    }
    if (Items != INDEX_NONE)
    {
        OutText = FString::Printf(TEXT("%d item(s)"), Items);
        return true;
    }
    MCP_PROPERTY_EXPORT_TEXT(Member, OutText, Value, nullptr, Object, PPF_None);
    OutText = OutText.Len() > MaxValueChars ? OutText.Left(MaxValueChars) + TEXT("...") : OutText;
    return true;
}

// A change that names no property (an undo or a redo, a PostEditChange after an edit in code): every value kept for a
// property of Object's class is read again, so a row never shows a value the undo took back.
void RefreshValues(int32 Index, UObject* Object)
{
    for (FJournalValue& Kept : JournalState().Entries[Index].Values)
    {
        const FProperty* Member = Object->GetClass()->FindPropertyByName(Kept.Property);
        FString Text;
        if (Member && (Kept.bFrom || Kept.bTo) && ValueText(Object, Member, Text))
        {
            Kept.To = Text;
            Kept.bTo = true;
        }
    }
}

// The member about to change: the chain's member node (a struct field's struct), else its head.
void OnPreChange(UObject* Object, const FEditPropertyChain& Chain)
{
    const auto* Node = Chain.GetActiveMemberNode() ? Chain.GetActiveMemberNode() : Chain.GetHead();
    const int32 Index = Node && CurrentBy() != JournalByEditor ? EditedEntry(Object) : INDEX_NONE;
    if (Index != INDEX_NONE)
    {
        NoteValue(Index, Object, Node->GetValue(), true, EPropertyChangeType::Unspecified);
    }
}
} // namespace

void NoteValue(int32 Index, UObject* Object, const FProperty* Member, bool bBefore, uint32 ChangeType)
{
    if (!Member && !bBefore && Object)
    {
        RefreshValues(Index, Object);
        return;
    }
    // Only a member of the object's own class reads from it as a container (a local variable's owner is its function).
    const UClass* Owner = Member ? Member->GetOwner<UClass>() : nullptr;
    if (!Object || !Owner || !Object->IsA(Owner) || (!bBefore && (ChangeType & EPropertyChangeType::Interactive)))
    {
        return;
    }
    TArray<FJournalValue>& Values = JournalState().Entries[Index].Values;
    const FName Name = Member->GetFName();
    FJournalValue* Value = Values.FindByPredicate([&Name](const FJournalValue& Known) { return Known.Property == Name; });
    if ((!Value && Values.Num() >= MaxJournalValues) || (bBefore && Value && (Value->bFrom || Value->bTo)))
    {
        return; // full, or the first old value is kept already
    }
    FString Text;
    if (!ValueText(Object, Member, Text))
    {
        return;
    }
    if (!Value)
    {
        Value = &Values.AddDefaulted_GetRef();
        Value->Property = Name;
    }
    if (bBefore)
    {
        Value->From = Text;
        Value->bFrom = true;
    }
    else
    {
        Value->To = Text;
        Value->bTo = true;
    }
}

void BindValueEvents()
{
    GPreChangeHandle = FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddStatic(&OnPreChange);
}

void UnbindValueEvents()
{
    FCoreUObjectDelegates::OnPreObjectPropertyChanged.Remove(GPreChangeHandle);
}
} // namespace McpChangeJournal
