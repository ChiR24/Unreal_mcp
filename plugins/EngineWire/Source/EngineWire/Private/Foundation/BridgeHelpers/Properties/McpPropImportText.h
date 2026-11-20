#pragma once

#include "CoreMinimal.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Dom/JsonValue.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// Writing a property from UE export text without a half-written value on refusal, and a struct that holds one list
// from a JSON list (McpPropApplyObjects.h converts JSON values through these).

/**
 * Fill a scratch value of Property and copy it over ValuePtr only when Fill returns true, so a refused write leaves
 * the property as it was. The scratch honours the property's REQUIRED alignment: TArray<uint8> only guarantees the
 * default allocator alignment, and FProperty::GetMinAlignment() exists because that is not always enough.
 * FTransform/FQuat/FVector4/FMatrix are alignas(16), and InitializeValue/ImportText on an under-aligned buffer for an
 * SSE type is undefined behaviour that UE's binned allocator would only surface as a rare crash.
 */
template <typename FillFn>
static inline bool McpWriteThroughScratch(void *ValuePtr, FProperty *Property, FillFn &&Fill) {
  void *Scratch = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
  if (!Scratch) {
    return false;
  }
  Property->InitializeValue(Scratch);
  ON_SCOPE_EXIT {
    Property->DestroyValue(Scratch);
    FMemory::Free(Scratch);
  };
  if (!Fill(Scratch)) {
    return false;
  }
  Property->CopyCompleteValue(ValuePtr, Scratch);
  return true;
}

/**
 * Import a UE export-text value into an already-resolved property value slot.
 *
 * Export text is the `(Key=Value,...)` form that ExportTextItem produces --
 * and it is exactly what `get_property` hands back to callers. Without an
 * import path for it, a caller could not feed our own output back into
 * `set_property`: reading `LayoutData` returned
 * `(Offsets=(Left=-48.000000,...))`, and passing that same string back failed
 * with PROPERTY_CONVERSION_FAILED. Accepting it closes that read/write
 * asymmetry.
 *
 * @param ValuePtr Pointer to the property's value (NOT the container).
 */
static inline bool ImportExportTextIntoValue(void *ValuePtr, FProperty *Property,
                                             const FString &Text) {
  if (!ValuePtr || !Property || Text.IsEmpty()) {
    return false;
  }

  // Parse into SCRATCH, not into the live property. ImportText writes struct members as it parses, so on
  // malformed input it returns nullptr having ALREADY written whatever it managed to read:
  // "(Offsets=(Left=10,Top=NOTANUMBER))" would leave Left=10 on the real object while the caller is told the
  // conversion failed.
  return McpWriteThroughScratch(ValuePtr, Property, [Property, &Text](void *Scratch) {
    // The error device stays GWarn: a null one crashed the editor on text an enum or struct import refused.
    return MCP_PROPERTY_IMPORT_TEXT(Property, *Text, Scratch, nullptr, PPF_None) != nullptr;
  });
}

// A struct that holds one list (a gameplay tag container's GameplayTags) takes a JSON list, or one string, as that
// list, through the struct's own text import: callers send ["Ability.Fire"] where the export text is
// (GameplayTags=((TagName="Ability.Fire"))). 0 when the struct or value is not that shape, 1 when set, -1 when the
// struct kept fewer values than were given (OutError says how many).
static inline int32 ImportStructAsItsOnlyList(FStructProperty *SP, void *Value, const TSharedPtr<FJsonValue> &Field,
                                              FString &OutError) {
  FArrayProperty *Only = nullptr;
  int32 Lists = 0;
  for (TFieldIterator<FProperty> It(SP->Struct); It; ++It) {
    if (FArrayProperty *List = CastField<FArrayProperty>(*It); List && !List->HasAnyPropertyFlags(CPF_Transient)) {
      Only = List;
      ++Lists;
    }
  }
  const TArray<TSharedPtr<FJsonValue>> Items =
      Field->Type == EJson::Array ? Field->AsArray() : TArray<TSharedPtr<FJsonValue>>{Field};
  TArray<FString> Texts;
  for (const TSharedPtr<FJsonValue> &Item : Items) {
    if (Item.IsValid() && Item->Type == EJson::String) {
      Texts.Add(TEXT("\"") + Item->AsString().ReplaceCharWithEscapedChar() + TEXT("\""));
    } else if (Item.IsValid() && Item->Type == EJson::Number) {
      Texts.Add(Item->AsString());
    }
  }
  if (Lists != 1 || Texts.Num() != Items.Num()) {
    return 0;
  }
  // An element its type refuses fails the whole import (an unregistered gameplay tag does), so that counts as none kept.
  const bool bTaken =
      ImportExportTextIntoValue(Value, SP, FString::Printf(TEXT("(%s=(%s))"), *Only->GetName(), *FString::Join(Texts, TEXT(","))));
  const int32 Kept = bTaken ? FScriptArrayHelper(Only, Only->ContainerPtrToValuePtr<void>(Value)).Num() : 0;
  if (bTaken && Kept == Items.Num()) {
    return 1;
  }
  OutError = FString::Printf(TEXT("%s took %d of the %d values given for its %s; the others are not valid there (a gameplay "
                                  "tag must be registered in the project's tag list first). %s"),
                             *SP->Struct->GetName(), Kept, Items.Num(), *Only->GetName(),
                             bTaken ? TEXT("It now holds only those.") : TEXT("Nothing was set."));
  return -1;
}
