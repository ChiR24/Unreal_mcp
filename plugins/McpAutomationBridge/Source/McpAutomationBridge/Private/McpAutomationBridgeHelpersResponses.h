#pragma once

// Standardized Response Helpers
// See: https://google.github.io/styleguide/jsoncstyleguide.xml

/**
 * Sends a standardized success response with a "data" envelope.
 *
 * Format:
 * {
 *   "success": true,
 *   "data": { ... },
 *   "warnings": [],
 *   "error": null
 * }
 */
static inline void SendStandardSuccessResponse(
    UMcpAutomationBridgeSubsystem *Subsystem,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
    const FString &Message, const TSharedPtr<FJsonObject> &Data,
    const TArray<FString> &Warnings = TArray<FString>()) {
  if (!Subsystem)
    return;

  TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
  Envelope->SetBoolField(TEXT("success"), true);
  Envelope->SetObjectField(TEXT("data"),
                           Data.IsValid() ? Data : MakeShared<FJsonObject>());

  TArray<TSharedPtr<FJsonValue>> WarningVals;
  for (const FString &W : Warnings) {
    WarningVals.Add(MakeShared<FJsonValueString>(W));
  }
  Envelope->SetArrayField(TEXT("warnings"), WarningVals);

  Envelope->SetField(TEXT("error"), MakeShared<FJsonValueNull>());

  Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, Envelope,
                                    FString());
}

/**
 * Sends a standardized error response with structured error details.
 *
 * Format:
 * {
 *   "success": false,
 *   "data": {},   // Empty object for schema compliance
 *   "error": {
 *     "code": "ERROR_CODE",
 *     "message": "Human readable message",
 *     "parameter": "optional_param_name",
 *     ...
 *   }
 * }
 */
static inline void SendStandardErrorResponse(
    UMcpAutomationBridgeSubsystem *Subsystem,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
    const FString &ErrorCode, const FString &ErrorMessage,
    const TSharedPtr<FJsonObject> &ErrorDetails = nullptr) {
  if (!Subsystem)
    return;

  TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
  Envelope->SetBoolField(TEXT("success"), false);

  // CRITICAL: Add empty data object for schema compliance
  // The MCP schema requires data: { type: 'object' } in all responses
  Envelope->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());

  TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
  ErrorObj->SetStringField(TEXT("code"), ErrorCode);
  ErrorObj->SetStringField(TEXT("message"), ErrorMessage);

  if (ErrorDetails.IsValid()) {
    // Merge details into error object
    for (const auto &Pair : ErrorDetails->Values) {
      ErrorObj->SetField(Pair.Key, Pair.Value);
    }
  }

  Envelope->SetObjectField(TEXT("error"), ErrorObj);

  Subsystem->SendAutomationResponse(Socket, RequestId, false, ErrorMessage,
                                    Envelope, ErrorCode);
}
