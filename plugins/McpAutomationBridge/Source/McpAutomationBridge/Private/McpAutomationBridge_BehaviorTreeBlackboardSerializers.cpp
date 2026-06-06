#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BehaviorTreeSerializers.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Class.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"

namespace McpBehaviorTreeSerializers
{

static TSharedRef<FJsonObject> SerializeBlackboardEntry(
    const FBlackboardEntry& Entry, UBlackboardData* Source, UBlackboardData* SelfBB)
{
    TSharedRef<FJsonObject> KeyObj = MakeShared<FJsonObject>();

    // --- PR0a-pinned (must stay bit-identical) ---
    KeyObj->SetStringField(TEXT("name"), Entry.EntryName.ToString());
    KeyObj->SetStringField(TEXT("type"),
        Entry.KeyType ? Entry.KeyType->GetClass()->GetName() : TEXT("Unknown")); // BlackboardKeyType_*
    KeyObj->SetBoolField(TEXT("instanceSynced"), Entry.bInstanceSynced != 0);

    // --- additive enrichment (paths only set when non-null; otherwise omitted) ---
    if (const UBlackboardKeyType_Object* ObjKey = Cast<UBlackboardKeyType_Object>(Entry.KeyType))
    {
        if (ObjKey->BaseClass) { KeyObj->SetStringField(TEXT("baseClass"), ObjKey->BaseClass->GetPathName()); }
    }
    else if (const UBlackboardKeyType_Class* ClassKey = Cast<UBlackboardKeyType_Class>(Entry.KeyType))
    {
        if (ClassKey->BaseClass) { KeyObj->SetStringField(TEXT("baseClass"), ClassKey->BaseClass->GetPathName()); }
    }
    else if (const UBlackboardKeyType_Enum* EnumKey = Cast<UBlackboardKeyType_Enum>(Entry.KeyType))
    {
        if (EnumKey->EnumType)        { KeyObj->SetStringField(TEXT("enumClass"), EnumKey->EnumType->GetPathName()); }
        if (!EnumKey->EnumName.IsEmpty()) { KeyObj->SetStringField(TEXT("enumName"), EnumKey->EnumName); }
    }

#if WITH_EDITORONLY_DATA
    if (!Entry.EntryCategory.IsNone())
    {
        KeyObj->SetStringField(TEXT("entryCategory"), Entry.EntryCategory.ToString());
    }
#endif

    KeyObj->SetStringField(TEXT("sourceBlackboard"), Source ? Source->GetPathName() : FString());
    KeyObj->SetBoolField(TEXT("inherited"), Source != SelfBB);
    return KeyObj;
}

void SerializeBlackboardData(UBlackboardData* BB, const TSharedRef<FJsonObject>& Out)
{
    if (!BB)
    {
        Out->SetNumberField(TEXT("keyCount"), 0);
        Out->SetArrayField(TEXT("blackboardKeys"), TArray<TSharedPtr<FJsonValue>>());
        return;
    }

    // Walk the Parent chain (R4 cycle defense: max depth 32 + visited set).
    // Chain ends up ordered parent-most ... self (self last), so inherited keys
    // are emitted before own keys (spec 2.6).
    TArray<UBlackboardData*> Chain;
    {
        TSet<const UBlackboardData*> Visited;
        UBlackboardData* Cur = BB;
        int32 Depth = 0;
        while (Cur && Depth < 32 && !Visited.Contains(Cur))
        {
            Visited.Add(Cur);
            Chain.Insert(Cur, 0);     // prepend
            Cur = Cur->Parent;
            ++Depth;
        }
    }

    if (BB->Parent)
    {
        Out->SetStringField(TEXT("parentBlackboard"), BB->Parent->GetPathName());
    }

    // Override-aware emit: UE auto-adds `SelfActor` to EVERY Blackboard, and a child may
    // redefine a parent key. A naive concat would double-count `SelfActor` and emit duplicate
    // overridden keys (inflating keyCount). Mimic UBlackboardData::UpdateParentKeys: for each
    // key NAME, the most-derived definer wins. Chain is ordered parent-most .. self, so the
    // LAST occurrence of a name is the most-derived. Emit in parent-first order, skipping any
    // key whose name is redefined by a more-derived blackboard.
    TMap<FName, const UBlackboardData*> MostDerived;
    for (UBlackboardData* Source : Chain)
    {
        for (const FBlackboardEntry& Entry : Source->Keys)
        {
            MostDerived.Add(Entry.EntryName, Source); // later (more-derived) overwrites earlier
        }
    }

    TArray<TSharedPtr<FJsonValue>> Keys;
    for (UBlackboardData* Source : Chain)
    {
        for (const FBlackboardEntry& Entry : Source->Keys)
        {
            if (MostDerived.FindRef(Entry.EntryName) != Source)
            {
                continue; // this name is overridden by a more-derived blackboard
            }
            Keys.Add(MakeShared<FJsonValueObject>(SerializeBlackboardEntry(Entry, Source, BB)));
        }
    }
    Out->SetNumberField(TEXT("keyCount"), Keys.Num()); // == blackboardKeys.length (F7)
    Out->SetArrayField(TEXT("blackboardKeys"), Keys);
}


}
