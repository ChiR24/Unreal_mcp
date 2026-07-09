#include "Domains/Sequence/Media/McpAutomationBridge_SequenceMedia.h"

#include "Domains/Sequence/McpAutomationBridge_SequencePathSecurity.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"

namespace McpSequenceMedia {
namespace {

bool ValidateMediaUrl(const FString &Url, FString &OutCode,
                      FString &OutError) {
  if (Url.StartsWith(TEXT("file://"), ESearchCase::IgnoreCase)) {
    FString ResolvedPath;
    if (!McpSequencePathSecurity::ValidateLocalPath(
            Url.Mid(7), McpSequencePathSecurity::ELocalPathUse::MediaInput,
            ResolvedPath, OutError)) {
      OutCode = TEXT("MEDIA_PATH_NOT_ALLOWED");
      return false;
    }
    if (!IFileManager::Get().FileExists(*ResolvedPath)) {
      OutCode = TEXT("MEDIA_FILE_NOT_FOUND");
      OutError = TEXT("The referenced media file does not exist");
      return false;
    }
    return true;
  }

  FString ResolvedUrl;
  McpSequencePathSecurity::ERemoteMediaUrlError ErrorType;
  if (!McpSequencePathSecurity::ValidateRemoteMediaUrl(
          Url, ResolvedUrl, ErrorType, OutError)) {
    OutCode =
        ErrorType == McpSequencePathSecurity::ERemoteMediaUrlError::NotAllowed
            ? TEXT("MEDIA_URL_NOT_ALLOWED")
            : TEXT("INVALID_MEDIA_SOURCE");
    return false;
  }
  return true;
}

bool ValidateNestedSources(UObject *Source, UClass *MediaSourceClass,
                           TSet<const UObject *> &Visited, int32 Depth,
                           FString &OutCode, FString &OutError) {
  if (!Source || !MediaSourceClass || !Source->IsA(MediaSourceClass) ||
      Visited.Contains(Source)) {
    return true;
  }
  if (Depth > 8) {
    OutCode = TEXT("INVALID_MEDIA_SOURCE");
    OutError = TEXT("Media source nesting exceeds the supported depth");
    return false;
  }
  Visited.Add(Source);
  if (!CallBoolFunction(Source, TEXT("Validate"))) {
    OutCode = TEXT("INVALID_MEDIA_SOURCE");
    OutError = FString::Printf(TEXT("Media source is invalid: %s"),
                               *Source->GetPathName());
    return false;
  }
  const FString Url = GetMediaUrl(Source);
  if (!Url.IsEmpty() && !ValidateMediaUrl(Url, OutCode, OutError)) {
    return false;
  }

  for (TFieldIterator<FProperty> It(Source->GetClass()); It; ++It) {
    FProperty *Property = *It;
    if (FObjectPropertyBase *ObjectProperty =
            CastField<FObjectPropertyBase>(Property)) {
      UObject *Nested =
          ObjectProperty->GetObjectPropertyValue_InContainer(Source);
      if (!ValidateNestedSources(Nested, MediaSourceClass, Visited, Depth + 1,
                                 OutCode, OutError)) {
        return false;
      }
      continue;
    }
    FMapProperty *MapProperty = CastField<FMapProperty>(Property);
    FObjectPropertyBase *ValueProperty =
        MapProperty ? CastField<FObjectPropertyBase>(MapProperty->ValueProp)
                    : nullptr;
    if (!MapProperty || !ValueProperty) {
      continue;
    }
    FScriptMapHelper Map(MapProperty,
                         MapProperty->ContainerPtrToValuePtr<void>(Source));
    for (int32 Index = 0; Index < Map.GetMaxIndex(); ++Index) {
      if (!Map.IsValidIndex(Index)) {
        continue;
      }
      UObject *Nested =
          ValueProperty->GetObjectPropertyValue(Map.GetValuePtr(Index));
      if (!ValidateNestedSources(Nested, MediaSourceClass, Visited, Depth + 1,
                                 OutCode, OutError)) {
        return false;
      }
    }
  }
  return true;
}

}

bool ValidateMediaSourcePolicy(UObject *MediaSource, FString &OutErrorCode,
                               FString &OutError) {
  OutErrorCode.Reset();
  OutError.Reset();
  FString ClassError;
  UClass *MediaSourceClass =
      ResolveMediaClass(TEXT("MediaSource"), ClassError);
  if (!MediaSource || !MediaSourceClass || !MediaSource->IsA(MediaSourceClass)) {
    OutErrorCode = TEXT("INVALID_MEDIA_SOURCE");
    OutError = ClassError.IsEmpty() ? TEXT("A valid media source is required")
                                    : ClassError;
    return false;
  }
  TSet<const UObject *> Visited;
  return ValidateNestedSources(MediaSource, MediaSourceClass, Visited, 0,
                               OutErrorCode, OutError);
}

}
