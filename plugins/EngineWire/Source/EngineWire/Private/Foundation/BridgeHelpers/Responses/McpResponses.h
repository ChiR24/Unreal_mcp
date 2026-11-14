#pragma once

// Standardized Response Helpers
// See: https://google.github.io/styleguide/jsoncstyleguide.xml

#include "Editor.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Security/McpProjectPaths.h"

// Sets OutError and OutCode and returns false: a refusal in one line, from a function that hands it to its caller.
inline bool McpRefuse(FString &OutError, FString &OutCode, const FString &Error,
                      const TCHAR *Code = TEXT("INVALID_ARGUMENT")) {
  OutError = Error;
  OutCode = Code;
  return false;
}

/**
 * Sends a standardized success response with a "data" envelope.
 *
 * Format:
 * {
 *   "success": true,
 *   "data": { ... },
 *   "warnings": []
 * }
 *
 * No "error" field: a null one only reached every reply as details.error.
 */
static inline void SendStandardSuccessResponse(
    UEngineWireSubsystem *Subsystem,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString &RequestId,
    const FString &Message, const TSharedPtr<FJsonObject> &Data,
    const TArray<FString> &Warnings = TArray<FString>()) {
  if (!Subsystem)
    return;

  TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
  Envelope->SetObjectField(TEXT("data"),
                           Data.IsValid() ? Data : MakeShared<FJsonObject>());

  TArray<TSharedPtr<FJsonValue>> WarningVals = McpHandlerUtils::ToJsonStringArray(Warnings);
  Envelope->SetArrayField(TEXT("warnings"), WarningVals);

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
    UEngineWireSubsystem *Subsystem,
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

// The payload's string field Field into Out; false, after the standard INVALID_ARGUMENT "<Field> required" error, when
// it is missing or empty. A handler answers `if (!McpReadRequiredField(...)) return true;`.
static inline bool McpReadRequiredField(UEngineWireSubsystem *Subsystem, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                        const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
                                        const TCHAR *Field, FString &Out) {
  Payload->TryGetStringField(Field, Out);
  if (!Out.IsEmpty())
    return true;
  SendStandardErrorResponse(Subsystem, Socket, RequestId, TEXT("INVALID_ARGUMENT"),
                            FString::Printf(TEXT("%s required"), Field), nullptr);
  return false;
}

// An asset path sanitized in place; false after the reply when the project-path guard refuses it (SECURITY_VIOLATION,
// in the guard's own words) or, with bMustExist, the asset registry holds nothing there (ASSET_NOT_FOUND).
static inline bool McpSanitizeAssetPathOrReply(UEngineWireSubsystem *Subsystem, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                               const FString &RequestId, FString &Path, bool bMustExist = true) {
  const FString AsGiven = Path;
  Path = SanitizeProjectRelativePath(Path);
  if (Path.IsEmpty()) {
    Subsystem->SendAutomationResponse(Socket, RequestId, false, McpPathRefusalMessage(TEXT("assetPath"), AsGiven),
                                      nullptr, TEXT("SECURITY_VIOLATION"));
    return false;
  }
  if (bMustExist && !McpAssetExists(Path)) {
    Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"), nullptr,
                                      TEXT("ASSET_NOT_FOUND"));
    return false;
  }
  return true;
}

// The payload's assetPath through McpSanitizeAssetPathOrReply; a missing one answers INVALID_ARGUMENT "assetPath required".
static inline bool McpReadAssetPathOrReply(UEngineWireSubsystem *Subsystem, TSharedPtr<FMcpBridgeWebSocket> Socket,
                                           const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
                                           FString &OutPath, bool bMustExist = true) {
  Payload->TryGetStringField(TEXT("assetPath"), OutPath);
  if (OutPath.IsEmpty()) {
    Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"), nullptr,
                                      TEXT("INVALID_ARGUMENT"));
    return false;
  }
  return McpSanitizeAssetPathOrReply(Subsystem, Socket, RequestId, OutPath, bMustExist);
}
