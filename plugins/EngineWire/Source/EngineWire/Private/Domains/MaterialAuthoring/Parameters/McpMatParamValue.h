#pragma once

#include "Domains/MaterialAuthoring/McpMatPrivate.h"

namespace McpMaterialAuthoringHandlers
{
// How one set_<kind>_parameter_value writes its value.
struct FParameterValueWriter
{
  // "Scalar", "Vector", "Texture" or "Static switch", for messages.
  const TCHAR* Kind;
  // The instance must publish a parameter of this type under the name.
  EMaterialParameterType Type;
  // Base material: sets the default on the parameter expression named Name, or
  // returns false with OutAvailable listing the expressions of this kind.
  TFunction<bool(UMaterial*, FName, TArray<FString>& OutAvailable)> SetDefault;
  // Material instance: sets the override.
  TFunction<void(UMaterialInstanceConstant*, FName)> SetOverride;
  // Adds the value that landed to the reply.
  TFunction<void(UMaterialInterface*, FName, const TSharedPtr<FJsonObject>&)> DescribeValue;
};

// assetPath + parameterName (+ save): writes the value onto a material instance,
// or onto a base material's parameter expression default. Replies itself.
void SetMaterialParameterValue(UEngineWireSubsystem* Bridge, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                               const FParameterValueWriter& Writer);

// Set applied to every parameter expression of type T named Name in the base material: each same-named node
// compiles with its own default (the material editor keeps them equal), so a write to the last one left the others,
// and the parts of the graph reading them, on the old value. False when none has the name; OutAvailable lists every
// T's name.
template <typename TExpression, typename TSetter>
bool SetParameterExpressions(UMaterial* Material, const FName Name, TArray<FString>& OutAvailable, TSetter Set)
{
  bool bFound = false;
  for (UMaterialExpression* Expr : MCP_GET_MATERIAL_EXPRESSIONS(Material)) {
    if (TExpression* Param = Cast<TExpression>(Expr)) {
      OutAvailable.Add(Param->ParameterName.ToString());
      if (Param->ParameterName == Name) {
        Set(*Param);
        bFound = true;
      }
    }
  }
  return bFound;
}
}
