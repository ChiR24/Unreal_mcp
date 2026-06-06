#pragma once

/**
 * Populate Out with the vector found at the given JSON field, or use Default if
 * the field is missing or invalid.
 *
 * @param Obj JSON object to read the field from; may be null.
 * @param FieldName Name of the field containing the vector (object with x/y/z
 * or an array of three numbers).
 * @param Out Receives the resulting FVector.
 * @param Default Fallback FVector used when the field is absent or cannot be
 * parsed.
 */
static inline void ReadVectorField(const TSharedPtr<FJsonObject> &Obj,
                                   const TCHAR *FieldName, FVector &Out,
                                   const FVector &Default) {
  if (!Obj.IsValid()) {
    Out = Default;
    return;
  }
  const TSharedPtr<FJsonObject> *FieldObj = nullptr;
  if (Obj->TryGetObjectField(FieldName, FieldObj) && FieldObj &&
      (*FieldObj).IsValid()) {
    double X = Default.X, Y = Default.Y, Z = Default.Z;
    if (!(*FieldObj)->TryGetNumberField(TEXT("x"), X))
      (*FieldObj)->TryGetNumberField(TEXT("X"), X);
    if (!(*FieldObj)->TryGetNumberField(TEXT("y"), Y))
      (*FieldObj)->TryGetNumberField(TEXT("Y"), Y);
    if (!(*FieldObj)->TryGetNumberField(TEXT("z"), Z))
      (*FieldObj)->TryGetNumberField(TEXT("Z"), Z);
    Out = FVector((float)X, (float)Y, (float)Z);
    return;
  }
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  if (Obj->TryGetArrayField(FieldName, Arr) && Arr && Arr->Num() >= 3) {
    Out = FVector((float)(*Arr)[0]->AsNumber(), (float)(*Arr)[1]->AsNumber(),
                  (float)(*Arr)[2]->AsNumber());
    return;
  }
  Out = Default;
}

/**
 * Read a rotator field from a JSON object into an FRotator.
 *
 * Attempts to read a rotator located at FieldName in Obj. Supports either an
 * object form with numeric fields "pitch"/"yaw"/"roll" (case-insensitive) or an
 * array form [pitch, yaw, roll]. If the field is missing or invalid, Out is
 * set to Default.
 *
 * @param Obj JSON object to read from.
 * @param FieldName Name of the field within Obj containing the rotator.
 * @param Out Output rotator populated from the JSON field or Default on
 * failure.
 * @param Default Fallback rotator used when the JSON field is absent or
 * invalid.
 */
static inline void ReadRotatorField(const TSharedPtr<FJsonObject> &Obj,
                                    const TCHAR *FieldName, FRotator &Out,
                                    const FRotator &Default) {
  if (!Obj.IsValid()) {
    Out = Default;
    return;
  }
  const TSharedPtr<FJsonObject> *FieldObj = nullptr;
  if (Obj->TryGetObjectField(FieldName, FieldObj) && FieldObj &&
      (*FieldObj).IsValid()) {
    double Pitch = Default.Pitch, Yaw = Default.Yaw, Roll = Default.Roll;
    if (!(*FieldObj)->TryGetNumberField(TEXT("pitch"), Pitch))
      (*FieldObj)->TryGetNumberField(TEXT("Pitch"), Pitch);
    if (!(*FieldObj)->TryGetNumberField(TEXT("yaw"), Yaw))
      (*FieldObj)->TryGetNumberField(TEXT("Yaw"), Yaw);
    if (!(*FieldObj)->TryGetNumberField(TEXT("roll"), Roll))
      (*FieldObj)->TryGetNumberField(TEXT("Roll"), Roll);
    Out = FRotator((float)Pitch, (float)Yaw, (float)Roll);
    return;
  }
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  if (Obj->TryGetArrayField(FieldName, Arr) && Arr && Arr->Num() >= 3) {
    Out = FRotator((float)(*Arr)[0]->AsNumber(), (float)(*Arr)[1]->AsNumber(),
                   (float)(*Arr)[2]->AsNumber());
    return;
  }
  Out = Default;
}

