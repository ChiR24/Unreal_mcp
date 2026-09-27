#include "Domains/MaterialAuthoring/Parameters/McpAutomationBridge_MaterialAuthoringParameterValue.h"

namespace
{
// The scalar setter echoes the number it wrote; the vector one reported only the
// parameter name, so a caller had no read-back of what actually landed. Built
// from the stored field, not from the request.
TSharedPtr<FJsonObject> ColorValueObject(const FLinearColor& Stored)
{
  return McpHandlerUtils::LinearColorToJson(Stored);
}
}
namespace McpMaterialAuthoringHandlers
{
bool HandleSetVectorParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction != TEXT("set_vector_parameter_value")) {
    return false;
  }
  // An array `value` used to miss the object-only check and leave Color at
  // white, so the call wrote white over the caller's colour and reported
  // success. add_vector_parameter already accepted both spellings; this
  // setter now matches it, and a `value` that is present but unreadable is
  // refused rather than silently becoming white.
  FLinearColor Color(1.0f, 1.0f, 1.0f, 1.0f);
  const TSharedPtr<FJsonObject> *ValueObj;
  const TArray<TSharedPtr<FJsonValue>> *ValueArr;
  if (Payload->TryGetObjectField(TEXT("value"), ValueObj)) {
    double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
    // Accept r/g/b/a and x/y/z/w, as the add path does.
    if (!(*ValueObj)->TryGetNumberField(TEXT("r"), R) &&
        !(*ValueObj)->TryGetNumberField(TEXT("x"), R)) {
      // An object carrying neither spelling used to fall through and write
      // white while reporting success.
      Bridge->SendAutomationError(
          Socket, RequestId,
          TEXT("'value' object needs r/g/b(/a) or x/y/z(/w) components; neither 'r' nor 'x' was given."),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    (*ValueObj)->TryGetNumberField(TEXT("g"), G);
    (*ValueObj)->TryGetNumberField(TEXT("y"), G);
    (*ValueObj)->TryGetNumberField(TEXT("b"), B);
    (*ValueObj)->TryGetNumberField(TEXT("a"), A);
    (*ValueObj)->TryGetNumberField(TEXT("z"), B);
    (*ValueObj)->TryGetNumberField(TEXT("w"), A);
    Color = FLinearColor(R, G, B, A);
  } else if (Payload->TryGetArrayField(TEXT("value"), ValueArr)) {
    if (ValueArr->Num() < 3) {
      Bridge->SendAutomationError(
          Socket, RequestId,
          FString::Printf(TEXT("'value' array needs at least 3 components (r, g, b); got %d."),
                          ValueArr->Num()),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    auto Num = [&](int32 Idx, double Def) {
      double V = Def;
      return (*ValueArr)[Idx]->TryGetNumber(V) ? V : Def;
    };
    Color = FLinearColor(Num(0, 1.0), Num(1, 1.0), Num(2, 1.0),
                         ValueArr->Num() > 3 ? Num(3, 1.0) : 1.0);
  } else if (Payload->HasField(TEXT("value"))) {
    Bridge->SendAutomationError(
        Socket, RequestId,
        TEXT("'value' must be an object {r,g,b,a} / {x,y,z,w} or an array [r,g,b(,a)]."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }


  FParameterValueWriter Writer;
  Writer.Kind = TEXT("Vector");
  Writer.Type = EMaterialParameterType::Vector;
  Writer.SetDefault = [Color](UMaterial* Material, FName Name, TArray<FString>& Available) {
    UMaterialExpressionVectorParameter* Param =
        FindParameterExpression<UMaterialExpressionVectorParameter>(Material, Name, Available);
    if (Param) { Param->DefaultValue = Color; }
    return Param != nullptr;
  };
  Writer.SetOverride = [Color](UMaterialInstanceConstant* Instance, FName Name) {
    Instance->SetVectorParameterValueEditorOnly(Name, Color);
  };
  // Read back what landed, not what was asked for.
  Writer.DescribeValue = [Color](UMaterialInterface* Material, FName Name, const TSharedPtr<FJsonObject>& Result) {
    FLinearColor Stored = Color;
    if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Material)) {
      for (const FVectorParameterValue& Entry : Instance->VectorParameterValues) {
        if (Entry.ParameterInfo.Name == Name) { Stored = Entry.ParameterValue; }
      }
    }
    Result->SetObjectField(TEXT("value"), ColorValueObject(Stored));
  };
  SetMaterialParameterValue(Bridge, RequestId, Payload, Socket, Writer);
  return true;
}
}
