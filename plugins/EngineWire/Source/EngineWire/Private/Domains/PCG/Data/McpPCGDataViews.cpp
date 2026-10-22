#include "Domains/PCG/Data/McpPCGDataViews.h"

#if MCP_HAS_PCG_EXECUTION
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
namespace McpPCGHandlers
{
void FPCGDataViewStore::Purge()
{
    check(IsInGameThread());
    const double Now = FPlatformTime::Seconds();
    for (auto It = Entries.CreateIterator(); It; ++It)
        if (!It.Value().Component.IsValid() || Now >= It.Value().ExpiresAt) It.RemoveCurrent();
}

int64 FPCGDataViewStore::GetCharacterCount() const
{
    int64 Count = 0;
    for (const auto& Pair : Entries) if (Pair.Value.Rows.IsValid()) Count += Pair.Value.Rows->GetCharacterCount() + Pair.Value.EncodedDescription.Len();
    return Count;
}

bool FPCGDataViewStore::CheckCapacity(const FString& Principal, int64 Characters, FString& OutError) const
{
    int32 OwnerCount = 0;
    int64 OwnerCharacters = Characters;
    for (const auto& Pair : Entries)
    {
        if (Pair.Value.Owner != Principal) continue;
        ++OwnerCount;
        if (Pair.Value.Rows.IsValid()) OwnerCharacters += Pair.Value.Rows->GetCharacterCount() + Pair.Value.EncodedDescription.Len();
    }
    if (Principal.IsEmpty() || Entries.Num() >= MaxHandles || OwnerCount >= MaxOwnerHandles ||
        OwnerCharacters > MaxOwnerCharacters || GetCharacterCount() + Characters > MaxCharacters)
    {
        OutError = TEXT("Bounded data-view retention is unavailable; release owned views or wait for expiry.");
        return false;
    }
    return true;
}

FPCGDataViewStore::FEntry* FPCGDataViewStore::Find(const FString& Id, UPCGComponent* Component, const FString& Principal, FString& OutError)
{
    check(IsInGameThread());
    OutError.Reset();
    Purge();
    FEntry* Entry = Entries.Find(Id);
    if (!Entry || Entry->Owner != Principal || !Component || Entry->Component.Get() != Component)
    {
        OutError = TEXT("Data view is unavailable for this owner and exact component.");
        return nullptr;
    }
    if (!Entry->Root->Resolve(Principal, Component, OutError)) return nullptr;
    return Entry;
}

FString FPCGDataViewStore::RetainRoot(UPCGComponent* Component, const FString& Principal, int32 Slot, FString& OutError)
{
    check(IsInGameThread());
    OutError.Reset();
    Purge();
    if (!CheckCapacity(Principal, 0, OutError)) return FString();
    auto Root = CreatePCGRootDataView(Component, Principal, Slot, OutError);
    if (!Root.IsValid()) return FString();
    FEntry Entry;
    Entry.Owner = Principal;
    Entry.Component = Component;
    Entry.ExpiresAt = Root->GetExpiresAt();
    Entry.Root = MoveTemp(Root);
    const FString Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Entries.Add(Id, MoveTemp(Entry));
    return Id;
}

TSharedPtr<FPCGRootDataView> FPCGDataViewStore::ResolveRoot(const FString& Id, UPCGComponent* Component, const FString& Principal, FString& OutError)
{
    FEntry* Entry = Find(Id, Component, Principal, OutError);
    if (!Entry || Entry->Rows.IsValid())
    {
        if (Entry) OutError = TEXT("A root dataset locator, not a projected row view, is required.");
        return nullptr;
    }
    return Entry->Root;
}

FString FPCGDataViewStore::RetainRows(const FString& RootId, UPCGComponent* Component, const FString& Principal,
    const TArray<TSharedPtr<FJsonValue>>& Rows, FString& OutError, const FString& EncodedDescription)
{
    if (EncodedDescription.Len() > MaxDescriptionCharacters)
    {
        OutError = TEXT("Projection description exceeds its bounded size.");
        return FString();
    }
    FEntry* Parent = Find(RootId, Component, Principal, OutError);
    if (!Parent) return FString();
    if (Parent->Rows.IsValid())
    {
        OutError = TEXT("A new projection requires a root locator, not an existing row view.");
        return FString();
    }
    if (!CheckCapacity(Principal, 0, OutError)) return FString();
    const auto Root = Parent->Root;
    const UPCGData* Data = Root->Resolve(Principal, Component, OutError);
    if (!Data) return FString();
    auto Snapshot = CreatePCGDataSnapshot(Component, Data, Principal, Rows, OutError);
    if (!Snapshot.IsValid() || !CheckCapacity(Principal, Snapshot->GetCharacterCount() + EncodedDescription.Len(), OutError)) return FString();
    if (!Root->Resolve(Principal, Component, OutError)) return FString();
    FEntry Entry;
    Entry.Owner = Principal;
    Entry.Component = Component;
    Entry.Root = Root;
    Entry.ExpiresAt = FMath::Min(Root->GetExpiresAt(), Snapshot->GetExpiresAt());
    Entry.EncodedDescription = EncodedDescription;
    Entry.Rows = MoveTemp(Snapshot);
    const FString Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Entries.Add(Id, MoveTemp(Entry));
    return Id;
}

bool FPCGDataViewStore::DescribeRows(const FString& Id, UPCGComponent* Component, const FString& Principal,
    TSharedPtr<FJsonObject>& OutDescription, double& OutRemainingSeconds, FString& OutError)
{
    OutDescription.Reset();
    OutRemainingSeconds = 0;
    FEntry* Entry = Find(Id, Component, Principal, OutError);
    if (!Entry || !Entry->Rows.IsValid() || Entry->EncodedDescription.IsEmpty())
    {
        if (Entry) OutError = TEXT("A typed projected row view is required.");
        return false;
    }
    TSharedPtr<FJsonObject> Description;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Entry->EncodedDescription), Description) || !Description.IsValid())
    {
        OutError = TEXT("The bounded projection description could not be decoded.");
        return false;
    }
    OutRemainingSeconds = FMath::Max(0.0, Entry->ExpiresAt - FPlatformTime::Seconds());
    if (OutRemainingSeconds <= 0)
    {
        OutError = TEXT("The projected view expired while reading its description.");
        return false;
    }
    OutDescription = MoveTemp(Description);
    return true;
}

