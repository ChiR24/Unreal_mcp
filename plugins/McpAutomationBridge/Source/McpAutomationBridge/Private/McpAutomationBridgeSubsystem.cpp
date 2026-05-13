// Ensure the subsystem type and bridge socket helpers are available before the
// generated subsystem header pulls in UE version-dependent declarations.
#if WITH_EDITOR
#include "McpAutomationBridgeHelpers.h"
#endif

#include "McpAutomationBridgeSubsystem.h"
#include "MCP/McpNativeTransport.h"
#include "HAL/PlatformTLS.h"
#include "Interfaces/IPluginManager.h"

// =============================================================================
// FMcpRequestErrorDevice - Custom log device for per-request error capture
// =============================================================================

static constexpr int32 MaxCapturedRequestMessages = 32;
static constexpr int32 MaxCapturedRequestMessageChars = 1024;

static bool IsKnownBenignMcpCompilerWarning(const FString& Message)
{
    return Message.Contains(TEXT("CooldownGameplayEffectClass"), ESearchCase::IgnoreCase) &&
        (Message.Contains(TEXT("no tags"), ESearchCase::IgnoreCase) ||
         Message.Contains(TEXT("grant any tags"), ESearchCase::IgnoreCase) ||
         Message.Contains(TEXT("grants no tag"), ESearchCase::IgnoreCase) ||
         Message.Contains(TEXT("granting no tag"), ESearchCase::IgnoreCase));
}

/**
 * Custom output device that captures errors and warnings during request processing.
 * This is temporarily attached to GLog during handler execution to detect
 * engine-level errors (like ensure failures) that don't propagate as exceptions.
 * 
 * Note: Uses the subsystem's shared capture with mutex-protected access and
 * only records messages from the request thread that enabled capture.
 */
class FMcpRequestErrorDevice : public FOutputDevice
{
public:
    FMcpRequestErrorDevice(UMcpAutomationBridgeSubsystem* InSubsystem)
        : Subsystem(InSubsystem)
    {
    }

    virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        // Only capture Error and Warning verbosity
        if (Verbosity == ELogVerbosity::Error || Verbosity == ELogVerbosity::Warning)
        {
            if (!Subsystem)
            {
                return;
            }

            // Thread-safe access to shared capture
            FScopeLock Lock(&Subsystem->ErrorCaptureMutex);
            auto& Capture = Subsystem->CurrentErrorCapture;
            if (!Capture.bActive ||
                Capture.CapturingThreadId != FPlatformTLS::GetCurrentThreadId())
            {
                return;
            }

            FString Message = FString::Printf(TEXT("[%s] %s"), *Category.ToString(), V);
            if (Message.Len() > MaxCapturedRequestMessageChars)
            {
                Message = Message.Left(MaxCapturedRequestMessageChars) + TEXT("[TRUNCATED]");
            }

            const bool bTreatAsWarning =
                Verbosity == ELogVerbosity::Warning || IsKnownBenignMcpCompilerWarning(Message);

            if (!bTreatAsWarning)
            {
                ++Capture.ErrorCount;
                if (Capture.ErrorMessages.Num() < MaxCapturedRequestMessages)
                {
                    Capture.ErrorMessages.Add(Message);
                }
                else
                {
                    Capture.bErrorMessagesTruncated = true;
                }
                Capture.bHasErrors = true;
            }
            else
            {
                ++Capture.WarningCount;
                if (Capture.WarningMessages.Num() < MaxCapturedRequestMessages)
                {
                    Capture.WarningMessages.Add(Message);
                }
                else
                {
                    Capture.bWarningMessagesTruncated = true;
                }
                Capture.bHasWarnings = true;
            }
        }
        
        // Note: We do not explicitly forward here. When this device is attached to GLog,
        // the engine still routes messages to all other output devices; this class only
        // captures errors and warnings without suppressing normal logging.
    }

    virtual bool CanBeUsedOnAnyThread() const override
    {
        return true;
    }

    virtual bool CanBeUsedOnMultipleThreads() const override
    {
        return true;
    }

private:
    UMcpAutomationBridgeSubsystem* Subsystem = nullptr;
};
#include "Dom/JsonObject.h"
#include "Async/TaskGraphInterfaces.h"
#include "Async/Async.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSettings.h"
#include "McpBridgeWebSocket.h"
#include "McpConnectionManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Editor-only includes for ExecuteEditorCommands
#if WITH_EDITOR
#include "Editor.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

// Define the subsystem log category declared in the public header.
DEFINE_LOG_CATEGORY(LogMcpAutomationBridgeSubsystem);

/**
 * @brief Produces a log-safe copy of a string by replacing control characters
 * and truncating long input.
 *
 * Creates a sanitized version of the input string where characters with code
 * points less than 32 or equal to 127 are replaced with '?' and the result is
 * truncated to 512 characters with "[TRUNCATED]" appended if the input is
 * longer.
 *
 * @param In Input string to sanitize.
 * @return FString Sanitized string suitable for logging.
 */
static inline FString SanitizeForLog(const FString &In) {
  if (In.IsEmpty())
    return FString();
  FString Out;
  Out.Reserve(FMath::Min<int32>(In.Len(), 1024));
  for (int32 i = 0; i < In.Len(); ++i) {
    const TCHAR C = In[i];
    if (C >= 32 && C != 127)
      Out.AppendChar(C);
    else
      Out.AppendChar('?');
  }
  if (Out.Len() > 512)
    Out = Out.Left(512) + TEXT("[TRUNCATED]");
  return Out;
}

static inline bool IsAllowedUnrealMountPrefixAt(
    const FString &Value, int32 Index, const TCHAR *Mount) {
  const int32 MountLen = FCString::Strlen(Mount);
  if (Index + MountLen > Value.Len()) {
    return false;
  }

  if (!Value.Mid(Index, MountLen).Equals(Mount, ESearchCase::CaseSensitive)) {
    return false;
  }

  const int32 AfterMount = Index + MountLen;
  return AfterMount == Value.Len() || Value[AfterMount] == '/';
}

static inline bool IsAllowedUnrealMountPath(const FString &Value, int32 Index) {
  return IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Game")) ||
         IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Engine")) ||
         IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Script")) ||
         IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Temp")) ||
         IsAllowedUnrealMountPrefixAt(Value, Index, TEXT("/Niagara"));
}

static inline bool IsResponsePathChar(TCHAR C) {
  return FChar::IsAlnum(C) || C == '/' || C == '\\' || C == '.' ||
         C == '_' || C == '-' || C == ':' || C == '(' || C == ')' ||
         C == '+' || C == '~' || C == ' ';
}

static inline bool IsUnrealMountPathChar(TCHAR C) {
  return FChar::IsAlnum(C) || C == '/' || C == '.' || C == '_' ||
         C == '-' || C == ':';
}

