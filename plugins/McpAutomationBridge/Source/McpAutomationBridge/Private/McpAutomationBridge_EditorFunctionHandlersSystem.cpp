#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_EditorFunctionHandlersShared.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Misc/OutputDeviceNull.h"
#include "Subsystems/EngineSubsystem.h"

namespace McpEditorFunctionHandlers {

bool HandleSystemFunctions(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &FN, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (FN == TEXT("GENERATE_MEMORY_REPORT")) {
    FString OutputPath;
    Payload->TryGetStringField(TEXT("outputPath"), OutputPath);
    bool bDetailed = false;
    Payload->TryGetBoolField(TEXT("detailed"), bDetailed);

    if (!GEditor) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Editor not available"), nullptr,
                                    TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    FString MemReportCmd =
        bDetailed ? TEXT("memreport -full") : TEXT("memreport");
    GEditor->Exec(nullptr, *MemReportCmd);

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetBoolField(TEXT("success"), true);
    Out->SetStringField(
        TEXT("message"),
        TEXT("Memory report generated (check Saved/Profiling/MemReports)"));

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Memory report generated"), Out,
                                  FString());
    return true;
  }

  if (FN == TEXT("CALL_SUBSYSTEM")) {
    FString SubsystemName;
    Payload->TryGetStringField(TEXT("subsystem"), SubsystemName);
    FString TargetFuncName;
    Payload->TryGetStringField(TEXT("function"), TargetFuncName);
    const TSharedPtr<FJsonObject> *Args = nullptr;
    Payload->TryGetObjectField(TEXT("args"), Args);

    if (SubsystemName.IsEmpty() || TargetFuncName.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                                 TEXT("subsystem and function required"),
                                 TEXT("INVALID_ARGUMENT"));
      return true;
    }

    if (!GEditor) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Editor not available"), nullptr,
                                    TEXT("EDITOR_NOT_AVAILABLE"));
      return true;
    }

    UObject *TargetSubsystem = nullptr;
    if (!TargetSubsystem) {
      UClass *SubsystemClass = ResolveClassByName(SubsystemName);
      if (SubsystemClass) {
        if (SubsystemClass->IsChildOf(UEditorSubsystem::StaticClass())) {
          TargetSubsystem = GEditor->GetEditorSubsystemBase(SubsystemClass);
        } else if (SubsystemClass->IsChildOf(UEngineSubsystem::StaticClass())) {
          TargetSubsystem = GEngine->GetEngineSubsystemBase(SubsystemClass);
        }
      }
    }

    if (!TargetSubsystem) {
      TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
      Err->SetStringField(
          TEXT("error"),
          FString::Printf(TEXT("Subsystem '%s' not found or not initialized"),
                          *SubsystemName));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                    TEXT("Subsystem not found"), Err,
                                    TEXT("SUBSYSTEM_NOT_FOUND"));
      return true;
    }

    FString CmdString = TargetFuncName;
    if (Args && (*Args).IsValid()) {
      for (const auto &Pair : (*Args)->Values) {
        CmdString += TEXT(" ");
        CmdString += Pair.Key;
        CmdString += TEXT("=");

        switch (Pair.Value->Type) {
        case EJson::String:
          CmdString += FString::Printf(TEXT("\"%s\""), *Pair.Value->AsString());
          break;
        case EJson::Number:
          CmdString += FString::Printf(TEXT("%f"), Pair.Value->AsNumber());
          break;
        case EJson::Boolean:
          CmdString += Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
          break;
        default:
          break;
        }
      }
    }

    FOutputDeviceNull Ar;
    bool bResult = TargetSubsystem->CallFunctionByNameWithArguments(
        *CmdString, Ar, nullptr, true);

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetBoolField(TEXT("success"), bResult);
    Out->SetStringField(TEXT("subsystem"), SubsystemName);
    Out->SetStringField(TEXT("function"), TargetFuncName);

    Bridge.SendAutomationResponse(
        RequestingSocket, RequestId, bResult,
        bResult ? TEXT("Function called") : TEXT("Function call failed"), Out,
        bResult ? FString() : TEXT("CALL_FAILED"));
    return true;
  }

  if (FN == TEXT("CONFIGURE_TEXTURE_STREAMING")) {
    bool bEnabled = true;
    if (Payload->HasField(TEXT("enabled")))
      Payload->TryGetBoolField(TEXT("enabled"), bEnabled);

    double PoolSize = -1;
    if (Payload->HasField(TEXT("poolSize")))
      Payload->TryGetNumberField(TEXT("poolSize"), PoolSize);

    bool bBoost = false;
    if (Payload->HasField(TEXT("boostPlayerLocation")))
      Payload->TryGetBoolField(TEXT("boostPlayerLocation"), bBoost);

    if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
            TEXT("r.TextureStreaming"))) {
      CVar->Set(bEnabled ? 1 : 0, ECVF_SetByCode);
    }

    if (PoolSize >= 0) {
      if (IConsoleVariable *CVar = IConsoleManager::Get().FindConsoleVariable(
              TEXT("r.Streaming.PoolSize"))) {
        CVar->Set((int32)PoolSize, ECVF_SetByCode);
      }
    }

    TSharedPtr<FJsonObject> Out = McpHandlerUtils::CreateResultObject();
    Out->SetBoolField(TEXT("success"), true);
    Out->SetBoolField(TEXT("enabled"), bEnabled);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                                  TEXT("Texture streaming configured"), Out,
                                  FString());
    return true;
  }

  return false;
}

}
#endif
