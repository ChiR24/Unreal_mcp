// Copyright (c) 2024 EngineWire Contributors

#include "Domains/ControlActor/Placement/McpSpawnRepeat.h"

#include "Dom/JsonValue.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

namespace McpSpawnRepeat {
bool Expand(const TSharedPtr<FJsonObject> &Item, int32 MaxActors, TArray<TSharedPtr<FJsonObject>> &Out,
            FString &Error) {
  const TSharedPtr<FJsonObject> *RepeatPtr = nullptr;
  if (!Item->TryGetObjectField(TEXT("repeat"), RepeatPtr) || !RepeatPtr || !(*RepeatPtr).IsValid()) {
    if (Item->HasField(TEXT("repeat"))) {
      Error = TEXT("repeat must be an object: {count, offset [x, y, z], rows, rowOffset [x, y, z]}");
      return false;
    }
    Out.Add(Item);
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
