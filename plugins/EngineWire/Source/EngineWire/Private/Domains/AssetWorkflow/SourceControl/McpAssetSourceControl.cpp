// Copyright (c) 2024 EngineWire Contributors

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/PackageName.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"

namespace
{
// The packages a checkout or submit names (assetPath, or the paths / assetPaths list), with the asset paths they came
// from; false after the reply when no path was sent, source control is off, or no path names an asset of the project.
// A path that names none is listed in OutSkipped.
bool SourceControlPackagesOrReply(UEngineWireSubsystem *Self, const FString &RequestId,
                                  TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
                                  const TSharedPtr<FJsonObject> &Payload, TArray<FString> &PackageNames,
                                  TArray<FString> &ValidPaths, TArray<TSharedPtr<FJsonValue>> &Skipped) {
  // The contract declares `paths`; only assetPaths was read, so a multi-asset
  // call through the gateway was impossible. assetPaths stays as the fallback.
  const TArray<FString> AssetPaths = McpGetStringListField(
      Payload, Payload->HasField(TEXT("paths")) ? TEXT("paths") : TEXT("assetPaths"), TEXT("assetPath"));

  if (AssetPaths.Num() == 0) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                        TEXT("assetPath (string) or paths (array) required"),
                        TEXT("INVALID_ARGUMENT"));
    return false;
  }

  if (!ISourceControlModule::Get().IsEnabled()) {
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("Source control is off: turn it on with sourceControlOp enable and a provider (Git, Perforce or Subversion)."), nullptr,
                           TEXT("SOURCE_CONTROL_DISABLED"));
    return false;
  }

  // A path that named no asset was dropped without a word, so a typo looked checked out.
  for (const FString &Path : AssetPaths) {
    const FString SafePath = SanitizeProjectRelativePath(Path);
    if (!SafePath.IsEmpty() && McpAssetExists(SafePath)) {
      ValidPaths.Add(SafePath);
      PackageNames.Add(FPackageName::ObjectPathToPackageName(SafePath));
    } else {
      Skipped.Add(MakeShared<FJsonValueString>(Path));
    }
  }

  if (PackageNames.Num() == 0) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetArrayField(TEXT("skipped"), Skipped);
    Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                           FString::Printf(TEXT("No asset in the project at %s; nothing was sent to source control."),
                                           *FString::Join(AssetPaths, TEXT(", "))), Result,
                           TEXT("NO_VALID_ASSETS"));
    return false;
  }
  return true;
}
} // namespace

bool UEngineWireSubsystem::HandleSourceControlCheckout(
    const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TArray<FString> PackageNames;
  TArray<FString> ValidPaths;
  TArray<TSharedPtr<FJsonValue>> Skipped;
  if (!SourceControlPackagesOrReply(this, RequestId, RequestingSocket, Payload, PackageNames, ValidPaths, Skipped)) {
    return true;
  }

  bool bSuccess = SourceControlHelpers::CheckOutFiles(PackageNames, true);

  TArray<TSharedPtr<FJsonValue>> CheckedOutPaths = McpHandlerUtils::ToJsonStringArray(ValidPaths);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bSuccess);
  Result->SetNumberField(TEXT("checkedOut"), PackageNames.Num());
  Result->SetArrayField(TEXT("assets"), CheckedOutPaths);
  Result->SetArrayField(TEXT("skipped"), Skipped);

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess,
                         bSuccess ? TEXT("Assets checked out successfully")
                                  : TEXT("Checkout failed"),
                         Result,
                         bSuccess ? FString() : TEXT("CHECKOUT_FAILED"));
  return true;
}

bool UEngineWireSubsystem::HandleSourceControlSubmit(
    const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TArray<FString> PackageNames;
  TArray<FString> ValidPaths;
  TArray<TSharedPtr<FJsonValue>> Skipped;
  if (!SourceControlPackagesOrReply(this, RequestId, RequestingSocket, Payload, PackageNames, ValidPaths, Skipped)) {
    return true;
  }

  FString Description;
  if (!Payload->TryGetStringField(TEXT("description"), Description) ||
      Description.IsEmpty()) {
    Description = TEXT("Automated submission via EngineWire");
  }

  ISourceControlProvider &SourceControlProvider =
      ISourceControlModule::Get().GetProvider();

  TArray<FString> FilePaths;
  for (const FString &PackageName : PackageNames) {
    FString FilePath;
    if (FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetAssetPackageExtension()) ||
        FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, FilePath, FPackageName::GetMapPackageExtension())) {
      FilePaths.Add(FilePath);
    }
  }

  TSharedRef<FCheckIn, ESPMode::ThreadSafe> CheckInOperation =
      ISourceControlOperation::Create<FCheckIn>();
  CheckInOperation->SetDescription(FText::FromString(Description));

  ECommandResult::Type Result =
      SourceControlProvider.Execute(CheckInOperation, FilePaths);
  bool bSuccess = (Result == ECommandResult::Succeeded);

  TSharedPtr<FJsonObject> ResultObj = McpHandlerUtils::CreateResultObject();
  ResultObj->SetBoolField(TEXT("success"), bSuccess);
  ResultObj->SetNumberField(TEXT("submitted"),
                            bSuccess ? PackageNames.Num() : 0);
  ResultObj->SetArrayField(TEXT("skipped"), Skipped);

  SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Assets submitted successfully") : TEXT("Submit failed"),
      ResultObj, bSuccess ? FString() : TEXT("SUBMIT_FAILED"));
  return true;
}

bool UEngineWireSubsystem::HandleSourceControlEnable(
    const FString &RequestId,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString Provider = TEXT("None");
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("provider"), Provider);
  }

  ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
  const bool bWantsProvider = !Provider.IsEmpty() && !Provider.Equals(TEXT("None"), ESearchCase::IgnoreCase);

  // Asking for another provider while one was enabled answered "already enabled" and switched nothing.
  if (SourceControlModule.IsEnabled() &&
      (!bWantsProvider || SourceControlModule.GetProvider().GetName().ToString().Equals(Provider, ESearchCase::IgnoreCase))) {
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control already enabled"), Result, FString());
    return true;
  }

  // SetProvider ends in a Fatal log for a name no registered provider has (a typo, or a provider whose plugin is off):
  // only a registered name is set, and any other gets the refusal below.
  bool bKnownProvider = false;
  if (bWantsProvider) {
    TArray<FName> Registered;
    SourceControlModule.GetProviderNames(Registered);
    bKnownProvider = Registered.Contains(FName(*Provider));
    if (bKnownProvider) {
      SourceControlModule.SetProvider(FName(*Provider));
    }
  }

  bool bEnabled = SourceControlModule.IsEnabled() && (!bWantsProvider || bKnownProvider);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), bEnabled);
  Result->SetStringField(TEXT("provider"), SourceControlModule.GetProvider().GetName().ToString());

  if (bEnabled) {
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Source control enabled"), Result, FString());
  } else {
    // It pointed at the Editor preferences dialog, the one place an automation caller cannot reach.
    Result->SetStringField(TEXT("error"), bWantsProvider
        ? FString::Printf(TEXT("Provider '%s' is not available or could not connect: its plugin must be enabled, and Git needs "
            "a repository at the project root (source_control_init makes one)."), *Provider)
        : FString(TEXT("Source control is off: name a provider (provider Git, Perforce or Subversion) to turn it on.")));
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           Result->GetStringField(TEXT("error")), Result,
                           TEXT("SOURCE_CONTROL_ENABLE_FAILED"));
  }
  return true;
}
