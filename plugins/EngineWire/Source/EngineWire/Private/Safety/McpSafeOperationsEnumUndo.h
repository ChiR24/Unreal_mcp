#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "Misc/Change.h"
#include "Misc/ITransaction.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

// An enum edit recorded as a change undo can apply, never as the enum's saved state. From UE 5.7 restoring an enum from
// the undo buffer crashes the editor: UEnum::Serialize loads the values over the ones the enum holds, which
// UEnum::FNameData::Initialize asserts against (check(NumValues == 0)); 5.0-5.6 load them whole. The engine's own enum
// edits Modify the enum (FEnumEditorUtils::PrepareForChange), which records that saved state. While an FMcpEnumUndo is
// open the enum is not transactional, so those Modify calls record nothing; when it closes, the enum's values and
// reflected properties before and after go into the open transaction as one change that undo and redo write back with
// SetEnums, and the enum's own PostEditUndo then tells the editor, as after the engine's undo. Every 5.x takes this path.
namespace McpSafeOperations
{
// An enum's values (every entry but the _MAX the engine keeps last) and each of its reflected properties as text.
struct FMcpEnumState
{
    TArray<TPair<FName, int64>> Names;
    TArray<FString> Properties;

    static FMcpEnumState Read(UEnum* Enum)
    {
        FMcpEnumState State;
        for (int32 Index = 0; Index + 1 < Enum->NumEnums(); ++Index)
        {
            State.Names.Emplace(Enum->GetNameByIndex(Index), Enum->GetValueByIndex(Index));
        }
        for (TFieldIterator<FProperty> It(Enum->GetClass()); It; ++It)
        {
            FString Text;
            MCP_PROPERTY_EXPORT_TEXT(*It, Text, It->ContainerPtrToValuePtr<void>(Enum), nullptr, Enum, PPF_None);
            State.Properties.Add(MoveTemp(Text));
        }
        return State;
    }

    void Write(UEnum* Enum) const
    {
        TArray<TPair<FName, int64>> Values = Names;
        MCP_SET_ENUMS(Enum, Values, Enum->GetCppForm());
        int32 Index = 0;
        for (TFieldIterator<FProperty> It(Enum->GetClass()); It && Index < Properties.Num(); ++It, ++Index)
        {
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
            It->ImportText_Direct(*Properties[Index], It->ContainerPtrToValuePtr<void>(Enum), Enum, PPF_None);
#else
            It->ImportText(*Properties[Index], It->ContainerPtrToValuePtr<void>(Enum), PPF_None, Enum);
#endif
        }
    }

    bool operator==(const FMcpEnumState& Other) const
    {
        return Names == Other.Names && Properties == Other.Properties;
    }
};

// Undo writes the state before the edit back, redo the state after it. An enum deleted since reads as null: nothing to do.
class FMcpEnumChange final : public FCommandChange
{
public:
    FMcpEnumChange(FMcpEnumState&& InBefore, FMcpEnumState&& InAfter) : Before(MoveTemp(InBefore)), After(MoveTemp(InAfter)) {}

    virtual void Apply(UObject* Object) override
    {
        if (UEnum* Enum = Cast<UEnum>(Object))
        {
            After.Write(Enum);
        }
    }

    virtual void Revert(UObject* Object) override
    {
        if (UEnum* Enum = Cast<UEnum>(Object))
        {
            Before.Write(Enum);
        }
    }

    virtual FString ToString() const override { return TEXT("Enum values and properties"); }

private:
    FMcpEnumState Before;
    FMcpEnumState After;
};

// The transactions an FMcpEnumUndo recorded an enum change in: undoing or redoing them never restores an enum's saved
// state, so the undo guard (McpStepRestoresEnum) lets them through.
inline TSet<FGuid>& McpEnumChangeSteps()
{
    static TSet<FGuid> Steps;
    return Steps;
}

// Open from before an enum is changed until the change is made, inside the transaction that is to hold it.
class FMcpEnumUndo
{
public:
    explicit FMcpEnumUndo(UEnum* InEnum)
        : Enum(InEnum), bWasTransactional(InEnum->HasAnyFlags(RF_Transactional)), Before(FMcpEnumState::Read(InEnum))
    {
        Enum->ClearFlags(RF_Transactional);
    }

    ~FMcpEnumUndo()
    {
        if (bWasTransactional)
        {
            Enum->SetFlags(RF_Transactional);
        }
        FMcpEnumState After = FMcpEnumState::Read(Enum);
        if (GUndo && !(After == Before))
        {
            GUndo->StoreUndo(Enum, MakeUnique<FMcpEnumChange>(MoveTemp(Before), MoveTemp(After)));
            McpEnumChangeSteps().Add(GUndo->GetContext().TransactionId);
        }
    }

    FMcpEnumUndo(const FMcpEnumUndo&) = delete;
    FMcpEnumUndo& operator=(const FMcpEnumUndo&) = delete;

private:
    UEnum* Enum;
    bool bWasTransactional;
    FMcpEnumState Before;
};
} // namespace McpSafeOperations
