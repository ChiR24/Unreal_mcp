// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Placement/McpSpawnRepeat.h"

#include "Components/SplineComponent.h"
#include "Dom/JsonValue.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "GameFramework/Actor.h"

namespace McpSpawnRepeat {
namespace {
/** repeat.alongSpline: the named actor's spline and where along it the copies stand (OutStart, OutStep, OutCopies). */
const USplineComponent *FindSpline(const TSharedPtr<FJsonObject> &Repeat, TFunctionRef<AActor *(const FString &)> Find,
                                   double &OutStart, double &OutStep, int32 &OutCopies, FString &Error) {
  const FString Name = GetJsonStringField(Repeat, TEXT("alongSpline"));
  const AActor *Actor = Name.IsEmpty() ? nullptr : Find(Name);
  const USplineComponent *Spline = Actor ? Actor->FindComponentByClass<USplineComponent>() : nullptr;
  if (!Spline) {
    Error = FString::Printf(TEXT("repeat.alongSpline: %s %s."), *Name, Actor ? TEXT("has no spline") : TEXT("is no actor in the level"));
    return nullptr;
  }
  const double Length = Spline->GetSplineLength();
  const double Count = GetJsonNumberField(Repeat, TEXT("count"), 0.0);
  const double Spacing = GetJsonNumberField(Repeat, TEXT("spacingCm"), 0.0);
  if ((Count >= 1.0) == (Spacing > 0.0)) {
    Error = TEXT("repeat.alongSpline takes count (copies from startCm to the end) or spacingCm (one every so many cm), one of them.");
    return nullptr;
  }
  OutStart = FMath::Clamp(GetJsonNumberField(Repeat, TEXT("startCm"), 0.0), 0.0, Length);
  const double Copies = Count >= 1.0 ? FMath::FloorToDouble(Count) : FMath::FloorToDouble((Length - OutStart) / Spacing) + 1.0;
  OutCopies = static_cast<int32>(FMath::Min(Copies, 1.0e6));
  OutStep = Count >= 1.0 ? (OutCopies > 1 ? (Length - OutStart) / (OutCopies - 1) : 0.0) : Spacing;
  return Spline;
}

/** The copies along a spline: each on it, sideOffsetCm to its right (negative: left) and heightCm up, turned with it. */
bool ExpandAlongSpline(const TSharedPtr<FJsonObject> &Item, const TSharedPtr<FJsonObject> &Repeat, int32 MaxActors,
                       TArray<TSharedPtr<FJsonObject>> &Out, FString &Error, TFunctionRef<AActor *(const FString &)> Find) {
  double Start = 0.0, Step = 0.0;
  int32 Copies = 0;
  const USplineComponent *Spline = FindSpline(Repeat, Find, Start, Step, Copies, Error);
  if (!Spline) {
    return false;
  }
  if (Out.Num() + double(Copies) > MaxActors) {
    Error = FString::Printf(TEXT("repeat would make %d actors; a batch places at most %d once repeats are unrolled"), Copies, MaxActors);
    return false;
  }
  const double Side = GetJsonNumberField(Repeat, TEXT("sideOffsetCm"), 0.0);
  const double Height = GetJsonNumberField(Repeat, TEXT("heightCm"), 0.0);
  const bool bAlign = GetJsonBoolField(Repeat, TEXT("alignToSpline"), true);
  // The item's own rotation turns each copy from the spline's direction (or from the world's, alignToSpline false).
  const FQuat Turn = ExtractRotatorField(Item, TEXT("rotation"), FRotator::ZeroRotator).Quaternion();
  FString Name;
  const bool bNamed = Item->TryGetStringField(TEXT("actorName"), Name) && !Name.IsEmpty();
  for (int32 Index = 0; Index < Copies; ++Index) {
    const double Distance = Start + Step * Index;
    const FVector Point = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World)
        + Spline->GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) * Side + FVector(0.0, 0.0, Height);
    const FQuat Along = bAlign ? Spline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World).Quaternion() : FQuat::Identity;
    TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
    Copy->Values = Item->Values;
    Copy->RemoveField(TEXT("repeat"));
    Copy->SetArrayField(TEXT("location"), McpHandlerUtils::VectorToJsonArray(Point));
    Copy->SetArrayField(TEXT("rotation"), McpHandlerUtils::RotatorToJsonArray((Along * Turn).Rotator()));
    if (bNamed) {
      Copy->SetStringField(TEXT("actorName"), FString::Printf(TEXT("%s_%d"), *Name, Index + 1));
    }
    Out.Add(Copy);
  }
  return true;
}
} // namespace

