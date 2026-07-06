#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"

namespace McpSequencePathSecurity {
namespace {

FString ExtractScheme(const FString &Url) {
  int32 SchemeEnd = INDEX_NONE;
  return Url.FindChar(TEXT(':'), SchemeEnd) && SchemeEnd > 0
             ? Url.Left(SchemeEnd).ToLower()
             : FString();
}

}

bool ValidateRemoteMediaUrl(const FString &InputUrl, FString &OutResolvedUrl,
                            ERemoteMediaUrlError &OutErrorType,
                            FString &OutError) {
  OutResolvedUrl = InputUrl.TrimStartAndEnd();
  OutErrorType = ERemoteMediaUrlError::InvalidArgument;
  OutError.Reset();
  // Empty URL is rejected as invalid input. This is intentionally stricter
  // than the TS-side validateUrlArgument, which returns undefined (no-op) for
  // empty input. The TS layer treats empty as "caller did not provide a URL";
  // the C++ layer treats empty as "the URL parameter, if supplied, must not be
  // blank". Callers that want to omit the URL must not send the key at all.
  if (OutResolvedUrl.IsEmpty()) {
    OutError = TEXT("A non-empty media URL is required.");
    return false;
  }
  const FString Scheme = ExtractScheme(OutResolvedUrl);
  if (Scheme == TEXT("file")) {
    OutError = TEXT(
        "file:// media URLs are not allowed; use filePath/mediaPath with an allowed local root.");
    return false;
  }
  if (Scheme != TEXT("http") && Scheme != TEXT("https")) {
    OutError = TEXT("Only http:// and https:// media URLs are recognized.");
    return false;
  }
  OutErrorType = ERemoteMediaUrlError::NotAllowed;
  OutError = TEXT(
      "[REMOTE_MEDIA_NETWORK_DISABLED] Network media URLs are disabled because the Unreal media backend can follow redirects outside the validated destination. Use filePath/mediaPath under Project Content or Project Saved.");
  return false;
}

}
