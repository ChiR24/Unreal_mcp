#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceIntegerValidation.h"

namespace McpSequenceIntegerValidation {
namespace {
bool ValidateObject(const TSharedPtr<FJsonObject> &Object,
                    const TArray<FString> &FieldNames,
                    const FString &Prefix, FString &OutError) {
  if (!Object.IsValid()) {
    return true;
  }
  for (const FString &FieldName : FieldNames) {
    if (!Object->HasField(FieldName)) {
      continue;
    }
    double Value = 0.0;
    if (!Object->TryGetNumberField(FieldName, Value) ||
        !FMath::IsFinite(Value) || FMath::TruncToDouble(Value) != Value ||
        Value < static_cast<double>(MIN_int32) ||
        Value > static_cast<double>(MAX_int32)) {
      OutError = FString::Printf(
          TEXT("%s%s must be an integer between %d and %d."),
          *Prefix, *FieldName, MIN_int32, MAX_int32);
      return false;
    }
  }
  return true;
}
}

bool ValidateSequenceIntegerFields(
    const TSharedPtr<FJsonObject> &Payload, FString &OutError) {
  static const TArray<FString> RootFields = {
      TEXT("frame"),
      TEXT("width"),
      TEXT("height"),
      TEXT("startFrame"),
      TEXT("endFrame"),
      TEXT("lengthInFrames"),
      TEXT("playbackStart"),
      TEXT("playbackEnd"),
      TEXT("temporalSampleCount"),
      TEXT("spatialSampleCount"),
      TEXT("durationFrames"),
      TEXT("rowIndex"),
      TEXT("sectionIndex"),
      TEXT("materialIndex"),
      TEXT("playlistIndex"),
      TEXT("index")};
  static const TArray<FString> SettingsFields = {
      TEXT("handleFrameCount"),
      TEXT("zeroPadFrameNumbers"),
      TEXT("spatialSampleCount"),
      TEXT("temporalSampleCount")};
  if (!ValidateObject(Payload, RootFields, FString(), OutError)) {
    return false;
  }
  const TSharedPtr<FJsonObject> *Settings = nullptr;
  if (Payload.IsValid() &&
      Payload->TryGetObjectField(TEXT("settings"), Settings) &&
      Settings && !ValidateObject(
          *Settings, SettingsFields, TEXT("settings."), OutError)) {
    return false;
  }
  return true;
}

}