/**
 * Extracts a FVector from a JSON object field, returning a default when the
 * field is absent or invalid.
 * @param Source JSON object to read from.
 * @param FieldName Name of the field to extract (expects an object with x/y/z
 * or an array).
 * @param DefaultValue Value to return when the field is missing or cannot be
 * parsed.
 * @returns The parsed FVector from the specified field, or DefaultValue if
 * parsing failed.
 */
static inline FVector ExtractVectorField(const TSharedPtr<FJsonObject> &Source,
                                         const TCHAR *FieldName,
                                         const FVector &DefaultValue) {
  FVector Parsed = DefaultValue;
  ReadVectorField(Source, FieldName, Parsed, DefaultValue);
  return Parsed;
}

/**
 * Extracts a rotator value from a JSON object field, returning the provided
 * default when the field is absent or cannot be parsed.
 * @param Source JSON object to read the field from.
 * @param FieldName Name of the field to extract.
 * @param DefaultValue Value returned when the field is missing or invalid.
 * @returns Parsed FRotator from the specified field, or DefaultValue if
 * extraction fails.
 */
static inline FRotator
ExtractRotatorField(const TSharedPtr<FJsonObject> &Source,
                    const TCHAR *FieldName, const FRotator &DefaultValue) {
  FRotator Parsed = DefaultValue;
  ReadRotatorField(Source, FieldName, Parsed, DefaultValue);
  return Parsed;
}

// ============================================================================
// CONSOLIDATED JSON FIELD ACCESSORS
// ============================================================================
// These helpers safely extract values from JSON objects with defaults.
// Use these instead of duplicating helpers in each handler file.
// ============================================================================

/**
 * Safely get a string field from a JSON object with a default value.
 * @param Obj JSON object to read from (may be null/invalid).
 * @param Field Name of the string field.
 * @param Default Value to return if field is missing or Obj is invalid.
 * @returns The string value or Default.
 */
static inline FString GetJsonStringField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, const FString& Default = TEXT(""))
{
    FString Value;
    if (Obj.IsValid() && Obj->TryGetStringField(Field, Value))
    {
        return Value;
    }
    return Default;
}

/**
 * Safely get a number field from a JSON object with a default value.
 * @param Obj JSON object to read from (may be null/invalid).
 * @param Field Name of the number field.
 * @param Default Value to return if field is missing or Obj is invalid.
 * @returns The number value or Default.
 */
static inline double GetJsonNumberField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, double Default = 0.0)
{
    double Value = Default;
    if (Obj.IsValid())
    {
        Obj->TryGetNumberField(Field, Value);
    }
    return Value;
}

/**
 * Safely get a boolean field from a JSON object with a default value.
 * @param Obj JSON object to read from (may be null/invalid).
 * @param Field Name of the boolean field.
 * @param Default Value to return if field is missing or Obj is invalid.
 * @returns The boolean value or Default.
 */
static inline bool GetJsonBoolField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, bool Default = false)
{
    bool Value = Default;
    if (Obj.IsValid())
    {
        Obj->TryGetBoolField(Field, Value);
    }
    return Value;
}

/**
 * Safely get an integer field from a JSON object with a default value.
 * @param Obj JSON object to read from (may be null/invalid).
 * @param Field Name of the number field to read as int32.
 * @param Default Value to return if field is missing or Obj is invalid.
 * @returns The integer value or Default.
 */
static inline int32 GetJsonIntField(const TSharedPtr<FJsonObject>& Obj, const FString& Field, int32 Default = 0)
{
    double Value = static_cast<double>(Default);
    if (Obj.IsValid())
    {
        Obj->TryGetNumberField(Field, Value);
    }
    return static_cast<int32>(Value);
}
