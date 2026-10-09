#pragma once

// A vector-like JSON value: an object carrying the three Keys (FJsonObject field
// lookup ignores case, so "X" matches "x"; a missing key keeps InOut's value) or
// an array of at least three numbers. False, InOut untouched, for anything else.
static inline bool ReadJsonTriple(const TSharedPtr<FJsonValue> &Value,
                                  const TCHAR *const (&Keys)[3], double (&InOut)[3]) {
  const TSharedPtr<FJsonObject> *Obj = nullptr;
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  if (Value.IsValid() && Value->TryGetObject(Obj) && Obj && (*Obj).IsValid()) {
    for (int32 Index = 0; Index < 3; ++Index)
      (*Obj)->TryGetNumberField(Keys[Index], InOut[Index]);
    return true;
  }
  if (Value.IsValid() && Value->TryGetArray(Arr) && Arr && Arr->Num() >= 3) {
    for (int32 Index = 0; Index < 3; ++Index)
      InOut[Index] = (*Arr)[Index]->AsNumber();
    return true;
  }
  return false;
}

// {x,y,z} or [x,y,z]; Default when absent or malformed.
static inline FVector ReadJsonVector(const TSharedPtr<FJsonValue> &Value, const FVector &Default) {
  static const TCHAR *const Keys[3] = {TEXT("x"), TEXT("y"), TEXT("z")};
  double V[3] = {Default.X, Default.Y, Default.Z};
  return ReadJsonTriple(Value, Keys, V) ? FVector(V[0], V[1], V[2]) : Default;
}

// {pitch,yaw,roll} or [pitch,yaw,roll]; Default when absent or malformed.
static inline FRotator ReadJsonRotator(const TSharedPtr<FJsonValue> &Value, const FRotator &Default) {
  static const TCHAR *const Keys[3] = {TEXT("pitch"), TEXT("yaw"), TEXT("roll")};
  double V[3] = {Default.Pitch, Default.Yaw, Default.Roll};
  return ReadJsonTriple(Value, Keys, V) ? FRotator(V[0], V[1], V[2]) : Default;
}

static inline FVector ExtractVectorField(const TSharedPtr<FJsonObject> &Source,
                                         const TCHAR *FieldName,
                                         const FVector &DefaultValue) {
  return ReadJsonVector(Source.IsValid() ? Source->TryGetField(FieldName) : nullptr, DefaultValue);
}

static inline FRotator ExtractRotatorField(const TSharedPtr<FJsonObject> &Source,
                                           const TCHAR *FieldName,
                                           const FRotator &DefaultValue) {
  return ReadJsonRotator(Source.IsValid() ? Source->TryGetField(FieldName) : nullptr, DefaultValue);
}

// {r,g,b,a} (or {x,y,z,w}) object, a missing channel keeps Default's, or an
// [r,g,b(,a)] array; Default when the field is absent or malformed.
static inline FLinearColor ExtractLinearColorField(const TSharedPtr<FJsonObject> &Source,
                                                   const TCHAR *FieldName,
                                                   const FLinearColor &Default) {
  if (!Source.IsValid()) {
    return Default;
  }
  const TSharedPtr<FJsonObject> *Obj = nullptr;
  if (Source->TryGetObjectField(FieldName, Obj) && Obj && (*Obj).IsValid()) {
    double R = Default.R, G = Default.G, B = Default.B, A = Default.A;
    if (!(*Obj)->TryGetNumberField(TEXT("r"), R)) (*Obj)->TryGetNumberField(TEXT("x"), R);
    if (!(*Obj)->TryGetNumberField(TEXT("g"), G)) (*Obj)->TryGetNumberField(TEXT("y"), G);
    if (!(*Obj)->TryGetNumberField(TEXT("b"), B)) (*Obj)->TryGetNumberField(TEXT("z"), B);
    if (!(*Obj)->TryGetNumberField(TEXT("a"), A)) (*Obj)->TryGetNumberField(TEXT("w"), A);
    return FLinearColor(static_cast<float>(R), static_cast<float>(G), static_cast<float>(B), static_cast<float>(A));
  }
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  if (Source->TryGetArrayField(FieldName, Arr) && Arr && Arr->Num() >= 3) {
    return FLinearColor(static_cast<float>((*Arr)[0]->AsNumber()), static_cast<float>((*Arr)[1]->AsNumber()),
                        static_cast<float>((*Arr)[2]->AsNumber()),
                        Arr->Num() > 3 ? static_cast<float>((*Arr)[3]->AsNumber()) : Default.A);
  }
  return Default;
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

// The string entries of the ListField array, else the non-empty SingleField string, else nothing.
static inline TArray<FString> McpGetStringListField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* ListField, const TCHAR* SingleField)
{
    TArray<FString> Out;
    const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
    if (Obj.IsValid() && Obj->TryGetArrayField(ListField, List) && List && List->Num() > 0)
    {
        for (const TSharedPtr<FJsonValue>& Value : *List)
        {
            if (Value.IsValid() && Value->Type == EJson::String)
            {
                Out.Add(Value->AsString());
            }
        }
    }
    else
    {
        FString Single;
        if (Obj.IsValid() && Obj->TryGetStringField(SingleField, Single) && !Single.IsEmpty())
        {
            Out.Add(Single);
        }
    }
    return Out;
}

