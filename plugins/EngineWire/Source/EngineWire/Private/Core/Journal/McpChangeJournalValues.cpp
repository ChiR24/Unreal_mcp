// Copyright (c) 2024 EngineWire Contributors
//
// What a changed property held, as exported text: before the first change the journal saw, and after the latest one. An
// edit in the editor's Details panel announces the property before it changes (OnPreObjectPropertyChanged, through
// PreEditChange with its property chain); a change made in code announces only after, so it has no before.

#include "Core/Journal/McpChangeJournalState.h"

#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace McpChangeJournal
{
namespace
{
constexpr int32 MaxJournalValues = 8;
constexpr int32 MaxValueChars = 96;

FDelegateHandle GPreChangeHandle;

FString ValueText(UObject* Object, const FProperty* Member)
{
    FString Text;
    Member->ExportText_InContainer(0, Text, Object, nullptr, Object, PPF_None);
    return Text.Len() > MaxValueChars ? Text.Left(MaxValueChars) + TEXT("...") : Text;
}

// The member about to change: the chain's member node (a struct field's struct), else its head.
void OnPreChange(UObject* Object, const FEditPropertyChain& Chain)
{
    const auto* Node = Chain.GetActiveMemberNode() ? Chain.GetActiveMemberNode() : Chain.GetHead();
    const int32 Index = Node && CurrentBy() != JournalByEditor ? EditedEntry(Object) : INDEX_NONE;
    if (Index != INDEX_NONE)
    {
        NoteValue(Index, Object, Node->GetValue(), true);
    }
}
} // namespace

void NoteValue(int32 Index, UObject* Object, const FProperty* Member, bool bBefore)
{
    // Only a member of the object's own class reads from it as a container (a local variable's owner is its function).
    const UClass* Owner = Member ? Member->GetOwner<UClass>() : nullptr;
    if (!Object || !Owner || !Object->IsA(Owner))
    {
        return;
    }
    TArray<FJournalValue>& Values = JournalState().Entries[Index].Values;
    const FName Name = Member->GetFName();
    FJournalValue* Value = Values.FindByPredicate([&Name](const FJournalValue& Known) { return Known.Property == Name; });
    if (!Value && Values.Num() >= MaxJournalValues)
    {
        return;
    }
    if (!Value)
    {
        Value = &Values.AddDefaulted_GetRef();
        Value->Property = Name;
    }
    if (bBefore && !Value->bFrom && !Value->bTo)
    {
        Value->From = ValueText(Object, Member); // the first old value: later edits keep it
        Value->bFrom = true;
    }
    else if (!bBefore)
    {
        Value->To = ValueText(Object, Member);
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
