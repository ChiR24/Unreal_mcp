#pragma once

#include "McpConnectionManager.h"
#include "Foundation/McpSecureTokenCompare.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "McpAutomationBridgeSettings.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Core/Subsystem/McpAutomationBridgeSubsystemResponseSanitization.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

inline bool IsImagePayloadPreviewField(const FString& Key) {
  return Key.Equals(TEXT("imageBase64"), ESearchCase::IgnoreCase) ||
         Key.Equals(TEXT("imageData"), ESearchCase::IgnoreCase) ||
         Key.Equals(TEXT("data"), ESearchCase::IgnoreCase);
}
