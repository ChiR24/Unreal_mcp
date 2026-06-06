#pragma once

#include "McpConnectionManager.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "McpAutomationBridgeSettings.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

inline FString SanitizeForLogConnMgr(const FString& In) {
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

inline bool IsImagePayloadPreviewField(const FString& Key) {
  return Key.Equals(TEXT("imageBase64"), ESearchCase::IgnoreCase) ||
         Key.Equals(TEXT("imageData"), ESearchCase::IgnoreCase) ||
         Key.Equals(TEXT("data"), ESearchCase::IgnoreCase);
}
