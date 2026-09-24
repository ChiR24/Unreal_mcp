#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Safety/McpSafeOperationsOpenEditorGuard.h"

#if WITH_EDITOR
namespace
{
// The scalar setter echoes the number it wrote; the vector one reported only the
// parameter name, so a caller had no read-back of what actually landed. Built
// from the stored field, not from the request.
TSharedPtr<FJsonObject> ColorValueObject(const FLinearColor& Stored)
{
  TSharedPtr<FJsonObject> Value = MakeShared<FJsonObject>();
  Value->SetNumberField(TEXT("r"), Stored.R);
  Value->SetNumberField(TEXT("g"), Stored.G);
  Value->SetNumberField(TEXT("b"), Stored.B);
  Value->SetNumberField(TEXT("a"), Stored.A);
  return Value;
}
}
namespace McpMaterialAuthoringHandlers
{
bool HandleSetVectorParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_vector_parameter_value")) {
    FString AssetPath, ParamName;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

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

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      // Fallback: a BASE UMaterial. Mirrors set_scalar_parameter_value, which has always
      // accepted a base material here. Without this the two setters disagree about what a
      // "parameter value" means, and a master material's vector defaults are unreachable.
      if (UMaterial *BaseMaterial = LoadObject<UMaterial>(nullptr, *AssetPath)) {
        // The material editor edits a preview duplicate and writes it back over
        // the original on close, so a write taken now is silently reverted the
        // moment the tab closes. Refuse instead of reporting a success the
        // caller only discovers was a lie much later.
        if (McpSafeOperations::IsAssetEditorOpen(BaseMaterial)) {
          Bridge->SendAutomationError(
              Socket, RequestId,
              McpSafeOperations::OpenAssetEditorRefusal(BaseMaterial),
              TEXT("ASSET_EDITOR_OPEN"));
          return true;
        }
        UMaterialExpressionVectorParameter *Param = nullptr;
        TArray<FString> AvailableParams;
        const FName ParamFName(*ParamName);
        // Duplicate-named parameters are a graph authoring error; last match wins, matching
        // the ambiguity UE itself has and the behaviour of the scalar handler.
        for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(BaseMaterial)) {
          if (UMaterialExpressionVectorParameter *Vector =
                  Cast<UMaterialExpressionVectorParameter>(Expr)) {
            AvailableParams.Add(Vector->ParameterName.ToString());
            if (Vector->ParameterName == ParamFName) {
              Param = Vector;
            }
          }
        }
        if (!Param) {
          Bridge->SendAutomationError(Socket, RequestId,
                              FString::Printf(TEXT("Vector parameter '%s' not found on base material. Available: [%s]"),
                                              *ParamName, *FString::Join(AvailableParams, TEXT(", "))),
                              TEXT("PARAMETER_NOT_FOUND"));
          return true;
        }
        Param->DefaultValue = Color;
        BaseMaterial->PostEditChange();
        BaseMaterial->MarkPackageDirty();

        bool bSaveBase = true;
        Payload->TryGetBoolField(TEXT("save"), bSaveBase);
        if (bSaveBase) {
          SaveMaterialAsset(BaseMaterial);
        }

        TSharedPtr<FJsonObject> BaseResult = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(BaseResult, BaseMaterial);
        BaseResult->SetStringField(TEXT("parameterName"), ParamName);
        BaseResult->SetObjectField(TEXT("value"), ColorValueObject(Param->DefaultValue));
        BaseResult->SetStringField(TEXT("note"), TEXT("Base material (not an instance): the VectorParameter expression's DefaultValue was updated."));
        Bridge->SendAutomationResponse(
            Socket, RequestId, true,
            FString::Printf(TEXT("Vector parameter '%s' default set on base material."), *ParamName),
            BaseResult);
        return true;
      }
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load a material instance or material at this path."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Checked like the texture setter: a name the parent does not publish was
    // stored as a dead override and reported as "Vector parameter set".
    TArray<FMaterialParameterInfo> VectorInfos;
    TArray<FGuid> VectorGuids;
    Instance->GetAllVectorParameterInfo(VectorInfos, VectorGuids);
    TArray<FString> InstanceParams;
    for (const FMaterialParameterInfo &Info : VectorInfos) {
      InstanceParams.Add(Info.Name.ToString());
    }
    if (!InstanceParams.Contains(ParamName)) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Vector parameter '%s' not found on this material instance. Available: [%s]"),
                                          *ParamName, *FString::Join(InstanceParams, TEXT(", "))),
                          TEXT("PARAMETER_NOT_FOUND"));
      return true;
    }
    Instance->SetVectorParameterValueEditorOnly(FName(*ParamName), Color);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(Instance);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    FLinearColor Stored = Color;
    for (const FVectorParameterValue& Entry : Instance->VectorParameterValues) {
      if (Entry.ParameterInfo.Name == FName(*ParamName)) {
        Stored = Entry.ParameterValue;
      }
    }
    Result->SetObjectField(TEXT("value"), ColorValueObject(Stored));
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Vector parameter '%s' set."), *ParamName), Result);
    return true;
  }

  return false;
}
}
#endif
