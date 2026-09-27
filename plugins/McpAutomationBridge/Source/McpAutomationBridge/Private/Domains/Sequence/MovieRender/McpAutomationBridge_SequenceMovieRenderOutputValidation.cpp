#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE

#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceFrameRate.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderResourceLimits.h"

#include "Misc/FrameRate.h"
#include "Misc/Paths.h"
#include "McpAutomationBridgeSettings.h"
#include "String/LexFromString.h"

namespace McpSequenceMovieRender {
namespace {
bool AreFileNameTokensSafe(const FString &Format) {
  static const TSet<FString> Allowed = {
      TEXT("camera_name"), TEXT("date"), TEXT("day"), TEXT("file_dup"),
      TEXT("frame_number"), TEXT("frame_number_rel"),
      TEXT("frame_number_shot"), TEXT("frame_number_shot_rel"),
      TEXT("frame_rate"), TEXT("job_name"), TEXT("layer_name"),
      TEXT("level_name"), TEXT("month"), TEXT("output_height"),
      TEXT("output_resolution"), TEXT("output_width"),
      TEXT("overscan_percentage"), TEXT("renderer_name"),
      TEXT("renderer_sub_name"), TEXT("sampling_mode"),
      TEXT("sequence_name"), TEXT("shot_name"), TEXT("shutter_timing"),
      TEXT("ss_count"), TEXT("time"), TEXT("ts_count"), TEXT("version"),
      TEXT("year")};
  int32 Cursor = 0;
  while (Cursor < Format.Len()) {
    const int32 Open = Format.Find(TEXT("{"), ESearchCase::CaseSensitive,
                                   ESearchDir::FromStart, Cursor);
    if (Open == INDEX_NONE)
      return !Format.Mid(Cursor).Contains(TEXT("}"));
    const int32 Close = Format.Find(TEXT("}"), ESearchCase::CaseSensitive,
                                    ESearchDir::FromStart, Open + 1);
    if (Close == INDEX_NONE || Format.Mid(Cursor, Open - Cursor).Contains(TEXT("}")))
      return false;
    if (!Allowed.Contains(Format.Mid(Open + 1, Close - Open - 1).ToLower()))
      return false;
    Cursor = Close + 1;
  }
  return true;
}
}

bool ValidateFileNameFormat(const FString &FileNameFormat,
                            FString &OutValidationError) {
  if (FileNameFormat.IsEmpty() || !FPaths::IsRelative(FileNameFormat) ||
      FileNameFormat.Contains(TEXT("/")) ||
      FileNameFormat.Contains(TEXT("\\")) ||
      FileNameFormat.Contains(TEXT("..")) ||
      FileNameFormat.Contains(TEXT(":")) ||
      !AreFileNameTokensSafe(FileNameFormat)) {
    OutValidationError =
        TEXT("fileNameFormat must use safe filename tokens without paths or traversal.");
    return false;
  }
  for (const TCHAR Character : FileNameFormat) {
    if (Character < 32) {
      OutValidationError =
          TEXT("fileNameFormat must not contain control characters.");
      return false;
    }
  }
  return true;
}

bool ValidateRenderJobName(const FString &JobName,
                           FString &OutValidationError) {
  const FString Trimmed = JobName.TrimStartAndEnd();
  if (Trimmed.IsEmpty() || Trimmed.Len() > 128 ||
      Trimmed.Contains(TEXT("..")) || Trimmed.Contains(TEXT("/")) ||
      Trimmed.Contains(TEXT("\\")) || Trimmed.Contains(TEXT(":")) ||
      Trimmed.Contains(TEXT("{")) || Trimmed.Contains(TEXT("}"))) {
    OutValidationError =
        TEXT("renderJobName must be 1-128 characters without paths, traversal, or format tokens.");
    return false;
  }
  for (const TCHAR Character : Trimmed) {
    if (Character < 32) {
      OutValidationError =
          TEXT("renderJobName must not contain control characters.");
      return false;
    }
  }
  return true;
}

bool ValidateRenderOutputDirectory(const FString &OutputDirectory,
                                   FString &OutResolvedDirectory,
                                   FString &OutValidationError) {
  FString Expanded = OutputDirectory;
  Expanded.ReplaceInline(TEXT("{project_dir}"), *FPaths::ProjectDir(),
                         ESearchCase::IgnoreCase);
  if (Expanded.Contains(TEXT("{")) || Expanded.Contains(TEXT("}"))) {
    OutValidationError =
        TEXT("outputDirectory contains an unsupported format token.");
    return false;
  }
  return McpSequencePathSecurity::ValidateLocalPath(
      Expanded, McpSequencePathSecurity::ELocalPathUse::RenderOutput,
      OutResolvedDirectory, OutValidationError);
}

bool ValidateOutputSettingsPayload(const TSharedPtr<FJsonObject> &Payload,
                                   FString &OutMessage, FString &OutCode) {
  if (!Payload.IsValid())
    return true;

  FString TextValue;
  if (Payload->TryGetStringField(TEXT("outputDirectory"), TextValue) &&
      !TextValue.IsEmpty()) {
    FString ResolvedDirectory;
    if (!ValidateRenderOutputDirectory(TextValue, ResolvedDirectory,
                                       OutMessage)) {
      OutCode = TEXT("MRQ_OUTPUT_PATH_NOT_ALLOWED");
      return false;
    }
  }
  if (Payload->TryGetStringField(TEXT("fileNameFormat"), TextValue) &&
      !TextValue.IsEmpty() &&
      !ValidateFileNameFormat(TextValue, OutMessage)) {
    OutCode = TEXT("MRQ_FILENAME_FORMAT_NOT_ALLOWED");
    return false;
  }
  if (Payload->TryGetStringField(TEXT("resolution"), TextValue) &&
      !TextValue.IsEmpty()) {
    FIntPoint Resolution;
    if (!TryParseResolution(TextValue, Resolution)) {
      OutMessage = TEXT("resolution must use WIDTHxHEIGHT format.");
      OutCode = TEXT("INVALID_RESOLUTION");
      return false;
    }
    if (!ValidateResolutionResourceLimits(
            Resolution.X, Resolution.Y, OutMessage, OutCode)) {
      return false;
    }
  }
  int32 Width = 0, Height = 0;
  const bool bHasWidth = Payload->TryGetNumberField(TEXT("width"), Width);
  const bool bHasHeight = Payload->TryGetNumberField(TEXT("height"), Height);
  if (bHasWidth != bHasHeight || (bHasWidth && (Width <= 0 || Height <= 0))) {
    OutMessage = TEXT("width and height must be positive.");
    OutCode = TEXT("INVALID_RESOLUTION");
    return false;
  }
  if (bHasWidth &&
      !ValidateResolutionResourceLimits(Width, Height, OutMessage, OutCode)) {
    return false;
  }
  if (Payload->HasField(TEXT("frameRate"))) {
    FFrameRate FrameRate;
    if (!McpSequenceFrameRate::TryParse(
            Payload, TEXT("frameRate"), FrameRate, OutMessage)) {
      OutCode = TEXT("INVALID_FRAME_RATE");
      return false;
    }
  }
  int32 StartFrame = 0, EndFrame = 0;
  const bool bHasStart =
      Payload->TryGetNumberField(TEXT("startFrame"), StartFrame);
  const bool bHasEnd =
      Payload->TryGetNumberField(TEXT("endFrame"), EndFrame);
  if (bHasStart != bHasEnd) {
    OutMessage = TEXT("startFrame and endFrame must be provided together.");
    OutCode = TEXT("INVALID_FRAME_RANGE");
    return false;
  }
  // End-exclusive range: endFrame == startFrame renders nothing (see the same
  // gate in McpAutomationBridge_SequenceMovieRenderOutput.cpp).
  if (bHasStart && EndFrame <= StartFrame) {
    OutMessage = TEXT("endFrame must be greater than startFrame. The range is "
                      "end-exclusive, so one frame is startFrame..startFrame+1.");
    OutCode = TEXT("INVALID_FRAME_RANGE");
    return false;
  }
  int32 HandleFrameCount = 0;
  if (TryGetSettingsInt(Payload, TEXT("handleFrameCount"),
                                  HandleFrameCount) &&
      HandleFrameCount < 0) {
    OutMessage = TEXT("handleFrameCount must not be negative.");
    OutCode = TEXT("INVALID_FRAME_RANGE");
    return false;
  }
  int32 ZeroPadFrameNumbers = 0;
  if (TryGetSettingsInt(Payload, TEXT("zeroPadFrameNumbers"),
                                  ZeroPadFrameNumbers)) {
    const UMcpAutomationBridgeSettings *Settings =
        GetDefault<UMcpAutomationBridgeSettings>();
    const int32 Maximum =
        FMath::Max(1, Settings
                          ? Settings->MaxMovieRenderZeroPadFrameNumbers
                          : 12);
    if (ZeroPadFrameNumbers < 1 || ZeroPadFrameNumbers > Maximum) {
      OutMessage = FString::Printf(
          TEXT("zeroPadFrameNumbers must be between 1 and %d."), Maximum);
      OutCode = TEXT("INVALID_ZERO_PAD_FRAME_NUMBERS");
      return false;
    }
  }
  if (!ValidateFrameResourceLimits(
          bHasStart, StartFrame, EndFrame, HandleFrameCount, OutMessage,
          OutCode)) {
    return false;
  }
  return true;
}
}

#endif