static FString RedactFilesystemPathsForResponse(const FString &Input) {
  FString Output;
  Output.Reserve(Input.Len());

  for (int32 Index = 0; Index < Input.Len();) {
    const bool bAllowedUnrealPath = Input[Index] == '/' &&
                                    IsAllowedUnrealMountPath(Input, Index);
    const bool bUnixPath = Input[Index] == '/' && !bAllowedUnrealPath;
    const bool bWindowsPath = Index + 2 < Input.Len() &&
                              FChar::IsAlpha(Input[Index]) &&
                              Input[Index + 1] == ':' &&
                              (Input[Index + 2] == '/' || Input[Index + 2] == '\\');
    const bool bUncPath = Index + 1 < Input.Len() &&
                          Input[Index] == '\\' && Input[Index + 1] == '\\';

    if (bAllowedUnrealPath) {
      while (Index < Input.Len() && IsUnrealMountPathChar(Input[Index])) {
        Output.AppendChar(Input[Index]);
        ++Index;
      }
      continue;
    }

    if (bUnixPath || bWindowsPath || bUncPath) {
      Output += TEXT("[path redacted]");
      while (Index < Input.Len() && IsResponsePathChar(Input[Index])) {
        ++Index;
      }
      continue;
    }

    Output.AppendChar(Input[Index]);
    ++Index;
  }

  return Output;
}

static void RedactFollowingValueForResponse(FString &Text, const FString &Marker) {
  const auto IsValueDelimiter = [](TCHAR C) {
    return C == ';' || C == '&' || C == ',' || C == '\r' || C == '\n';
  };
  const auto IsHeaderDelimiter = [](TCHAR C) {
    return C == ';' || C == '&' || C == '\r' || C == '\n';
  };

  int32 SearchStart = 0;
  while (SearchStart < Text.Len()) {
    const int32 MarkerIndex = Text.Find(
        Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchStart);
    if (MarkerIndex == INDEX_NONE) {
      return;
    }

    const int32 ValueStart = MarkerIndex + Marker.Len();
    const bool bAllowWhitespaceInValue = Marker.EndsWith(TEXT(":"));
    int32 ValueEnd = ValueStart;
    if (bAllowWhitespaceInValue) {
      while (ValueEnd < Text.Len() && FChar::IsWhitespace(Text[ValueEnd]) &&
             Text[ValueEnd] != '\r' && Text[ValueEnd] != '\n') {
        ++ValueEnd;
      }
      while (ValueEnd < Text.Len() && !IsHeaderDelimiter(Text[ValueEnd])) {
        ++ValueEnd;
      }
    } else {
      while (ValueEnd < Text.Len() && !IsValueDelimiter(Text[ValueEnd]) &&
             !FChar::IsWhitespace(Text[ValueEnd])) {
        ++ValueEnd;
      }
    }

    Text = Text.Left(ValueStart) + TEXT("[redacted]") + Text.Mid(ValueEnd);
    SearchStart = ValueStart + 10;
  }
}

static FString SanitizeEngineErrorForResponse(const FString &In) {
  FString Out = RedactFilesystemPathsForResponse(SanitizeForLog(In));
  RedactFollowingValueForResponse(Out, TEXT("token="));
  RedactFollowingValueForResponse(Out, TEXT("capabilitytoken="));
  RedactFollowingValueForResponse(Out, TEXT("password="));
  RedactFollowingValueForResponse(Out, TEXT("secret="));
  RedactFollowingValueForResponse(Out, TEXT("api_key="));
  RedactFollowingValueForResponse(Out, TEXT("apikey="));
  RedactFollowingValueForResponse(Out, TEXT("authorization:"));
  RedactFollowingValueForResponse(Out, TEXT("bearer "));

  if (Out.Len() > 512) {
    Out = Out.Left(512) + TEXT("[TRUNCATED]");
  }
  return Out;
}

/**
 * @brief Initialize the automation bridge subsystem, preparing networking,
 * handlers, and periodic processing.
 *
 * Creates and initializes the connection manager, registers automation action
 * handlers and a message-received callback, starts the connection manager, and
 * registers a recurring ticker to process pending automation requests.
 *
 * NOTE: This subsystem is intentionally disabled during commandlet execution
 * (cooking, packaging, etc.) to prevent the WebSocket server from interfering
 * with cook operations and blocking writes to the staged build directory.
 *
 * @param Collection Subsystem collection provided by the engine during
 * initialization.
 */
void UMcpAutomationBridgeSubsystem::Initialize(
    FSubsystemCollectionBase &Collection) {
  Super::Initialize(Collection);

  // Skip initialization during commandlet execution (cooking, packaging, etc.)
  // The WebSocket server and background threads can interfere with cook
  // operations, particularly file I/O to the staged build directory.
  if (IsRunningCommandlet()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("McpAutomationBridgeSubsystem skipping initialization - running "
                "as commandlet (cook/package mode)."));
    return;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("McpAutomationBridgeSubsystem initializing."));

  // Create and initialize the connection manager
  ConnectionManager = MakeShared<FMcpConnectionManager>();
  ConnectionManager->Initialize(GetDefault<UMcpAutomationBridgeSettings>());

  // Bind message received delegate
  ConnectionManager->SetOnMessageReceived(
      FMcpMessageReceivedCallback::CreateWeakLambda(
          this, [this](const FString &RequestId, const FString &Action,
                       const TSharedPtr<FJsonObject> &Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket) {
            QueueAutomationRequest(RequestId, Action, Payload, Socket);
          }));

  // Initialize the handler registry
  InitializeHandlers();

  // Start the connection manager
  ConnectionManager->Start();

  // Native MCP Streamable HTTP transport (opt-in)
  {
    const auto* Settings = GetDefault<UMcpAutomationBridgeSettings>();
    if (Settings && Settings->bEnableNativeMCP)
    {
      // Find plugin directory for loading tool schemas
      FString PluginDir;
      TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("McpAutomationBridge"));
      if (Plugin.IsValid())
      {
        PluginDir = Plugin->GetBaseDir();
      }

      NativeTransport = MakeShared<FMcpNativeTransport>(this);
      if (!NativeTransport->Start(Settings->NativeMCPPort, PluginDir, Settings->bLoadAllToolsOnStart,
                                  Settings->NativeMCPInstructions,
                                  Settings->ListenHost, Settings->bAllowNonLoopback))
      {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
               TEXT("Failed to start Native MCP server on port %d"), Settings->NativeMCPPort);
        NativeTransport.Reset();
      }
    }
  }

  // Register Ticker. Automation requests are drained here, after the engine
  // world tick has completed, so map transitions do not run from arbitrary
  // GameThread task-graph points inside active tick groups.
  TickHandle = FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateUObject(this,
                                     &UMcpAutomationBridgeSubsystem::Tick),
      0.0f
  );

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("McpAutomationBridgeSubsystem Initialized."));
}

/**
 * @brief Shuts down the MCP Automation Bridge subsystem and releases its
 * resources.
 *
 * Removes the registered ticker, stops and clears the connection manager,
 * detaches and clears the log capture device, and calls the superclass
 * deinitialization.
 *
 * NOTE: During commandlet execution (cooking, packaging), the subsystem
 * may not have fully initialized, so cleanup checks are defensive.
 */