bool Expand(const TSharedPtr<FJsonObject> &Item, int32 MaxActors, TArray<TSharedPtr<FJsonObject>> &Out,
            FString &Error, TFunctionRef<AActor *(const FString &)> Find) {
  const TSharedPtr<FJsonObject> *RepeatPtr = nullptr;
  if (!Item->TryGetObjectField(TEXT("repeat"), RepeatPtr) || !RepeatPtr || !(*RepeatPtr).IsValid()) {
    if (Item->HasField(TEXT("repeat"))) {
      Error = TEXT("repeat must be an object: {count, offset [x, y, z], rows, rowOffset [x, y, z]} or {alongSpline, count or spacingCm, ...}");
      return false;
    }
    Out.Add(Item);
  } else if ((*RepeatPtr)->HasField(TEXT("alongSpline"))) {
    if (!ExpandAlongSpline(Item, *RepeatPtr, MaxActors, Out, Error, Find)) {
      return false;
    }
  } else {
    const TSharedPtr<FJsonObject> &Repeat = *RepeatPtr;
    double Count = 0.0, Rows = 1.0;
    Repeat->TryGetNumberField(TEXT("count"), Count);
    Repeat->TryGetNumberField(TEXT("rows"), Rows);
    if (Count < 1.0 || Rows < 1.0) {
      Error = TEXT("repeat needs count of 1 or more (and rows, when given, of 1 or more)");
      return false;
    }
    const int32 Columns = FMath::FloorToInt(Count), RowCount = FMath::FloorToInt(Rows);
    if (Out.Num() + double(Columns) * RowCount > MaxActors) {
      Error = FString::Printf(TEXT("repeat would make %.0f actors; a batch places at most %d once repeats are unrolled"),
                              double(Columns) * RowCount, MaxActors);
      return false;
    }
    const FVector Base = ExtractVectorField(Item, TEXT("location"), FVector::ZeroVector);
    const FVector Offset = ExtractVectorField(Repeat, TEXT("offset"), FVector::ZeroVector);
    const FVector RowOffset = ExtractVectorField(Repeat, TEXT("rowOffset"), FVector::ZeroVector);
    FString Name;
    const bool bNamed = Item->TryGetStringField(TEXT("actorName"), Name) && !Name.IsEmpty();
    int32 Number = 0;
    for (int32 Row = 0; Row < RowCount; ++Row) {
      for (int32 Column = 0; Column < Columns; ++Column) {
        TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>();
        Copy->Values = Item->Values;
        Copy->RemoveField(TEXT("repeat"));
        Copy->SetArrayField(TEXT("location"),
                            McpHandlerUtils::VectorToJsonArray(Base + Offset * Column + RowOffset * Row));
        if (bNamed) {
          Copy->SetStringField(TEXT("actorName"), FString::Printf(TEXT("%s_%d"), *Name, ++Number));
        }
        Out.Add(Copy);
      }
    }
  }
  if (Out.Num() > MaxActors) {
    Error = FString::Printf(TEXT("a batch places at most %d actors once repeats are unrolled"), MaxActors);
    return false;
  }
  return true;
}
} // namespace McpSpawnRepeat
