#pragma once

#include "CoreMinimal.h"

#include "Components/SceneComponent.h"
#include "Dom/JsonObject.h"
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsPropagate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

// The placement of one edit_scs operation: {location, rotation, scale} in `transform`, or the
// same three keys directly on the operation. The contract promises both and the single-component
// form only has the second, but only `transform` was ever read, so a `rotation` given on the
// operation was dropped while the operation still answered success. A part left out keeps the
// template's value; a part that cannot be applied is named, never skipped.
namespace McpScsTransform
{
/** Copies location/rotation/scale given directly on Op into its `transform` (a part already there wins). */
inline void FoldDirect(const TSharedPtr<FJsonObject> &Op)
{
  const TSharedPtr<FJsonValue> Nested = Op->TryGetField(TEXT("transform"));
  const TSharedPtr<FJsonObject> *Existing = nullptr;
  // A transform of the wrong shape is left as it is, for Apply to name.
  if (Nested.IsValid() && !Nested->IsNull() && !Nested->TryGetObject(Existing))
    return;
  TSharedPtr<FJsonObject> Transform = MakeShared<FJsonObject>();
  if (Existing)
    Transform = *Existing;
  for (const TCHAR *Key : {TEXT("location"), TEXT("rotation"), TEXT("scale")})
  {
    const TSharedPtr<FJsonValue> Part = Op->TryGetField(Key);
    if (Part.IsValid() && !Part->IsNull() && !Transform->HasField(Key))
      Transform->SetField(Key, Part);
  }
  if (Transform->Values.Num() > 0)
    Op->SetObjectField(TEXT("transform"), Transform);
}

/**
 * Applies Op's `transform` to Template's relative transform. Returns what did not land as
 * "part: reason" (the shape of McpScsPropertyBag::Apply's misses); bAnyApplied is set when a part did.
 */
inline TArray<FString> Apply(UActorComponent *Template, const TSharedPtr<FJsonObject> &Op,
                             McpScsPropagate::FDefaults &Defaults, bool &bAnyApplied)
{
  TArray<FString> Rejected;
  const TSharedPtr<FJsonValue> Value = Op->TryGetField(TEXT("transform"));
  if (!Value.IsValid() || Value->IsNull())
    return Rejected;
  const TSharedPtr<FJsonObject> *Transform = nullptr;
  USceneComponent *Scene = Cast<USceneComponent>(Template);
  if (!Value->TryGetObject(Transform) || !Transform)
    Rejected.Add(TEXT("transform: expected an object holding location, rotation and scale"));
  else if (!Scene)
    Rejected.Add(FString::Printf(TEXT("transform: a %s has no transform"), *Template->GetClass()->GetName()));
  if (Rejected.Num() > 0)
    return Rejected;
  int32 Readable = 0;
  for (const TCHAR *Part : {TEXT("location"), TEXT("rotation"), TEXT("scale")})
  {
    const TSharedPtr<FJsonValue> Field = (*Transform)->TryGetField(Part);
    const TArray<TSharedPtr<FJsonValue>> *Triple = nullptr;
    if (!Field.IsValid() || Field->IsNull())
      continue;
    if ((Field->Type == EJson::Object && Field->AsObject()->Values.Num() > 0) || (Field->TryGetArray(Triple) && Triple->Num() >= 3))
      ++Readable;
    else
      Rejected.Add(FString::Printf(TEXT("%s: expected an object ({x,y,z} or {pitch,yaw,roll}) or an array of three numbers"), Part));
  }
  if (Readable == 0)
    return Rejected;
  for (const TCHAR *Path : {TEXT("RelativeLocation"), TEXT("RelativeRotation"), TEXT("RelativeScale3D")})
    Defaults.Capture(Path);
  const FVector Location = ExtractVectorField(*Transform, TEXT("location"), Scene->GetRelativeLocation());
  const FRotator Rotation = ExtractRotatorField(*Transform, TEXT("rotation"), Scene->GetRelativeRotation());
  const FVector Scale = ExtractVectorField(*Transform, TEXT("scale"), Scene->GetRelativeScale3D());
  Scene->SetRelativeLocation(Location);
  Scene->SetRelativeRotation(Rotation);
  Scene->SetRelativeScale3D(Scale);
  bAnyApplied = true;
  return Rejected;
}
}