void UMcpAutomationBridgeSubsystem::Deinitialize() {
  // Remove ticker if it was registered (won't be valid if we skipped init
  // during commandlet)
  if (TickHandle.IsValid()) {
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    TickHandle.Reset();
  }

  // Skip verbose logging during commandlet mode since we didn't fully
  // initialize
  if (!IsRunningCommandlet()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("McpAutomationBridgeSubsystem deinitializing."));
  }

  if (NativeTransport)
  {
    NativeTransport->Shutdown();
    NativeTransport.Reset();
  }

  if (ConnectionManager.IsValid()) {
    ConnectionManager->Stop();
    ConnectionManager.Reset();
  }

  if (LogCaptureDevice.IsValid()) {
    if (GLog)
      GLog->RemoveOutputDevice(LogCaptureDevice.Get());
    LogCaptureDevice.Reset();
  }

  // Clean up RequestErrorDevice to prevent dangling pointer in GLog
  if (RequestErrorDevice.IsValid()) {
    FScopeLock Lock(&ErrorCaptureMutex);
    if (GLog)
      GLog->RemoveOutputDevice(RequestErrorDevice.Get());
    RequestErrorDevice.Reset();
  }

  Super::Deinitialize();
}

/**
 * @brief Reports whether the automation bridge currently has any active
 * connections.
 *
 * @return `true` if the connection manager exists and has one or more active
 * sockets, `false` otherwise.
 */
bool UMcpAutomationBridgeSubsystem::IsBridgeActive() const {
  return ConnectionManager.IsValid() &&
         ConnectionManager->GetActiveSocketCount() > 0;
}

/**
 * @brief Determine the bridge's connection state from active sockets.
 *
 * Maps the connection manager's state to the subsystem's bridge state enum.
 * Returns Connected if active sockets exist, Connecting if a reconnect is
 * pending, or Disconnected otherwise.
 *
 * @return EMcpAutomationBridgeState The current connection state.
 */
EMcpAutomationBridgeState
UMcpAutomationBridgeSubsystem::GetBridgeState() const {
  if (ConnectionManager.IsValid()) {
    if (ConnectionManager->GetActiveSocketCount() > 0) {
      return EMcpAutomationBridgeState::Connected;
    }
    if (ConnectionManager->IsReconnectPending()) {
      return EMcpAutomationBridgeState::Connecting;
    }
  }
  return EMcpAutomationBridgeState::Disconnected;
}

// =============================================================================
// Per-Request Error Capture Implementation
// =============================================================================

UMcpAutomationBridgeSubsystem::FRequestErrorCapture& UMcpAutomationBridgeSubsystem::GetCurrentErrorCapture()
{
    return CurrentErrorCapture;
}

void UMcpAutomationBridgeSubsystem::BeginErrorCapture()
{
    // Create and attach the error capture device if not already
    if (!RequestErrorDevice.IsValid())
    {
        RequestErrorDevice = MakeShared<FMcpRequestErrorDevice>(this);
    }

    {
        // Clear any previous capture state (thread-safe)
        FScopeLock Lock(&ErrorCaptureMutex);
        CurrentErrorCapture.Reset();
        CurrentErrorCapture.CapturingThreadId = FPlatformTLS::GetCurrentThreadId();
        CurrentErrorCapture.bActive = true;
    }

    // Attach to GLog to capture errors
    if (GLog && RequestErrorDevice.IsValid())
    {
        GLog->AddOutputDevice(RequestErrorDevice.Get());
    }
}

TArray<FString> UMcpAutomationBridgeSubsystem::EndErrorCapture()
{
    // Detach the error capture device
    if (GLog && RequestErrorDevice.IsValid())
    {
        GLog->RemoveOutputDevice(RequestErrorDevice.Get());
    }
    
    // Get captured errors (thread-safe)
    FScopeLock Lock(&ErrorCaptureMutex);

    TArray<FString> AllMessages;
    AllMessages.Reserve(CurrentErrorCapture.ErrorMessages.Num() +
                        CurrentErrorCapture.WarningMessages.Num());
    for (const FString& ErrorMessage : CurrentErrorCapture.ErrorMessages)
    {
        AllMessages.Add(SanitizeEngineErrorForResponse(ErrorMessage));
    }
    for (const FString& WarningMessage : CurrentErrorCapture.WarningMessages)
    {
        AllMessages.Add(SanitizeEngineErrorForResponse(WarningMessage));
    }
    CurrentErrorCapture.bActive = false;
    CurrentErrorCapture.CapturingThreadId = 0;
    
    return AllMessages;
}

bool UMcpAutomationBridgeSubsystem::HasCapturedErrors() const
{
    FScopeLock Lock(&ErrorCaptureMutex);
    return CurrentErrorCapture.bHasErrors.load();
}

TArray<FString> UMcpAutomationBridgeSubsystem::GetCapturedErrorMessages() const
{
    FScopeLock Lock(&ErrorCaptureMutex);
    TArray<FString> SanitizedMessages;
    SanitizedMessages.Reserve(CurrentErrorCapture.ErrorMessages.Num());
    for (const FString& ErrorMessage : CurrentErrorCapture.ErrorMessages)
    {
        SanitizedMessages.Add(SanitizeEngineErrorForResponse(ErrorMessage));
    }
    return SanitizedMessages;
}

/**
 * @brief Forward a raw text message to the connection manager for transmission.
 *
 * @param Message The raw message string to send.
 * @return `true` if the connection manager accepted the message for sending,
 * `false` otherwise.
 */
bool UMcpAutomationBridgeSubsystem::SendRawMessage(const FString &Message) {
  if (ConnectionManager.IsValid()) {
    return ConnectionManager->SendRawMessage(Message);
  }
  return false;
}

/**
 * @brief Per-frame tick that processes deferred automation requests when it is
 * safe to do so.
 *
 * Invokes processing of any pending automation requests that were previously
 * deferred due to unsafe engine states (saving, garbage collection, or async
 * loading).
 *
 * @param DeltaTime Time elapsed since the last tick, in seconds.
 * @return true to remain registered and continue receiving ticks.
 */
bool UMcpAutomationBridgeSubsystem::Tick(float DeltaTime) {
  // Drain pending requests only outside unsafe engine states.
  if (!GIsSavingPackage && !IsGarbageCollecting() && !IsAsyncLoading()) {
    ProcessPendingAutomationRequests();
  }
  // Cleanup stale HTTP pending requests (5 minute timeout)
  if (NativeTransport)
  {
    NativeTransport->CleanupStaleRequests();
  }
  return true;
}

void UMcpAutomationBridgeSubsystem::QueueAutomationRequest(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    ERequestOrigin Origin) {
  FPendingAutomationRequest Pending;
  Pending.RequestId = RequestId;
  Pending.Action = Action;
  Pending.Payload = Payload;
  Pending.RequestingSocket = RequestingSocket;
  Pending.Origin = Origin;

  {
    FScopeLock Lock(&PendingAutomationRequestsMutex);
    PendingAutomationRequests.Add(MoveTemp(Pending));
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("Queued automation request for core ticker: RequestId=%s action=%s"),
         *RequestId, *Action);
}