// A scalar as tag text: a string as-is, a bool as "true"/"false", a number via %g; false for null, arrays and objects.
// A JSON number as a literal: whole numbers without a fraction ("150", which int pins and properties need), others
// through SanitizeFloat.
static inline FString McpJsonNumberToString(double Number)
{
    const double Rounded = FMath::RoundToDouble(Number);
    return FMath::IsNearlyEqual(Number, Rounded) && FMath::Abs(Number) < 1.0e15
        ? FString::Printf(TEXT("%lld"), static_cast<long long>(Rounded))
        : FString::SanitizeFloat(Number);
}

static inline bool McpJsonScalarToString(const TSharedPtr<FJsonValue>& Value, FString& Out)
{
    if (!Value.IsValid())
    {
        return false;
    }
    switch (Value->Type)
    {
    case EJson::String: Out = Value->AsString(); return true;
    case EJson::Boolean: Out = Value->AsBool() ? TEXT("true") : TEXT("false"); return true;
    case EJson::Number: Out = McpJsonNumberToString(Value->AsNumber()); return true;
    default: return false;
    }
}

// The first of Fields holding a non-empty string (canonical name first, then its aliases); empty when none does.
static inline FString McpGetFirstStringField(const TSharedPtr<FJsonObject>& Obj, std::initializer_list<const TCHAR*> Fields)
{
    FString Value;
    for (const TCHAR* Field : Fields)
    {
        if (Obj.IsValid() && Obj->TryGetStringField(Field, Value) && !Value.IsEmpty())
        {
            return Value;
        }
    }
    return FString();
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

// A batch reply listed every step, so a 108-step graph build answered some 70 rows of
// {"index":N,"edit":"connect_pins","connected":true,"success":true}. A step that ran and says nothing beyond
// those fields is left to the batch's succeeded count; created nodes, pin reports, replaced links, warnings and
// failures stay listed.
static inline TArray<TSharedPtr<FJsonValue>> McpListTellingSteps(const TArray<TSharedPtr<FJsonValue>>& Results)
{
    TArray<TSharedPtr<FJsonValue>> Telling;
    for (const TSharedPtr<FJsonValue>& Value : Results)
    {
        const TSharedPtr<FJsonObject> Step = Value.IsValid() ? Value->AsObject() : nullptr;
        int32 Plain = 0;
        for (const TCHAR* Field : {TEXT("index"), TEXT("edit"), TEXT("success"), TEXT("connected"), TEXT("id")})
        {
            Plain += Step.IsValid() && Step->HasField(Field) ? 1 : 0;
        }
        bool bRan = false;
        bool bConnected = true;
        const bool bQuiet = Step.IsValid() && Step->TryGetBoolField(TEXT("success"), bRan) && bRan &&
                            (!Step->TryGetBoolField(TEXT("connected"), bConnected) || bConnected) &&
                            Step->Values.Num() == Plain;
        if (!bQuiet)
        {
            Telling.Add(Value);
        }
    }
    return Telling;
}