bool FPCGDataViewStore::ReadRows(const FString& Id, UPCGComponent* Component, const FString& Principal,
    int32 Offset, int32 Limit, TArray<TSharedPtr<FJsonValue>>& OutRows, int32& OutTotal, FString& OutError)
{
    OutRows.Reset();
    OutTotal = 0;
    FEntry* Entry = Find(Id, Component, Principal, OutError);
    if (!Entry || !Entry->Rows.IsValid())
    {
        if (Entry) OutError = TEXT("An immutable projected row view, not a live root locator, is required.");
        return false;
    }
    if (!Entry->Rows->ReadPage(Principal, Component, Offset, Limit, OutRows, OutError)) return false;
    OutTotal = Entry->Rows->GetRowCount();
    return true;
}

bool FPCGDataViewStore::Release(const FString& Id, UPCGComponent* Component, const FString& Principal)
{
    check(IsInGameThread());
    Purge();
    const FEntry* Found = Entries.Find(Id);
    if (!Found || Found->Owner != Principal || Found->Component.Get() != Component) return false;
    const auto Root = Found->Root;
    const bool bRoot = !Found->Rows.IsValid();
    Entries.Remove(Id);
    if (bRoot)
        for (auto It = Entries.CreateIterator(); It; ++It)
            if (It.Value().Root == Root) It.RemoveCurrent();
    return true;
}
}
#endif