// The in-file implementation of ProcessAutomationRequest was intentionally
// removed from this translation unit. The function is now implemented in
// McpAutomationBridge_ProcessRequest.cpp to avoid duplicate definitions and
// to keep this file focused. See that file for the full request dispatcher
/**
 * @brief Sends an automation response for a specific request to the given
 * socket.
 *
 * If the connection manager is not available this call is a no-op.
 *
 * @param TargetSocket WebSocket to which the response will be sent.
 * @param RequestId Identifier of the automation request being responded to.
 * @param bSuccess `true` if the request succeeded, `false` otherwise.
 * @param Message Human-readable message or description associated with the
 * response.
 * @param Result Optional JSON object containing result data; may be null.
 * @param ErrorCode Error code string to include when `bSuccess` is `false`.
 *
 * Successful handler responses are converted to ENGINE_ERROR failures when
 * Unreal logged errors during the active automation request. This keeps tool
 * results aligned with the editor log instead of reporting success for a
 * request whose handler triggered engine errors.
 */

void UMcpAutomationBridgeSubsystem::SendAutomationResponse(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString &RequestId,
    const bool bSuccess, const FString &Message,
    const TSharedPtr<FJsonObject> &Result, const FString &ErrorCode,
    ERequestOrigin Origin) {
  bool bEffectiveSuccess = bSuccess;
  FString EffectiveMessage = Message;
  FString EffectiveErrorCode = ErrorCode;
  TSharedPtr<FJsonObject> EffectiveResult = Result;

  if (bSuccess && bProcessingAutomationRequest)
  {
    TArray<FString> CapturedErrors;
    int32 TotalCapturedErrorCount = 0;
    bool bCapturedErrorsTruncated = false;
    {
      FScopeLock Lock(&ErrorCaptureMutex);
      if (CurrentErrorCapture.bHasErrors.load())
      {
        CapturedErrors = CurrentErrorCapture.ErrorMessages;
        TotalCapturedErrorCount = CurrentErrorCapture.ErrorCount;
        bCapturedErrorsTruncated = CurrentErrorCapture.bErrorMessagesTruncated;
      }
    }

    if (CapturedErrors.Num() > 0)
    {
      bEffectiveSuccess = false;
      EffectiveErrorCode = TEXT("ENGINE_ERROR");
      const FString FirstError = SanitizeEngineErrorForResponse(CapturedErrors[0]);
      EffectiveMessage = FString::Printf(
          TEXT("Handler reported success but Unreal logged errors: %s"),
          *FirstError.Left(240));

      TSharedPtr<FJsonObject> AugmentedResult = MakeShared<FJsonObject>();
      if (Result.IsValid())
      {
        for (const auto &Pair : Result->Values)
        {
          AugmentedResult->SetField(Pair.Key, Pair.Value);
        }
      }

      TArray<TSharedPtr<FJsonValue>> ErrorValues;
      const int32 MaxErrorsInResponse = 3;
      const int32 ErrorResponseCount = FMath::Min(CapturedErrors.Num(), MaxErrorsInResponse);
      for (int32 ErrorIndex = 0; ErrorIndex < ErrorResponseCount; ++ErrorIndex)
      {
        ErrorValues.Add(MakeShared<FJsonValueString>(
            SanitizeEngineErrorForResponse(CapturedErrors[ErrorIndex])));
      }
      AugmentedResult->SetBoolField(TEXT("success"), false);
      AugmentedResult->SetNumberField(TEXT("engineErrorCount"), TotalCapturedErrorCount);
      AugmentedResult->SetArrayField(TEXT("engineErrors"), ErrorValues);
      if (bCapturedErrorsTruncated || CapturedErrors.Num() > MaxErrorsInResponse)
      {
        AugmentedResult->SetBoolField(TEXT("engineErrorsTruncated"), true);
      }

      TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
      ErrorObj->SetStringField(TEXT("code"), EffectiveErrorCode);
      ErrorObj->SetStringField(TEXT("message"), EffectiveMessage);
      AugmentedResult->SetObjectField(TEXT("error"), ErrorObj);
      EffectiveResult = AugmentedResult;
    }
  }

  if (!bEffectiveSuccess)
  {
    EffectiveMessage = SanitizeEngineErrorForResponse(EffectiveMessage);
  }

  // When handlers omit Origin (default WebSocket), use the stored
  // CurrentRequestOrigin from the active ProcessAutomationRequest call.
  ERequestOrigin EffectiveOrigin = (Origin == ERequestOrigin::WebSocket)
      ? CurrentRequestOrigin : Origin;
  // Some handlers complete later via AsyncTask(GameThread) after the request
  // scope has already reset CurrentRequestOrigin. A live native pending request
  // is authoritative in that case and must receive the response over SSE.
  if (Origin == ERequestOrigin::WebSocket && NativeTransport &&
      NativeTransport->HasPendingRequest(RequestId))
  {
    EffectiveOrigin = ERequestOrigin::NativeHTTP;
  }
  if (EffectiveOrigin == ERequestOrigin::NativeHTTP && NativeTransport)
  {
    if (!NativeTransport->CompletePendingRequest(RequestId, bEffectiveSuccess, EffectiveMessage, EffectiveResult, EffectiveErrorCode))
    {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("Native HTTP response for %s dropped — request already expired or unknown"),
        *RequestId);
    }
    return;
  }
  if (ConnectionManager.IsValid()) {
    ConnectionManager->SendAutomationResponse(TargetSocket, RequestId, bEffectiveSuccess,
                                              EffectiveMessage, EffectiveResult, EffectiveErrorCode);
  }
}

/**
 * @brief Log a failure and send a standardized automation error response.
 *
 * Resolves an empty ErrorCode to "AUTOMATION_ERROR", logs a sanitized warning
 * with the resolved error and message, and sends a failure response for the
 * specified request.
 *
 * @param TargetSocket Optional socket to target the response; may be null to
 * broadcast or use a default.
 * @param RequestId Identifier of the automation request that failed.
 * @param Message Human-readable failure message.
 * @param ErrorCode Error code to include with the response; "AUTOMATION_ERROR"
 * is used if empty.
 */
void UMcpAutomationBridgeSubsystem::SendAutomationError(
    TSharedPtr<FMcpBridgeWebSocket> TargetSocket, const FString &RequestId,
    const FString &Message, const FString &ErrorCode) {
  const FString ResolvedError =
      ErrorCode.IsEmpty() ? TEXT("AUTOMATION_ERROR") : ErrorCode;
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("Automation request failed (%s): %s"), *ResolvedError,
         *SanitizeForLog(Message));
  SendAutomationResponse(TargetSocket, RequestId, false, Message, nullptr,
                         ResolvedError);
}

/**
 * @brief Send a progress update during long-running operations.
 *
 * Sends a progress_update message to the MCP server to extend the request
 * timeout and provide status feedback. This prevents timeout errors when
 * UE is actively working on a task.
 *
 * @param RequestId The request ID being tracked
 * @param Percent Optional progress percent (0-100), use negative to omit
 * @param Message Optional status message describing current operation
 * @param bStillWorking True if operation is still in progress
 */
