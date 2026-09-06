#pragma once

#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"

namespace McpMaterialAuthoringHandlers
{
// Appends the `inputs` / `outputs` arrays describing every FunctionInput and
// FunctionOutput expression in Expressions (a material function's node list).
template <typename TExpressions>
inline void AppendMaterialFunctionIO(const TSharedPtr<FJsonObject>& Result, const TExpressions& Expressions)
{
  TArray<TSharedPtr<FJsonValue>> InputsArray;
  TArray<TSharedPtr<FJsonValue>> OutputsArray;
  for (UMaterialExpression *Expr : Expressions) {
    if (!Expr) continue;
    if (UMaterialExpressionFunctionInput *In = Cast<UMaterialExpressionFunctionInput>(Expr)) {
      TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
      Obj->SetStringField(TEXT("name"), In->InputName.ToString());
      Obj->SetStringField(TEXT("type"), FunctionInputTypeToString(In->InputType));
      Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(In));
      Obj->SetBoolField(TEXT("usePreviewValueAsDefault"), In->bUsePreviewValueAsDefault);
      Obj->SetNumberField(TEXT("sortPriority"), In->SortPriority);
      Obj->SetStringField(TEXT("description"), In->Description);
      const auto PV = In->PreviewValue;
      TSharedPtr<FJsonObject> PreviewObj = MakeShared<FJsonObject>();
      PreviewObj->SetNumberField(TEXT("x"), PV.X);
      PreviewObj->SetNumberField(TEXT("y"), PV.Y);
      PreviewObj->SetNumberField(TEXT("z"), PV.Z);
      PreviewObj->SetNumberField(TEXT("w"), PV.W);
      Obj->SetObjectField(TEXT("previewValue"), PreviewObj);
      InputsArray.Add(MakeShared<FJsonValueObject>(Obj));
    } else if (UMaterialExpressionFunctionOutput *Out = Cast<UMaterialExpressionFunctionOutput>(Expr)) {
      TSharedPtr<FJsonObject> Obj = McpHandlerUtils::CreateResultObject();
      Obj->SetStringField(TEXT("name"), Out->OutputName.ToString());
      Obj->SetStringField(TEXT("nodeId"), MCP_NODE_ID(Out));
      Obj->SetNumberField(TEXT("sortPriority"), Out->SortPriority);
      Obj->SetStringField(TEXT("description"), Out->Description);
      OutputsArray.Add(MakeShared<FJsonValueObject>(Obj));
    }
  }
  Result->SetArrayField(TEXT("inputs"), InputsArray);
  Result->SetArrayField(TEXT("outputs"), OutputsArray);
}
} // namespace McpMaterialAuthoringHandlers
#endif
