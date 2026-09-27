#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "HAL/PlatformProcess.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#include "Editor/UnrealEd/Public/Editor.h"

namespace McpSystemControlHandlers {

bool HandleRunTests(UMcpAutomationBridgeSubsystem* Self,
                    const FString& RequestId,
                    const TSharedPtr<FJsonObject>& Payload,
                    FSystemControlSocket RequestingSocket) {
  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);

  FString TestName;
  Payload->TryGetStringField(TEXT("test"), TestName);

  if (!TestName.IsEmpty() && Filter.IsEmpty()) {
    Filter = TestName;
  }
  Filter.TrimStartAndEndInline();
  if (!McpIsSafeAutomationTestFilter(Filter)) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Test filter contains unsafe characters"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString TestCommand;
  if (Filter.IsEmpty()) {
    TestCommand = TEXT("automation RunAll");
  } else {
    TestCommand = FString::Printf(TEXT("automation RunTests %s"), *Filter);
  }

  if (GEngine && GEditor && GEditor->GetEditorWorldContext().World()) {
    GEngine->Exec(GEditor->GetEditorWorldContext().World(), *TestCommand);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("command"), TestCommand);
    Result->SetStringField(TEXT("filter"), Filter);

    Self->SendAutomationResponse(
        RequestingSocket, RequestId, true,
        TEXT("Automation tests started. Check Output Log for results."),
        Result);
  } else {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Editor world not available for running tests"),
        TEXT("EDITOR_NOT_AVAILABLE"));
  }
  return true;
}
}