void UMcpAutomationBridgeSubsystem::SendProgressUpdate(
    const FString &RequestId, float Percent, const FString &Message, bool bStillWorking,
    ERequestOrigin Origin) {
  if (Origin == ERequestOrigin::NativeHTTP && NativeTransport)
  {
    NativeTransport->SendSSEProgressUpdate(RequestId, Percent, Message);
    return;
  }
  if (ConnectionManager.IsValid()) {
    ConnectionManager->SendProgressUpdate(RequestId, Percent, Message, bStillWorking);
  }
}

/**
 * @brief Records telemetry for an automation request with outcome details.
 *
 * Forwards the request identifier, success flag, human-readable message, and
 * error code to the connection manager for telemetry/logging.
 *
 * @param RequestId Unique identifier of the automation request.
 * @param bSuccess `true` if the request completed successfully, `false`
 * otherwise.
 * @param Message Human-readable message describing the outcome or context.
 * @param ErrorCode Short error identifier (empty if none).
 */
void UMcpAutomationBridgeSubsystem::RecordAutomationTelemetry(
    const FString &RequestId, const bool bSuccess, const FString &Message,
    const FString &ErrorCode) {
  if (ConnectionManager.IsValid()) {
    ConnectionManager->RecordAutomationTelemetry(RequestId, bSuccess, Message,
                                                 ErrorCode);
  }
}

/**
 * @brief Registers an automation action handler for the given action string.
 *
 * If a non-empty handler is provided, stores it under Action (replacing any
 * existing handler for the same key). If Handler is null/invalid, the call is a
 * no-op.
 *
 * @param Action The action identifier string used to look up the handler.
 * @param Handler Callable invoked when the specified action is requested.
 */
void UMcpAutomationBridgeSubsystem::RegisterHandler(
    const FString &Action, FAutomationHandler Handler) {
  if (Handler) {
    AutomationHandlers.Add(Action, Handler);
  }
}

/**
 * @brief Registers all automation action handlers used by the MCP Automation
 * Bridge.
 *
 * Populates the subsystem's handler registry with mappings from action name
 * strings (for example: core/property actions, array/map/set container ops,
 * asset dependency queries, console/system and editor tooling actions,
 * blueprint/world/asset management, rendering/materials, input/control,
 * audio/lighting/physics/effects, and performance actions) to the functions
 * that handle those actions so incoming automation requests can be dispatched
 * by action name.
 *
 * This also registers a few common alias actions (e.g., "create_effect",
 * "clear_debug_shapes") so those actions dispatch directly to the intended
 * handler.
 */
