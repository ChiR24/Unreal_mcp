// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleGetSourceControlState(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("get_source_control_state"), ESearchCase::IgnoreCase)) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(Socket, RequestId,
                        TEXT("get_source_control_state payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Accept both assetPath and assetPaths
  TArray<FString> AssetPaths;
  const TArray<TSharedPtr<FJsonValue>> *AssetPathsArray = nullptr;
  if (Payload->TryGetArrayField(TEXT("assetPaths"), AssetPathsArray) &&
      AssetPathsArray && AssetPathsArray->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Val : *AssetPathsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        AssetPaths.Add(Val->AsString());
      }
    }
  } else {
    FString SinglePath;
    if (Payload->TryGetStringField(TEXT("assetPath"), SinglePath) && !SinglePath.IsEmpty()) {
      AssetPaths.Add(SinglePath);
    }
  }

  if (AssetPaths.Num() == 0) {
    SendAutomationError(Socket, RequestId,
                        TEXT("assetPath (string) or assetPaths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("sourceControlEnabled"), false);
    Result->SetStringField(TEXT("message"), TEXT("Source control is not enabled"));
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Source control disabled"), Result, FString());
    return true;
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<TSharedPtr<FJsonValue>> StatesArray;

  for (const FString &AssetPath : AssetPaths) {
    const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
    TSharedPtr<FJsonObject> StateObj = McpHandlerUtils::CreateResultObject();
    StateObj->SetStringField(TEXT("assetPath"), SafeAssetPath.IsEmpty() ? AssetPath : SafeAssetPath);

    if (SafeAssetPath.IsEmpty()) {
      StateObj->SetBoolField(TEXT("exists"), false);
      StateObj->SetStringField(TEXT("state"), TEXT("invalid_path"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Check if asset exists
    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      StateObj->SetBoolField(TEXT("exists"), false);
      StateObj->SetStringField(TEXT("state"), TEXT("not_found"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    StateObj->SetBoolField(TEXT("exists"), true);

    // Convert asset path to file path
    FString PackageName = FPackageName::ObjectPathToPackageName(SafeAssetPath);
    FString FilePath;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension()) &&
        !FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetMapPackageExtension())) {
      StateObj->SetStringField(TEXT("state"), TEXT("path_conversion_failed"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Get source control state
    FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(
        FilePath, EStateCacheUsage::Use);

    if (!SourceControlState.IsValid()) {
      StateObj->SetStringField(TEXT("state"), TEXT("unknown"));
      StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
      continue;
    }

    // Populate state info
    StateObj->SetBoolField(TEXT("isSourceControlled"), SourceControlState->IsSourceControlled());
    StateObj->SetBoolField(TEXT("isCheckedOut"), SourceControlState->IsCheckedOut());
    StateObj->SetBoolField(TEXT("isCurrent"), SourceControlState->IsCurrent());
    StateObj->SetBoolField(TEXT("isAdded"), SourceControlState->IsAdded());
    StateObj->SetBoolField(TEXT("isDeleted"), SourceControlState->IsDeleted());
    StateObj->SetBoolField(TEXT("isModified"), SourceControlState->IsModified());
    StateObj->SetBoolField(TEXT("isIgnored"), SourceControlState->IsIgnored());
    StateObj->SetBoolField(TEXT("isUnknown"), SourceControlState->IsUnknown());
    StateObj->SetBoolField(TEXT("canCheckIn"), SourceControlState->CanCheckIn());
    StateObj->SetBoolField(TEXT("canCheckout"), SourceControlState->CanCheckout());
    StateObj->SetBoolField(TEXT("canRevert"), SourceControlState->CanRevert());
    StateObj->SetBoolField(TEXT("canEdit"), SourceControlState->CanEdit());
    StateObj->SetBoolField(TEXT("canDelete"), SourceControlState->CanDelete());
    StateObj->SetBoolField(TEXT("canAdd"), SourceControlState->CanAdd());
    StateObj->SetBoolField(TEXT("isConflicted"), SourceControlState->IsConflicted());

    // Check if checked out by other
    FString WhoCheckedOut;
    bool bIsCheckedOutOther = SourceControlState->IsCheckedOutOther(&WhoCheckedOut);
    StateObj->SetBoolField(TEXT("isCheckedOutOther"), bIsCheckedOutOther);
    if (bIsCheckedOutOther && !WhoCheckedOut.IsEmpty()) {
      StateObj->SetStringField(TEXT("checkedOutBy"), WhoCheckedOut);
    }

    // Determine primary state string
    FString StateString = TEXT("unknown");
    if (!SourceControlState->IsSourceControlled()) {
      StateString = TEXT("not_controlled");
    } else if (SourceControlState->IsAdded()) {
      StateString = TEXT("added");
    } else if (SourceControlState->IsDeleted()) {
      StateString = TEXT("deleted");
    } else if (SourceControlState->IsConflicted()) {
      StateString = TEXT("conflicted");
    } else if (SourceControlState->IsCheckedOut()) {
      StateString = TEXT("checked_out");
    } else if (SourceControlState->IsModified()) {
      StateString = TEXT("modified");
    } else if (!SourceControlState->IsCurrent()) {
      StateString = TEXT("out_of_date");
    } else {
      StateString = TEXT("current");
    }
    StateObj->SetStringField(TEXT("state"), StateString);

    // Get display name
    StateObj->SetStringField(TEXT("displayName"), SourceControlState->GetDisplayName().ToString());

    StatesArray.Add(MakeShared<FJsonValueObject>(StateObj));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("sourceControlEnabled"), true);
  Result->SetArrayField(TEXT("states"), StatesArray);
  Result->SetNumberField(TEXT("queriedCount"), AssetPaths.Num());

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Source control state retrieved"), Result, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_source_control_state requires editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// ANALYZE GRAPH
// ============================================================================
