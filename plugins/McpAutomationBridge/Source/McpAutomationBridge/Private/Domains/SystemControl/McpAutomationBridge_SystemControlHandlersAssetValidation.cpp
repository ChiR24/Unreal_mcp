#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#include "EditorAssetLibrary.h"
#include "EditorValidatorSubsystem.h"

namespace McpSystemControlHandlers {
namespace {
TArray<TSharedPtr<FJsonValue>> McpTextValues(const TArray<FText>& Texts) {
  TArray<TSharedPtr<FJsonValue>> Out;
  for (const FText& Text : Texts) {
    Out.Add(MakeShared<FJsonValueString>(Text.ToString()));
  }
  return Out;
}

// Runs the project's and the engine's Data Validation on one loaded asset (what the editor's Validate Assets
// menu runs): its verdict, and its errors and warnings when it has any. Loading alone passed an asset whose
// validators reject it.
EDataValidationResult McpRunDataValidation(UEditorValidatorSubsystem* Validator, UObject* Asset,
                                           const TSharedPtr<FJsonObject>& Row) {
  TArray<FText> Errors, Warnings;
  const EDataValidationResult Verdict =
      Validator->IsAssetValid(FAssetData(Asset), Errors, Warnings, EDataValidationUsecase::Script);
  Row->SetStringField(TEXT("dataValidation"), Verdict == EDataValidationResult::Valid ? TEXT("valid")
                                              : Verdict == EDataValidationResult::Invalid ? TEXT("invalid")
                                                                                          : TEXT("notValidated"));
  if (Errors.Num() > 0) {
    Row->SetArrayField(TEXT("errors"), McpTextValues(Errors));
  }
  if (Warnings.Num() > 0) {
    Row->SetArrayField(TEXT("warnings"), McpTextValues(Warnings));
  }
  return Verdict;
}
} // namespace

bool HandleValidateAssets(UMcpAutomationBridgeSubsystem* Self,
                          const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload,
                          FSystemControlSocket RequestingSocket) {
  TArray<FString> PathsToValidate;

  const TArray<TSharedPtr<FJsonValue>>* PathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("paths"), PathsArray) && PathsArray) {
    for (const TSharedPtr<FJsonValue>& PathValue : *PathsArray) {
      if (PathValue.IsValid() && PathValue->Type == EJson::String) {
        FString Path = PathValue->AsString();
        Path.TrimStartAndEndInline();
        if (!Path.IsEmpty()) {
          PathsToValidate.Add(Path);
        }
      }
    }
  }

  // assetPath and path are both validated when both are given; path used to be dropped.
  for (const TCHAR* Field : {TEXT("assetPath"), TEXT("path")}) {
    FString SinglePath;
    if (Payload->TryGetStringField(Field, SinglePath)) {
      SinglePath.TrimStartAndEndInline();
      if (!SinglePath.IsEmpty()) {
        PathsToValidate.AddUnique(SinglePath);
      }
    }
  }

  if (PathsToValidate.IsEmpty()) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("validate_assets requires paths, assetPath, or path"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const bool bRecursive = Payload->HasField(TEXT("recursive"))
      ? GetJsonBoolField(Payload, TEXT("recursive"))
      : true;
  UEditorValidatorSubsystem* Validator = GetJsonBoolField(Payload, TEXT("dataValidation")) && GEditor
      ? GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>()
      : nullptr;
  TArray<TSharedPtr<FJsonValue>> Results;
  int32 InvalidCount = 0;

  auto AddValidationResult = [&](const FString& OriginalPath, bool bSuccess,
                                 const FString& Kind, const FString& Message,
                                 int32 AssetCount = INDEX_NONE) {
    TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("path"), OriginalPath);
    Item->SetBoolField(TEXT("isValid"), bSuccess);
    Item->SetStringField(TEXT("kind"), Kind);
    Item->SetStringField(TEXT("message"), Message);
    if (AssetCount != INDEX_NONE) {
      Item->SetNumberField(TEXT("assetCount"), AssetCount);
    }
    Results.Add(MakeShared<FJsonValueObject>(Item));
    InvalidCount += bSuccess ? 0 : 1;
    return Item;
  };
  // A row whose asset loaded but its validators reject turns invalid.
  auto FailRow = [&](const TSharedPtr<FJsonObject>& Item) {
    if (Item->GetBoolField(TEXT("isValid"))) {
      Item->SetBoolField(TEXT("isValid"), false);
      InvalidCount += 1;
    }
  };

  for (const FString& RawPath : PathsToValidate) {
    FString Path = RawPath;
    McpAssetPathCanonical::MapContentRootInline(Path);

    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (SafePath.IsEmpty()) {
      AddValidationResult(RawPath, false, TEXT("invalid"),
                          McpPathRefusalMessage(TEXT("asset path"), RawPath));
      continue;
    }

    if (McpAssetExists(SafePath)) {
      UObject* Asset = McpLoadAsset(SafePath);
      const TSharedPtr<FJsonObject> Row = AddValidationResult(
          SafePath, Asset != nullptr, TEXT("asset"),
          Asset ? TEXT("Asset loaded successfully")
                : TEXT("Asset exists but failed to load"));
      if (Asset && Validator && McpRunDataValidation(Validator, Asset, Row) == EDataValidationResult::Invalid) {
        FailRow(Row);
      }
      continue;
    }

    if (UEditorAssetLibrary::DoesDirectoryExist(SafePath)) {
      // A directory used to pass just for existing; every asset in it is now loaded like a single path.
      // ponytail: loads every listed asset in one request; batch it if whole-project scans time out.
      TArray<FString> Assets =
          UEditorAssetLibrary::ListAssets(SafePath, bRecursive, false);
      TArray<FString> Failed;
      TArray<TSharedPtr<FJsonValue>> Issues; // the assets the validators said something about, at most 20
      int32 Counts[3] = {0, 0, 0};          // invalid, valid, notValidated (EDataValidationResult order)
      for (const FString& AssetPath : Assets) {
        UObject* Asset = McpLoadAsset(AssetPath);
        if (!Asset) {
          Failed.Add(AssetPath);
        } else if (Validator) {
          TSharedPtr<FJsonObject> Issue = MakeShared<FJsonObject>();
          Issue->SetStringField(TEXT("asset"), AssetPath);
          const EDataValidationResult Verdict = McpRunDataValidation(Validator, Asset, Issue);
          Counts[FMath::Clamp(static_cast<int32>(Verdict), 0, 2)] += 1;
          if (Issues.Num() < 20 && (Issue->HasField(TEXT("errors")) || Issue->HasField(TEXT("warnings")) ||
                                    Verdict == EDataValidationResult::Invalid)) {
            Issues.Add(MakeShared<FJsonValueObject>(Issue));
          }
        }
      }
      TArray<FString> Shown(Failed.GetData(), FMath::Min(Failed.Num(), 20));
      const TSharedPtr<FJsonObject> Row =
          AddValidationResult(SafePath, Failed.Num() == 0, TEXT("directory"),
                              Failed.Num() == 0
                                  ? FString::Printf(TEXT("All %d asset(s) loaded successfully"), Assets.Num())
                                  : FString::Printf(TEXT("%d of %d asset(s) failed to load: %s"), Failed.Num(),
                                                    Assets.Num(), *FString::Join(Shown, TEXT(", "))),
                              Assets.Num());
      if (Validator) {
        TSharedPtr<FJsonObject> Verdicts = MakeShared<FJsonObject>();
        Verdicts->SetNumberField(TEXT("valid"), Counts[1]);
        Verdicts->SetNumberField(TEXT("invalid"), Counts[0]);
        Verdicts->SetNumberField(TEXT("notValidated"), Counts[2]);
        Row->SetObjectField(TEXT("dataValidation"), Verdicts);
        if (Issues.Num() > 0) {
          Row->SetArrayField(TEXT("issues"), Issues);
        }
        if (Counts[0] > 0) {
          FailRow(Row);
        }
      }
      continue;
    }

    AddValidationResult(SafePath, false, TEXT("missing"),
                        TEXT("Asset or directory not found"));
  }

  // An invalid asset is a finding, not a transport error: results[] says which path failed.
  const bool bAllValid = InvalidCount == 0;
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetBoolField(TEXT("isValid"), bAllValid);
  Result->SetArrayField(TEXT("results"), Results);
  Result->SetNumberField(TEXT("checkedCount"), Results.Num());
  Result->SetNumberField(TEXT("invalidCount"), InvalidCount);
  Self->SendAutomationResponse(
      RequestingSocket, RequestId, true,
      bAllValid ? TEXT("Asset validation completed; all assets valid")
                : FString::Printf(
                      TEXT("Asset validation completed; %d of %d path(s) invalid - see results[]"),
                      InvalidCount, Results.Num()),
      Result, FString());
  return true;
}

}