void UMcpAutomationBridgeSubsystem::InitializeHandlers() {
  // Core & Properties
  RegisterHandler(TEXT("execute_editor_function"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleExecuteEditorFunction(R, A, P, S);
                  });
  RegisterHandler(TEXT("set_object_property"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSetObjectProperty(R, A, P, S);
                  });
  RegisterHandler(TEXT("get_object_property"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleGetObjectProperty(R, A, P, S);
                  });

  // Containers (Arrays, Maps, Sets)
  RegisterHandler(TEXT("array_append"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleArrayAppend(R, A, P, S);
                  });
  RegisterHandler(TEXT("array_remove"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleArrayRemove(R, A, P, S);
                  });
  RegisterHandler(TEXT("array_insert"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleArrayInsert(R, A, P, S);
                  });
  RegisterHandler(TEXT("array_get_element"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleArrayGetElement(R, A, P, S);
                  });
  RegisterHandler(TEXT("array_set_element"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleArraySetElement(R, A, P, S);
                  });
  RegisterHandler(TEXT("array_clear"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleArrayClear(R, A, P, S);
                  });

  RegisterHandler(TEXT("map_set_value"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMapSetValue(R, A, P, S);
                  });
  RegisterHandler(TEXT("map_get_value"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMapGetValue(R, A, P, S);
                  });
  RegisterHandler(TEXT("map_remove_key"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMapRemoveKey(R, A, P, S);
                  });
  RegisterHandler(TEXT("map_has_key"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMapHasKey(R, A, P, S);
                  });
  RegisterHandler(TEXT("map_get_keys"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMapGetKeys(R, A, P, S);
                  });
  RegisterHandler(TEXT("map_clear"), [this](const FString &R, const FString &A,
                                            const TSharedPtr<FJsonObject> &P,
                                            TSharedPtr<FMcpBridgeWebSocket> S) {
    return HandleMapClear(R, A, P, S);
  });

  RegisterHandler(TEXT("set_add"), [this](const FString &R, const FString &A,
                                          const TSharedPtr<FJsonObject> &P,
                                          TSharedPtr<FMcpBridgeWebSocket> S) {
    return HandleSetAdd(R, A, P, S);
  });
  RegisterHandler(TEXT("set_remove"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSetRemove(R, A, P, S);
                  });
  RegisterHandler(TEXT("set_contains"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSetContains(R, A, P, S);
                  });
  RegisterHandler(TEXT("set_clear"), [this](const FString &R, const FString &A,
                                            const TSharedPtr<FJsonObject> &P,
                                            TSharedPtr<FMcpBridgeWebSocket> S) {
    return HandleSetClear(R, A, P, S);
  });

  // Asset Dependency
  RegisterHandler(TEXT("get_asset_references"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleGetAssetReferences(R, A, P, S);
                  });
  RegisterHandler(TEXT("get_asset_dependencies"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleGetAssetDependencies(R, A, P, S);
                  });

  // Asset Workflow
  RegisterHandler(TEXT("fixup_redirectors"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleFixupRedirectors(R, A, P, S);
                  });
  RegisterHandler(TEXT("source_control_checkout"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSourceControlCheckout(R, A, P, S);
                  });
  RegisterHandler(TEXT("source_control_submit"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSourceControlSubmit(R, A, P, S);
                  });
  RegisterHandler(TEXT("bulk_rename_assets"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleBulkRenameAssets(R, A, P, S);
                  });
  RegisterHandler(TEXT("bulk_delete_assets"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleBulkDeleteAssets(R, A, P, S);
                  });
  RegisterHandler(TEXT("generate_thumbnail"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleGenerateThumbnail(R, A, P, S);
                  });

  // Landscape
  RegisterHandler(TEXT("create_landscape"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleCreateLandscape(R, A, P, S);
                  });
  RegisterHandler(TEXT("create_procedural_terrain"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleCreateProceduralTerrain(R, A, P, S);
                  });
  RegisterHandler(TEXT("create_landscape_grass_type"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleCreateLandscapeGrassType(R, A, P, S);
                  });
  RegisterHandler(TEXT("sculpt_landscape"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSculptLandscape(R, A, P, S);
                  });
  RegisterHandler(TEXT("set_landscape_material"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSetLandscapeMaterial(R, A, P, S);
                  });
  RegisterHandler(TEXT("modify_heightmap"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleModifyHeightmap(R, A, P, S);
                  });
  RegisterHandler(TEXT("edit_landscape"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleEditLandscape(R, A, P, S);
                  });

  // Foliage
  RegisterHandler(TEXT("add_foliage_type"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAddFoliageType(R, A, P, S);
                  });
  RegisterHandler(TEXT("create_procedural_foliage"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleCreateProceduralFoliage(R, A, P, S);
                  });
  RegisterHandler(TEXT("paint_foliage"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandlePaintFoliage(R, A, P, S);
                  });
  RegisterHandler(TEXT("add_foliage_instances"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAddFoliageInstances(R, A, P, S);
                  });
  RegisterHandler(TEXT("remove_foliage"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleRemoveFoliage(R, A, P, S);
                  });
  RegisterHandler(TEXT("get_foliage_instances"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleGetFoliageInstances(R, A, P, S);
                  });

  // Niagara
  RegisterHandler(TEXT("create_niagara_system"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleCreateNiagaraSystem(R, A, P, S);
                  });
  RegisterHandler(TEXT("create_niagara_ribbon"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleCreateNiagaraRibbon(R, A, P, S);
                  });
  RegisterHandler(TEXT("create_niagara_emitter"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleCreateNiagaraEmitter(R, A, P, S);
                  });
  RegisterHandler(TEXT("spawn_niagara_actor"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSpawnNiagaraActor(R, A, P, S);
                  });
  RegisterHandler(TEXT("modify_niagara_parameter"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleModifyNiagaraParameter(R, A, P, S);
                  });
  RegisterHandler(TEXT("manage_niagara_authoring"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageNiagaraAuthoringAction(R, A, P, S);
                  });

  // Animation
  RegisterHandler(TEXT("create_anim_blueprint"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleCreateAnimBlueprint(R, A, P, S);
                  });
  RegisterHandler(TEXT("play_anim_montage"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandlePlayAnimMontage(R, A, P, S);
                  });
  RegisterHandler(TEXT("setup_ragdoll"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSetupRagdoll(R, A, P, S);
                  });
  RegisterHandler(TEXT("activate_ragdoll"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleActivateRagdoll(R, A, P, S);
                  });

  // Material Graph
  RegisterHandler(TEXT("add_material_texture_sample"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAddMaterialTextureSample(R, A, P, S);
                  });
  RegisterHandler(TEXT("add_material_expression"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAddMaterialExpression(R, A, P, S);
                  });
  RegisterHandler(TEXT("create_material_nodes"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleCreateMaterialNodes(R, A, P, S);
                  });

  // Sequencer
  RegisterHandler(TEXT("add_sequencer_keyframe"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAddSequencerKeyframe(R, A, P, S);
                  });
  RegisterHandler(TEXT("manage_sequencer_track"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageSequencerTrack(R, A, P, S);
                  });
  RegisterHandler(TEXT("add_camera_track"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAddCameraTrack(R, A, P, S);
                  });
  RegisterHandler(TEXT("add_animation_track"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAddAnimationTrack(R, A, P, S);
                  });
  RegisterHandler(TEXT("add_transform_track"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAddTransformTrack(R, A, P, S);
                  });

  // UI & Environment
  RegisterHandler(TEXT("manage_ui"), [this](const FString &R, const FString &A,
                                            const TSharedPtr<FJsonObject> &P,
                                            TSharedPtr<FMcpBridgeWebSocket> S) {
    return HandleUiAction(R, A, P, S);
  });
  RegisterHandler(TEXT("control_environment"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleControlEnvironmentAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("build_environment"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleBuildEnvironmentAction(R, A, P, S);
                  });

  // Tools & System
  RegisterHandler(TEXT("console_command"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleConsoleCommandAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("batch_console_commands"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleConsoleCommandAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("inspect"), [this](const FString &R, const FString &A,
                                          const TSharedPtr<FJsonObject> &P,
                                          TSharedPtr<FMcpBridgeWebSocket> S) {
    return HandleInspectAction(R, A, P, S);
  });
  RegisterHandler(TEXT("system_control"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSystemControlAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("list_blueprints"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleListBlueprints(R, A, P, S);
                  });
  RegisterHandler(TEXT("manage_world_partition"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleWorldPartitionAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("manage_render"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleRenderAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_input"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleInputAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("control_actor"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleControlActorAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_level"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleLevelAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_sequence"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSequenceAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_asset"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAssetAction(R, A, P, S);
                  });

  // CRITICAL: Register asset_query for O(1) dispatch - fixes timeout issues
  // This handler processes search_assets, find_by_tag, get_source_control_state, etc.
  RegisterHandler(TEXT("asset_query"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAssetQueryAction(R, A, P, S);
                  });

  // Direct action aliases for common asset_query subActions
  // These allow TS to call executeAutomationRequest('search_assets', {...}) directly
  RegisterHandler(TEXT("search_assets"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleSearchAssets(R, A, P, S);
                  });

  RegisterHandler(TEXT("find_by_tag"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleFindByTag(R, A, P, S);
                  });

  // Direct action aliases for manage_asset subActions that TS calls directly
  // These allow O(1) dispatch for GPU-heavy and common operations
  RegisterHandler(TEXT("generate_lods"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleGenerateLODs(R, A, P, S);
                  });

  RegisterHandler(TEXT("create_thumbnail"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleGenerateThumbnail(R, A, P, S);
                  });

  RegisterHandler(TEXT("get_source_control_state"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleGetSourceControlState(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_material_authoring"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageMaterialAuthoringAction(R, A, P, S);
                  });

  // === Consolidated and legacy action namespace registrations ===
  RegisterHandler(TEXT("manage_blueprint"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    FString SubAction;
                    if (P.IsValid()) {
                      P->TryGetStringField(TEXT("subAction"), SubAction);
                    }
                    static const TSet<FString> GraphSubActions = {
                        TEXT("create_node"),       TEXT("delete_node"),
                        TEXT("connect_pins"),      TEXT("break_pin_links"),
                        TEXT("set_node_property"), TEXT("create_reroute_node"),
                        TEXT("get_node_details"),  TEXT("get_graph_details"),
                        TEXT("get_pin_details"),   TEXT("list_node_types"),
                        TEXT("set_pin_default_value")};
                    if (GraphSubActions.Contains(SubAction)) {
                      return HandleBlueprintGraphAction(R, A, P, S);
                    }
                    return HandleBlueprintAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_geometry"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleGeometryAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_skeleton"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageSkeleton(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_texture"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageTextureAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_gas"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageGASAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_character"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageCharacterAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_combat"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageCombatAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_ai"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageAIAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_inventory"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageInventoryAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_interaction"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageInteractionAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_widget_authoring"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageWidgetAuthoringAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_networking"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageNetworkingAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_splines"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageSplinesAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_pipeline"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandlePipelineAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_behavior_tree"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleBehaviorTreeAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_audio"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAudioAction(R, A, P, S);
                  });

  // Audio authoring - uses subAction field (different from manage_audio which uses action)
  RegisterHandler(TEXT("manage_audio_authoring"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageAudioAuthoringAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_lighting"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleLightingAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_physics"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAnimationPhysicsAction(R, A, P, S);
                  });

  // Animation physics alias - TS uses 'animation_physics' as the tool name
  RegisterHandler(TEXT("animation_physics"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleAnimationPhysicsAction(R, A, P, S);
                  });

  // Animation authoring - uses subAction field (different from animation_physics which uses action)
  RegisterHandler(TEXT("manage_animation_authoring"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageAnimationAuthoringAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_effect"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleEffectAction(R, A, P, S);
                  });

  // Common effect aliases used by the Node server; registering them here keeps
  // dispatch O(1) and avoids relying on the late handler chain.
  RegisterHandler(TEXT("create_effect"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleEffectAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("clear_debug_shapes"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleEffectAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_performance"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandlePerformanceAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("enable_gpu_timing"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandlePerformanceAction(R, A, P, S);
                  });

  RegisterHandler(TEXT("manage_debug"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleDebugAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("spawn_category"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleDebugAction(R, A, P, S);
                  });

  // Phase 21: Game Framework
  RegisterHandler(TEXT("manage_game_framework"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageGameFrameworkAction(R, A, P, S);
                  });

  // Phase 22: Sessions & Local Multiplayer
  RegisterHandler(TEXT("manage_sessions"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageSessionsAction(R, A, P, S);
                  });

  // Phase 23: Level Structure
  RegisterHandler(TEXT("manage_level_structure"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageLevelStructureAction(R, A, P, S);
                  });

  // Phase 24: Volumes & Zones
  RegisterHandler(TEXT("manage_volumes"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageVolumesAction(R, A, P, S);
                  });

  // Phase 25: Navigation System
  RegisterHandler(TEXT("manage_navigation"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleManageNavigationAction(R, A, P, S);
                  });

  // Phase 27: Misc (camera, viewport, bookmarks, post-process, networking helpers)
  RegisterHandler(TEXT("manage_misc"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMiscAction(R, A, P, S);
                  });

  // Direct action aliases for misc handlers
  // Note: create_post_process_volume is handled via manage_volumes tool
  RegisterHandler(TEXT("create_camera"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMiscAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("set_camera_fov"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMiscAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("set_viewport_resolution"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMiscAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("set_game_speed"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMiscAction(R, A, P, S);
                  });
  RegisterHandler(TEXT("create_bookmark"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
                    return HandleMiscAction(R, A, P, S);
                  });

  // PIE State Handler - for checking Play-In-Editor state
  RegisterHandler(TEXT("check_pie_state"),
                  [this](const FString &R, const FString &A,
                         const TSharedPtr<FJsonObject> &P,
                         TSharedPtr<FMcpBridgeWebSocket> S) {
#if WITH_EDITOR
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    bool bIsInPIE = false;
                    FString PieState = TEXT("stopped");
                    
                    if (GEditor && GEditor->PlayWorld) {
                      bIsInPIE = true;
                      if (GEditor->PlayWorld->IsPaused()) {
                        PieState = TEXT("paused");
                      } else {
                        PieState = TEXT("playing");
                      }
                    }
                    
                    Result->SetBoolField(TEXT("isInPIE"), bIsInPIE);
                    Result->SetStringField(TEXT("pieState"), PieState);
                    
                    SendAutomationResponse(S, R, true, 
                        bIsInPIE ? TEXT("PIE is active") : TEXT("PIE is not active"),
                        Result);
                    return true;
#else
                    SendAutomationError(S, R, TEXT("PIE state check requires editor build"), TEXT("NOT_AVAILABLE"));
                    return true;
#endif
                  });
}

// Drain and process any automation requests that were enqueued while the
// subsystem was busy. This implementation lives in the primary subsystem
// translation unit to ensure the symbol is available at link time for
/**
 * @brief Processes all queued automation requests on the game thread.
 *
 * Ensures execution on the game thread (re-dispatches if called from another
 * thread), moves the shared pending-request queue into a local list under a
 * lock, clears the shared queue and the scheduled flag, then dispatches each
 * request to ProcessAutomationRequest.
 */
void UMcpAutomationBridgeSubsystem::ProcessPendingAutomationRequests() {
  if (!IsInGameThread()) {
    AsyncTask(ENamedThreads::GameThread,
              [this]() { this->ProcessPendingAutomationRequests(); });
    return;
  }

  TArray<FPendingAutomationRequest> LocalQueue;
  {
    FScopeLock Lock(&PendingAutomationRequestsMutex);
    if (PendingAutomationRequests.Num() == 0) {
      return;
    }
    LocalQueue = MoveTemp(PendingAutomationRequests);
    PendingAutomationRequests.Empty();
  }

  for (const FPendingAutomationRequest &Req : LocalQueue) {
    ProcessAutomationRequest(Req.RequestId, Req.Action, Req.Payload,
                             Req.RequestingSocket, Req.Origin);
  }
}

// ============================================================================
// ExecuteEditorCommands Implementation
// ============================================================================
/**
 * @brief Executes a list of editor console commands sequentially.
 *
 * Uses GEditor->Exec() to execute each command in the provided array.
 * Stops on first failure and returns the error message.
 *
 * @param Commands Array of console command strings to execute.
 * @param OutErrorMessage Error message if any command fails.
 * @return true if all commands executed successfully, false otherwise.
 */
bool UMcpAutomationBridgeSubsystem::ExecuteEditorCommands(
    const TArray<FString> &Commands, FString &OutErrorMessage) {
#if WITH_EDITOR
  // GEditor operations must run on the game thread
  check(IsInGameThread());
  
  if (!GEditor) {
    OutErrorMessage = TEXT("Editor not available");
    return false;
  }

  UWorld *EditorWorld = GEditor->GetEditorWorldContext().World();
  if (!EditorWorld) {
    OutErrorMessage = TEXT("Editor world context not available");
    return false;
  }

  for (const FString &Command : Commands) {
    const FString TrimmedCommand = Command.TrimStartAndEnd();
    if (TrimmedCommand.IsEmpty()) {
      continue;
    }

    if (McpContainsUnsafeCommandSeparator(TrimmedCommand)) {
      OutErrorMessage = FString::Printf(
          TEXT("Rejected unsafe editor command: %s"), *TrimmedCommand);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("ExecuteEditorCommands: %s"), *OutErrorMessage);
      return false;
    }

    // Execute the command via GEditor
    // Note: GEditor->Exec returns true if the command was handled
    if (!GEditor->Exec(EditorWorld, *TrimmedCommand)) {
      OutErrorMessage =
          FString::Printf(TEXT("Failed to execute command: %s"), *TrimmedCommand);
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("ExecuteEditorCommands: %s"), *OutErrorMessage);
      return false;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("ExecuteEditorCommands: Executed '%s'"), *TrimmedCommand);
  }

  return true;
#else
  OutErrorMessage = TEXT("Editor commands only available in editor builds");
  return false;
#endif
}

// ============================================================================
// CreateControlRigBlueprint Implementation
// ============================================================================
// Note: ControlRigBlueprintFactory is only available in UE 5.1+ or as private API
// The MCP_HAS_CONTROLRIG_FACTORY macro is defined in McpAutomationBridgeHelpers.h

#if MCP_HAS_CONTROLRIG_FACTORY
// Animation/Skeleton types needed for ControlRig blueprint creation
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

// ControlRig includes - these are only available when ControlRig plugin is enabled
// UE 5.7 renamed ControlRigBlueprint.h to ControlRigBlueprintLegacy.h
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
#include "ControlRigBlueprintLegacy.h"
#else
#include "ControlRigBlueprint.h"
#endif
// Include the generated class header for UControlRigBlueprintGeneratedClass
#include "ControlRigBlueprintGeneratedClass.h"
// Note: ControlRigBlueprintFactory header is Public only in UE 5.5+
// For UE 5.1-5.4 we use a fallback implementation with FKismetEditorUtilities
#if MCP_HAS_CONTROLRIG_FACTORY && ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
  #include "ControlRigBlueprintFactory.h"
#endif
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"  // For DoesAssetExist, LoadAsset in crash prevention checks
#endif

#if MCP_HAS_CONTROLRIG_FACTORY
/**
 * @brief Creates a new Control Rig Blueprint asset.
 *
 * Uses UControlRigBlueprintFactory to create the asset at the specified
 * location with the given skeleton as the target.
 *
 * @param AssetName Name for the new Control Rig Blueprint.
 * @param PackagePath Package path where the asset should be created (e.g.,
 * "/Game/Rigs").
 * @param TargetSkeleton Skeleton to associate with the Control Rig.
 * @param OutError Error message if creation fails.
 * @return Pointer to the created UBlueprint, or nullptr on failure.
 */
UBlueprint *UMcpAutomationBridgeSubsystem::CreateControlRigBlueprint(
    const FString &AssetName, const FString &PackagePath,
    USkeleton *TargetSkeleton, FString &OutError) {
#if WITH_EDITOR
  if (AssetName.IsEmpty()) {
    OutError = TEXT("Asset name cannot be empty");
    return nullptr;
  }

  if (PackagePath.IsEmpty()) {
    OutError = TEXT("Package path cannot be empty");
    return nullptr;
  }

  // Normalize the package path
  FString NormalizedPath = PackagePath;
  NormalizedPath.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
  NormalizedPath.ReplaceInline(TEXT("\\"), TEXT("/"));

  // Ensure path starts with /Game
  if (!NormalizedPath.StartsWith(TEXT("/Game"))) {
    NormalizedPath = TEXT("/Game") / NormalizedPath;
  }

  // Remove trailing slashes
  while (NormalizedPath.EndsWith(TEXT("/"))) {
    NormalizedPath.LeftChopInline(1);
  }

  // Build full package name
  FString FullPackageName = NormalizedPath / AssetName;

  // ============================================================================
  // CRASH PREVENTION: Pre-check for existing assets
  // ============================================================================
  // UE crashes when:
  // 1. Creating a blueprint with a name that already exists (assertion)
  // 2. Creating an object where a different class exists at that path (fatal)
  // We must check BEFORE calling CreatePackage/CreateBlueprint.

  // Check 1: Does a saved asset exist at this path?
  FString FullObjectPath = FullPackageName + TEXT(".") + AssetName;
  if (UEditorAssetLibrary::DoesAssetExist(FullObjectPath)) {
    UObject *ExistingAsset = UEditorAssetLibrary::LoadAsset(FullObjectPath);
    if (ExistingAsset) {
      // Check if it's a ControlRigBlueprint - safe to reuse
      if (ExistingAsset->IsA<UControlRigBlueprint>()) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Control Rig Blueprint already exists, reusing: %s"),
               *FullObjectPath);
        // Return existing asset - operation is idempotent
        return Cast<UBlueprint>(ExistingAsset);
      } else {
        // Different type at same path - would cause fatal crash
        OutError = FString::Printf(
            TEXT("Asset exists at path but is not a ControlRigBlueprint (is %s). "
                 "Cannot create ControlRigBlueprint at this path."),
            *ExistingAsset->GetClass()->GetName());
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"),
               *OutError);
        return nullptr;
      }
    }
  }

  // Check 2: Is there an in-memory object at this path? (unsaved blueprint)
  // This catches objects that exist in memory but aren't saved to disk yet
  UPackage *ExistingPackage = FindPackage(nullptr, *FullPackageName);
  if (ExistingPackage) {
    UObject *ExistingObject = FindObject<UObject>(ExistingPackage, *AssetName);
    if (ExistingObject) {
      if (ExistingObject->IsA<UControlRigBlueprint>()) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("Control Rig Blueprint already exists in memory, reusing: %s"),
               *FullObjectPath);
        return Cast<UBlueprint>(ExistingObject);
      } else {
        OutError = FString::Printf(
            TEXT("In-memory object exists at path but is not a ControlRigBlueprint (is %s). "
                 "Cannot create ControlRigBlueprint at this path."),
            *ExistingObject->GetClass()->GetName());
        UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("%s"),
               *OutError);
        return nullptr;
      }
    }
  }

  // Create the package
  UPackage *Package = CreatePackage(*FullPackageName);
  if (!Package) {
    OutError =
        FString::Printf(TEXT("Failed to create package: %s"), *FullPackageName);
    return nullptr;
  }

  Package->FullyLoad();

  // Create the Control Rig Blueprint using FKismetEditorUtilities
  // This works across all UE versions without needing ControlRigBlueprintFactory
  // Note: Use UControlRigBlueprintGeneratedClass instead of URigVMBlueprintGeneratedClass
  // to avoid needing to include RigVM module headers
  UControlRigBlueprint *NewBlueprint = Cast<UControlRigBlueprint>(
      FKismetEditorUtilities::CreateBlueprint(
          UControlRig::StaticClass(),  // Parent class
          Package,                      // Outer
          *AssetName,                   // Name
          BPTYPE_Normal,                // Blueprint type
          UControlRigBlueprint::StaticClass(),  // Blueprint class
          UControlRigBlueprintGeneratedClass::StaticClass(),  // Generated class
          NAME_None));

  if (!NewBlueprint) {
    OutError = TEXT("Factory failed to create Control Rig Blueprint");
    return nullptr;
  }

  // Set the target skeleton if provided
  if (TargetSkeleton) {
    // UControlRigBlueprint uses a preview skeletal mesh, not skeleton directly
    // Try to find a skeletal mesh that uses this skeleton
    USkeletalMesh *PreviewMesh = TargetSkeleton->GetPreviewMesh();
    if (PreviewMesh) {
      NewBlueprint->SetPreviewMesh(PreviewMesh);
    }
  }

  // Notify asset registry
  FAssetRegistryModule::AssetCreated(NewBlueprint);

  // Mark package dirty for save
  NewBlueprint->MarkPackageDirty();

  // Use safe asset save (UE 5.7 compatible)
  McpSafeAssetSave(NewBlueprint);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("Created Control Rig Blueprint: %s"), *FullPackageName);

  return NewBlueprint;
#else
  OutError = TEXT("Control Rig creation only available in editor builds");
  return nullptr;
#endif // WITH_EDITOR
}
#endif // MCP_HAS_CONTROLRIG_FACTORY
