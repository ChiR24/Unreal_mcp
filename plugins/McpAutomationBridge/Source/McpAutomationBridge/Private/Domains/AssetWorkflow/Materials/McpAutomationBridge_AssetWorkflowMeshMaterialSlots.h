// Copyright (c) 2024 MCP Automation Bridge Contributors
//
// The material slots of a static or a skeletal mesh asset behind one interface, and the two lookups set_mesh_materials
// makes for every entry: the slot it names and the material it assigns.

#pragma once

#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace McpMeshMaterials
{
struct FMeshSlots
{
    UStaticMesh* Static = nullptr;
    USkeletalMesh* Skeletal = nullptr;

    int32 Num() const { return Static ? Static->GetStaticMaterials().Num() : Skeletal->GetMaterials().Num(); }

    FName Name(int32 Index) const
    {
        return Static ? Static->GetStaticMaterials()[Index].MaterialSlotName : Skeletal->GetMaterials()[Index].MaterialSlotName;
    }

    FName ImportedName(int32 Index) const
    {
#if WITH_EDITORONLY_DATA
        return Static ? Static->GetStaticMaterials()[Index].ImportedMaterialSlotName
                      : Skeletal->GetMaterials()[Index].ImportedMaterialSlotName;
#else
        return NAME_None;
#endif
    }

    UMaterialInterface* Material(int32 Index) const
    {
        return Static ? Static->GetStaticMaterials()[Index].MaterialInterface : Skeletal->GetMaterials()[Index].MaterialInterface;
    }

    // The slot with this name (the slot name first, then the name it was imported under), or INDEX_NONE.
    int32 Find(const FString& Text) const
    {
        for (int32 Index = 0; Index < Num(); ++Index)
        {
            if (Name(Index).ToString().Equals(Text, ESearchCase::IgnoreCase)) return Index;
        }
        for (int32 Index = 0; Index < Num(); ++Index)
        {
            if (ImportedName(Index).ToString().Equals(Text, ESearchCase::IgnoreCase)) return Index;
        }
        return INDEX_NONE;
    }

    FString Describe() const
    {
        constexpr int32 MaxListed = 24;
        TArray<FString> Parts;
        for (int32 Index = 0; Index < Num() && Index < MaxListed; ++Index)
        {
            Parts.Add(FString::Printf(TEXT("%d '%s'"), Index, *Name(Index).ToString()));
        }
        if (Num() > MaxListed) Parts.Add(FString::Printf(TEXT("%d more"), Num() - MaxListed));
        return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : FString(TEXT("none"));
    }

    // Every slot as {slotIndex, slotName, material, changed}; the shape inspect_object objectKind=mesh lists them in.
    TArray<TSharedPtr<FJsonValue>> Table(const TSet<int32>& Changed) const
    {
        TArray<TSharedPtr<FJsonValue>> Out;
        for (int32 Index = 0; Index < Num(); ++Index)
        {
            const UMaterialInterface* Held = Material(Index);
            TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
            Entry->SetNumberField(TEXT("slotIndex"), Index);
            Entry->SetStringField(TEXT("slotName"), Name(Index).ToString());
            Entry->SetStringField(TEXT("material"), Held ? Held->GetPathName() : FString());
            Entry->SetBoolField(TEXT("changed"), Changed.Contains(Index));
            Out.Add(MakeShared<FJsonValueObject>(Entry));
        }
        return Out;
    }
};

struct FAssignment
{
    int32 Index = INDEX_NONE;
    UMaterialInterface* Material = nullptr;
};

// The slot an entry names: a whole number (also as text), or a slot name. A refusal says why and which code it is.
inline bool ResolveSlot(const FMeshSlots& Slots, const TSharedPtr<FJsonValue>& Value, int32& OutIndex, FString& OutReason,
                        FString& OutCode)
{
    OutCode = TEXT("INVALID_ENTRY");
    if (!Value.IsValid() || Value->IsNull())
    {
        OutReason = TEXT("slot is missing: give a slot index or a slot name");
        return false;
    }
    double Number = 0.0;
    if (Value->Type == EJson::String)
    {
        const FString Text = Value->AsString().TrimStartAndEnd();
        OutIndex = Slots.Find(Text);
        if (OutIndex != INDEX_NONE) return true;
        if (!Text.IsNumeric())
        {
            OutCode = TEXT("SLOT_NOT_FOUND");
            OutReason = FString::Printf(TEXT("no slot is named '%s' (slots: %s)"), *Text, *Slots.Describe());
            return false;
        }
        Number = FCString::Atod(*Text);
    }
    else if (Value->Type == EJson::Number)
    {
        Number = Value->AsNumber();
    }
    else
    {
        OutReason = TEXT("slot must be a slot index (a whole number) or a slot name");
        return false;
    }
    if (Number != FMath::FloorToDouble(Number))
    {
        OutReason = FString::Printf(TEXT("slot %s is not a whole number"), *LexToString(Number));
        return false;
    }
    if (Number < 0.0 || Number >= static_cast<double>(Slots.Num()))
    {
        OutCode = TEXT("SLOT_OUT_OF_RANGE");
        OutReason = FString::Printf(TEXT("slot %s is out of range: the mesh has %d slot(s) (slots: %s)"), *LexToString(Number),
                                    Slots.Num(), *Slots.Describe());
        return false;
    }
    OutIndex = static_cast<int32>(Number);
    return true;
}

// The material at a path, or nullptr with the reason. Never a default material: a path that does not load is refused.
inline UMaterialInterface* LoadMaterial(const FString& Text, FString& OutReason, FString& OutCode)
{
    OutCode = TEXT("INVALID_ENTRY");
    if (Text.IsEmpty())
    {
        OutReason = TEXT("materialPath is missing");
        return nullptr;
    }
    const FString SafePath = SanitizeProjectRelativePath(Text);
    if (SafePath.IsEmpty())
    {
        OutCode = TEXT("SECURITY_VIOLATION");
        OutReason = McpPathRefusalMessage(TEXT("materialPath"), Text);
        return nullptr;
    }
    UObject* Loaded = McpLoadAsset(SafePath);
    UMaterialInterface* Material = Cast<UMaterialInterface>(Loaded);
    if (!Material)
    {
        OutCode = TEXT("MATERIAL_NOT_FOUND");
        OutReason = Loaded ? FString::Printf(TEXT("%s is a %s, not a material or a material instance"), *Text, *Loaded->GetClass()->GetName())
                           : FString::Printf(TEXT("no material was found at %s"), *Text);
    }
    return Material;
}
}
