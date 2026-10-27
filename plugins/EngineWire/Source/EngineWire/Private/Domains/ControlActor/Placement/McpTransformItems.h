// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpTransformItems {
/**
 * set_transform's many forms as one payload per actor. actors gives each actor its own fields; actorNames moves every
 * named actor by the call's own location, offset, rotation and scale; an actors item takes from the call any of those
 * it leaves out (its own location or offset replaces the call's either). Scaling forty frost caps was forty items that
 * all said the same scale. False when the call is the single form.
 */
inline bool Expand(const TSharedPtr<FJsonObject> &Payload, TArray<TSharedPtr<FJsonObject>> &Out) {
  const TArray<TSharedPtr<FJsonValue>> *List = nullptr;
  const bool bNames = !Payload->HasField(TEXT("actors")) && Payload->TryGetArrayField(TEXT("actorNames"), List);
  if ((!bNames && !Payload->TryGetArrayField(TEXT("actors"), List)) || !List || List->Num() == 0) {
    return false;
  }
  for (const TSharedPtr<FJsonValue> &Value : *List) {
    TSharedPtr<FJsonObject> One = MakeShared<FJsonObject>();
    const TSharedPtr<FJsonObject> *Item = nullptr;
    FString Name;
    if (bNames && Value.IsValid() && Value->TryGetString(Name)) {
      One->SetStringField(TEXT("actorName"), Name);
    } else if (!bNames && Value.IsValid() && Value->TryGetObject(Item) && Item) {
      One->Values = (*Item)->Values;
    }
    One->RemoveField(TEXT("actors"));
    One->RemoveField(TEXT("actorNames"));
    const bool bOwnPlace = One->HasField(TEXT("location")) || One->HasField(TEXT("offset"));
    for (const TCHAR *Shared : {TEXT("location"), TEXT("offset"), TEXT("rotation"), TEXT("scale")}) {
      const bool bPlace = FCString::Strcmp(Shared, TEXT("location")) == 0 || FCString::Strcmp(Shared, TEXT("offset")) == 0;
      if (!One->HasField(Shared) && !(bPlace && bOwnPlace) && Payload->HasField(Shared)) {
        One->SetField(Shared, Payload->TryGetField(Shared));
      }
    }
    Out.Add(One);
  }
  return true;
}
} // namespace McpTransformItems
