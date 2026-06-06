#pragma once

#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "McpAutomationBridgeHelpersClassResolution.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR
inline bool ResolveAddVariablePinType(const FString &VarType,
                                      FEdGraphPinType &PinType,
                                      FString &OutError) {
  const FString LowerType = VarType.ToLower();
  if (LowerType == TEXT("float") || LowerType == TEXT("double")) {
    PinType.PinCategory = MCP_PC_Float;
  } else if (LowerType == TEXT("int") || LowerType == TEXT("integer")) {
    PinType.PinCategory = MCP_PC_Int;
  } else if (LowerType == TEXT("bool") || LowerType == TEXT("boolean")) {
    PinType.PinCategory = MCP_PC_Boolean;
  } else if (LowerType == TEXT("string")) {
    PinType.PinCategory = MCP_PC_String;
  } else if (LowerType == TEXT("name")) {
    PinType.PinCategory = MCP_PC_Name;
  } else if (LowerType == TEXT("text")) {
    PinType.PinCategory = MCP_PC_Text;
  } else if (LowerType == TEXT("vector")) {
    PinType.PinCategory = MCP_PC_Struct;
    PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
  } else if (LowerType == TEXT("rotator")) {
    PinType.PinCategory = MCP_PC_Struct;
    PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
  } else if (LowerType == TEXT("transform")) {
    PinType.PinCategory = MCP_PC_Struct;
    PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
  } else if (LowerType == TEXT("object")) {
    PinType.PinCategory = MCP_PC_Object;
    PinType.PinSubCategoryObject = UObject::StaticClass();
  } else if (LowerType == TEXT("class")) {
    PinType.PinCategory = MCP_PC_Class;
    PinType.PinSubCategoryObject = UObject::StaticClass();
  } else if (!VarType.TrimStartAndEnd().IsEmpty()) {
    PinType.PinCategory = MCP_PC_Object;
    UClass *FoundClass = ResolveUClass(VarType);
    if (!FoundClass) {
      OutError = FString::Printf(TEXT("Could not resolve class '%s'"), *VarType);
      return false;
    }
    PinType.PinSubCategoryObject = FoundClass;
  } else {
    PinType.PinCategory = MCP_PC_Wildcard;
  }
  return true;
}
#endif
}
