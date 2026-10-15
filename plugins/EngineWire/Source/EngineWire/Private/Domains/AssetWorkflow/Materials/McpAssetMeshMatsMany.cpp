// Copyright (c) 2024 EngineWire Contributors
//
// process_asset process=mesh_materials with assetPaths: the same entries on several meshes, as a whole Fab pack needs.
// Its meshes hold different slots ("transparency" on one, only "Material_0" on the next), so a slot a mesh lacks is
// passed over for that mesh. An entry that no mesh has is a typo and fails the call; a material that does not load
// fails it before any mesh changes. Setting 32 meshes one call each took 64 calls with their consents.

#include "Domains/AssetWorkflow/Materials/McpAssetMeshMats.h"
#include "Domains/AssetWorkflow/Materials/McpAssetMeshMatSlots.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetSaveRegistry.h"

namespace McpMeshMaterials
{
namespace
{
FString JoinCapped(const TArray<FString>& Lines)
{
    constexpr int32 MaxQuoted = 3;
    TArray<FString> Quoted;
    for (int32 Line = 0; Line < Lines.Num() && Line < MaxQuoted; ++Line) Quoted.Add(Lines[Line]);
    if (Lines.Num() > MaxQuoted) Quoted.Add(FString::Printf(TEXT("%d more"), Lines.Num() - MaxQuoted));
    return FString::Join(Quoted, TEXT("; "));
}

// One mesh of the list: its row, and the entries that named a slot it has.
void SetOnOneOfMany(const FString& Path, const TArray<TPair<TSharedPtr<FJsonValue>, UMaterialInterface*>>& Wanted, bool bSave,
                    TArray<bool>& Used, TArray<TSharedPtr<FJsonValue>>& Rows, TArray<FString>& Problems, int32& TotalChanged)
{
    TSharedPtr<FJsonObject> Row = McpHandlerUtils::CreateResultObject();
    Row->SetStringField(TEXT("assetPath"), Path);
    Rows.Add(MakeShared<FJsonValueObject>(Row));
    const FString SafePath = SanitizeProjectRelativePath(Path);
    UObject* Asset = SafePath.IsEmpty() ? nullptr : McpLoadAsset(SafePath);
    FMeshSlots Slots;
    Slots.Static = Cast<UStaticMesh>(Asset);
    Slots.Skeletal = Cast<USkeletalMesh>(Asset);
    if (!Slots.Static && !Slots.Skeletal)
    {
        const FString Error = SafePath.IsEmpty() ? McpPathRefusalMessage(TEXT("assetPaths"), Path)
                              : Asset         ? FString::Printf(TEXT("%s is a %s, not a mesh"), *Path, *Asset->GetClass()->GetName())
                                              : FString::Printf(TEXT("no mesh asset at %s"), *Path);
        Row->SetStringField(TEXT("error"), Error);
        Problems.Add(Error);
        return;
    }
    TArray<FAssignment> Assignments;
    int32 Changed = 0;
    for (int32 Entry = 0; Entry < Wanted.Num(); ++Entry)
    {
        FAssignment Assignment;
        FString Reason, Code;
        if (!ResolveSlot(Slots, Wanted[Entry].Key, Assignment.Index, Reason, Code)) continue;
        Assignment.Material = Wanted[Entry].Value;
        Changed += Slots.Material(Assignment.Index) != Assignment.Material ? 1 : 0;
        Assignments.Add(Assignment);
        Used[Entry] = true;
    }
    if (Changed > 0)
    {
        ApplyAssignments(Slots, Assignments);
        Asset->MarkPackageDirty();
    }
    Row->SetNumberField(TEXT("changed"), Changed);
    TotalChanged += Changed;
    const bool bWritable = bSave && !Asset->GetOutermost()->GetName().StartsWith(TEXT("/Engine/"));
    if (bWritable && !SaveLoadedAssetThrottled(Asset))
    {
        Row->SetBoolField(TEXT("saved"), false);
        Problems.Add(FString::Printf(TEXT("%s could not be saved"), *Path));
    }
}
} // namespace

bool SetOnManyMeshes(UEngineWireSubsystem* Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                     const TArray<TSharedPtr<FJsonValue>>& Paths, const TArray<TSharedPtr<FJsonValue>>& Entries, bool bSave)
{
    TArray<TPair<TSharedPtr<FJsonValue>, UMaterialInterface*>> Wanted;
    for (int32 Position = 0; Position < Entries.Num(); ++Position)
    {
        const TSharedPtr<FJsonObject>* EntryPtr = nullptr;
        FString MaterialText;
        FString Reason = TEXT("an entry must be an object {slot, materialPath}");
        FString Code = TEXT("INVALID_ENTRY");
        UMaterialInterface* Material = nullptr;
        if (Entries[Position].IsValid() && Entries[Position]->TryGetObject(EntryPtr) && EntryPtr && EntryPtr->IsValid())
        {
            (*EntryPtr)->TryGetStringField(TEXT("materialPath"), MaterialText);
            Material = LoadMaterial(MaterialText, Reason, Code);
        }
        if (!Material)
        {
            Bridge->SendAutomationResponse(Socket, RequestId, false,
                                           FString::Printf(TEXT("entry %d: %s; no mesh was changed"), Position, *Reason), nullptr, Code);
            return true;
        }
        Wanted.Emplace((*EntryPtr)->TryGetField(TEXT("slot")), Material);
    }
    TArray<bool> Used;
    Used.Init(false, Wanted.Num());
    TArray<TSharedPtr<FJsonValue>> Rows;
    TArray<FString> Problems;
    int32 TotalChanged = 0;
    for (const TSharedPtr<FJsonValue>& PathValue : Paths)
    {
        SetOnOneOfMany(PathValue.IsValid() ? PathValue->AsString() : FString(), Wanted, bSave, Used, Rows, Problems, TotalChanged);
    }
    for (int32 Entry = 0; Entry < Wanted.Num(); ++Entry)
    {
        if (!Used[Entry]) Problems.Add(FString::Printf(TEXT("entry %d names a slot that none of the meshes has"), Entry));
    }
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetArrayField(TEXT("meshes"), Rows);
    Resp->SetNumberField(TEXT("changed"), TotalChanged);
    const FString Summary = FString::Printf(TEXT("Set %d material slot(s) across %d mesh(es)"), TotalChanged, Rows.Num());
    if (Problems.Num() > 0)
    {
        Bridge->SendAutomationResponse(Socket, RequestId, false, Summary + TEXT("; ") + JoinCapped(Problems), Resp,
                                       TEXT("MATERIAL_SLOTS_PARTIAL"));
        return true;
    }
    Bridge->SendAutomationResponse(Socket, RequestId, true, Summary, Resp);
    return true;
}
} // namespace McpMeshMaterials
