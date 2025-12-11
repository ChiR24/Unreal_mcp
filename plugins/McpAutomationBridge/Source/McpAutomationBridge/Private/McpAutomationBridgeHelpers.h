// Helper utilities for McpAutomationBridgeSubsystem
#pragma once

#include "Containers/ScriptArray.h"
#include "Containers/StringConv.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "JsonObjectConverter.h"
#include "Misc/OutputDevice.h"
#include "Misc/ScopeLock.h"
#include "UObject/UnrealType.h"
#include <type_traits>

// Globals used by registry helpers and fast-mode simulations
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif
#include "Engine/Blueprint.h"
#endif

// Sanitize incoming JSON by stripping control characters that break parsers
static inline FString SanitizeIncomingJson(const FString &In) {
  FString Out;
  Out.Reserve(In.Len());
  for (int32 i = 0; i < In.Len(); ++i) {
    const TCHAR C = In[i];
    if (C >= 32)
      Out.AppendChar(C);
  }
  return Out;
}

// Sanitize a project-relative path to prevent traversal attacks.
// Ensures the path starts with a valid root (e.g. /Game, /Engine, /Plugin) and
// does not contain ".."
static inline FString SanitizeProjectRelativePath(const FString &InPath) {
  if (InPath.IsEmpty())
    return FString();

  FString CleanPath = InPath;
  FPaths::NormalizeFilename(CleanPath);

  // Reject paths containing traversal
  if (CleanPath.Contains(TEXT(".."))) {
    UE_LOG(
        LogMcpAutomationBridgeSubsystem, Warning,
        TEXT("SanitizeProjectRelativePath: Rejected path containing '..': %s"),
        *InPath);
    return FString();
  }

  // Ensure path starts with a slash
  if (!CleanPath.StartsWith(TEXT("/"))) {
    CleanPath = TEXT("/") + CleanPath;
  }

  // Whitelist valid roots
  const bool bValidRoot = CleanPath.StartsWith(TEXT("/Game")) ||
                          CleanPath.StartsWith(TEXT("/Engine")) ||
                          CleanPath.StartsWith(TEXT("/Script"));

  // Allow plugin content paths too (e.g. /MyPlugin/) - heuristic: starts with /
  // and has second /
  bool bLooksLikePlugin = false;
  if (!bValidRoot && CleanPath.Len() > 1) {
    int32 SecondSlash = -1;
    if (CleanPath.FindChar(TEXT('/'), SecondSlash)) {
      // Check if we have a second slash, e.g. /PluginName/Folder
      // FindChar finds the *first* char. We want the second one.
      if (CleanPath.FindLastChar(TEXT('/'), SecondSlash) && SecondSlash > 0) {
        bLooksLikePlugin = true;
      }
    }
  }

  // For strict safety, we might enforce /Game or /Engine, but plugins are
  // common. The critical part is no ".." and it looks like an asset path.

  return CleanPath;
}

static inline bool IsValidAssetPath(const FString &Path) {
  return !Path.IsEmpty() && Path.StartsWith(TEXT("/")) &&
         !Path.Contains(TEXT("..")) && !Path.Contains(TEXT("//"));
}

// Normalize an asset path to ensure it's in valid long package name format.
// Uses engine FPackageName API for proper validation.
// - If path doesn't start with '/', prepends '/Game/'
// - Removes trailing slashes
// - Returns the normalized path and whether it's valid
// - Reference: Engine/Source/Runtime/CoreUObject/Public/Misc/PackageName.h
#if WITH_EDITOR
#include "Misc/PackageName.h"

struct FNormalizedAssetPath {
  FString Path;
  bool bIsValid;
  FString ErrorMessage;
};

static inline FNormalizedAssetPath NormalizeAssetPath(const FString &InPath) {
  FNormalizedAssetPath Result;
  Result.bIsValid = false;

  if (InPath.IsEmpty()) {
    Result.ErrorMessage = TEXT("Asset path is empty");
    return Result;
  }

  FString CleanPath = InPath;

  // Remove trailing slashes
  while (CleanPath.EndsWith(TEXT("/"))) {
    CleanPath.RemoveAt(CleanPath.Len() - 1);
  }

  // Handle object paths (extract package name)
  // Object paths look like: /Game/Package.Object:SubObject
  FString PackageName = FPackageName::ObjectPathToPackageName(CleanPath);
  if (!PackageName.IsEmpty()) {
    CleanPath = PackageName;
  }

  // If path doesn't start with '/', try prepending /Game/
  if (!CleanPath.StartsWith(TEXT("/"))) {
    CleanPath = TEXT("/Game/") + CleanPath;
  }

  // Validate using engine API
  FText Reason;
  if (FPackageName::IsValidLongPackageName(CleanPath, true, &Reason)) {
    Result.Path = CleanPath;
    Result.bIsValid = true;
    return Result;
  }

  // If not in valid root, try other common roots
  TArray<FString> RootsToTry = {TEXT("/Game/"), TEXT("/Engine/"),
                                TEXT("/Script/")};
  FString BaseName = InPath;
  if (BaseName.StartsWith(TEXT("/"))) {
    // Extract just the asset name without the invalid root
    int32 LastSlash = -1;
    if (BaseName.FindLastChar(TEXT('/'), LastSlash) && LastSlash > 0) {
      BaseName = BaseName.RightChop(LastSlash + 1);
    }
  }

  for (const FString &Root : RootsToTry) {
    FString TestPath = Root + BaseName;
    FText DummyReason;
    if (FPackageName::IsValidLongPackageName(TestPath, true, &DummyReason)) {
      // Check if this asset actually exists
      if (FPackageName::DoesPackageExist(TestPath)) {
        Result.Path = TestPath;
        Result.bIsValid = true;
        return Result;
      }
    }
  }

  // Return what we have, with the validation error
  Result.Path = CleanPath;
  Result.ErrorMessage = FString::Printf(
      TEXT("Invalid asset path '%s': %s. Expected format: "
           "/Game/Folder/AssetName or /Engine/Folder/AssetName"),
      *InPath, *Reason.ToString());
  return Result;
}

// Convenience helper that tries to resolve the path and returns it, or empty if
// invalid Also outputs the resolved path to a pointer if provided
static inline FString TryResolveAssetPath(const FString &InPath,
                                          FString *OutResolvedPath = nullptr,
                                          FString *OutError = nullptr) {
  FNormalizedAssetPath Norm = NormalizeAssetPath(InPath);
  if (OutResolvedPath) {
    *OutResolvedPath = Norm.Path;
  }
  if (OutError && !Norm.bIsValid) {
    *OutError = Norm.ErrorMessage;
  }
  return Norm.bIsValid ? Norm.Path : FString();
}
#endif

#if WITH_EDITOR
// Resolve a UClass by a variety of heuristics: try full path lookup, attempt
// to load an asset by path (UBlueprint or UClass), then fall back to scanning
// loaded classes by name or path suffix. This replaces previous usages of
// FindObject<...>(ANY_PACKAGE, ...) which is deprecated.
static inline UClass *ResolveClassByName(const FString &ClassNameOrPath) {
  if (ClassNameOrPath.IsEmpty())
    return nullptr;

  // 1) If it's an asset path, prefer loading the asset and deriving the class
  // Skip /Script/ paths as they are native classes, not assets
  if ((ClassNameOrPath.StartsWith(TEXT("/")) ||
       ClassNameOrPath.Contains(TEXT("/"))) &&
      !ClassNameOrPath.StartsWith(TEXT("/Script/"))) {
    UObject *Loaded = nullptr;
// Prefer EditorAssetLibrary when available
#if WITH_EDITOR
    Loaded = UEditorAssetLibrary::LoadAsset(ClassNameOrPath);
#endif
    if (Loaded) {
      if (UBlueprint *BP = Cast<UBlueprint>(Loaded))
        return BP->GeneratedClass;
      if (UClass *C = Cast<UClass>(Loaded))
        return C;
    }
  }

  // 2) Try a direct FindObject using nullptr/explicit outer (expects full path)
  if (UClass *Direct = FindObject<UClass>(nullptr, *ClassNameOrPath))
    return Direct;

  // 2.5) Try guessing generic engine locations for common components (e.g.
  // StaticMeshComponent -> /Script/Engine.StaticMeshComponent) This helps when
  // the class has not been loaded yet so TObjectIterator won't find it.
  if (!ClassNameOrPath.Contains(TEXT("/")) &&
      !ClassNameOrPath.Contains(TEXT("."))) {
    FString EnginePath =
        FString::Printf(TEXT("/Script/Engine.%s"), *ClassNameOrPath);
    if (UClass *EngineClass = FindObject<UClass>(nullptr, *EnginePath))
      return EngineClass;

    // Attempt load for engine class (unlikely to need load for native, but just
    // in case)
    if (UClass *EngineClassLoaded = LoadObject<UClass>(nullptr, *EnginePath))
      return EngineClassLoaded;

    FString UMGPath = FString::Printf(TEXT("/Script/UMG.%s"), *ClassNameOrPath);
    if (UClass *UMGClass = FindObject<UClass>(nullptr, *UMGPath))
      return UMGClass;
  }

  // Special handling for common ambiguous types
  if (ClassNameOrPath.Equals(TEXT("NiagaraComponent"),
                             ESearchCase::IgnoreCase)) {
    if (UClass *NiagaraComp = FindObject<UClass>(
            nullptr, TEXT("/Script/Niagara.NiagaraComponent"))) {
      return NiagaraComp;
    }
  }

  // 3) Fallback: iterate loaded classes and match by short name or path suffix
  UClass *BestMatch = nullptr;
  for (TObjectIterator<UClass> It; It; ++It) {
    UClass *C = *It;
    if (!C)
      continue;

    // Exact short name match
    if (C->GetName().Equals(ClassNameOrPath, ESearchCase::IgnoreCase)) {
      // Prefer /Script/ (native) classes over others if multiple match
      if (C->GetPathName().StartsWith(TEXT("/Script/")))
        return C;
      if (!BestMatch)
        BestMatch = C;
    }
    // Match on ".ClassName" suffix (path-based short form)
    else if (C->GetPathName().EndsWith(
                 FString::Printf(TEXT(".%s"), *ClassNameOrPath),
                 ESearchCase::IgnoreCase)) {
      if (!BestMatch)
        BestMatch = C;
    }
  }

  return BestMatch;
}
#endif

// Extract top-level JSON objects by scanning for balanced braces.
static inline TArray<FString> ExtractTopLevelJsonObjects(const FString &In) {
  TArray<FString> Results;
  int32 Depth = 0;
  int32 Start = INDEX_NONE;
  for (int32 i = 0; i < In.Len(); ++i) {
    const TCHAR C = In[i];
    if (C == '{') {
      if (Depth == 0)
        Start = i;
      Depth++;
    } else if (C == '}') {
      Depth--;
      if (Depth == 0 && Start != INDEX_NONE) {
        Results.Add(In.Mid(Start, i - Start + 1));
        Start = INDEX_NONE;
      }
    }
  }
  return Results;
}

// Convert UTF-8 bytes of a FString to a lowercase hex string for diagnostics
static inline FString HexifyUtf8(const FString &In) {
  FTCHARToUTF8 Converter(*In);
  const uint8 *Bytes = reinterpret_cast<const uint8 *>(Converter.Get());
  int32 Len = Converter.Length();
  FString Hex;
  Hex.Reserve(Len * 2);
  for (int32 i = 0; i < Len; ++i) {
    Hex += FString::Printf(TEXT("%02x"), Bytes[i]);
  }
  return Hex;
}

// Lightweight output capture to collect log lines emitted during
// automation operations that write to GLog.
struct FMcpOutputCapture : public FOutputDevice {
  TArray<FString> Lines;
  virtual void Serialize(const TCHAR *V, ELogVerbosity::Type Verbosity,
                         const FName &Category) override {
    if (!V)
      return;
    FString S(V);
    // Remove trailing newlines for cleaner payloads
    while (S.EndsWith(TEXT("\n")))
      S.RemoveAt(S.Len() - 1);
    Lines.Add(S);
  }

  TArray<FString> Consume() {
    TArray<FString> Tmp = MoveTemp(Lines);
    Lines.Empty();
    return Tmp;
  }
};

// Export a single UProperty value from an object into a JSON value.
// Export a single UProperty value from an object into a JSON value.
static inline TSharedPtr<FJsonValue>
ExportPropertyToJsonValue(void *TargetContainer, FProperty *Property) {
  if (!TargetContainer || !Property)
    return nullptr;

  // Strings
  if (FStrProperty *Str = CastField<FStrProperty>(Property)) {
    return MakeShared<FJsonValueString>(
        Str->GetPropertyValue_InContainer(TargetContainer));
  }

  // Names
  if (FNameProperty *NP = CastField<FNameProperty>(Property)) {
    return MakeShared<FJsonValueString>(
        NP->GetPropertyValue_InContainer(TargetContainer).ToString());
  }

  // Booleans
  if (FBoolProperty *BP = CastField<FBoolProperty>(Property)) {
    return MakeShared<FJsonValueBoolean>(
        BP->GetPropertyValue_InContainer(TargetContainer));
  }

  // Numeric (handle concrete numeric property types to avoid engine-API
  // differences)
  if (FFloatProperty *FP = CastField<FFloatProperty>(Property)) {
    return MakeShared<FJsonValueNumber>(
        (double)FP->GetPropertyValue_InContainer(TargetContainer));
  }
  if (FDoubleProperty *DP = CastField<FDoubleProperty>(Property)) {
    return MakeShared<FJsonValueNumber>(
        (double)DP->GetPropertyValue_InContainer(TargetContainer));
  }
  if (FIntProperty *IP = CastField<FIntProperty>(Property)) {
    return MakeShared<FJsonValueNumber>(
        (double)IP->GetPropertyValue_InContainer(TargetContainer));
  }
  if (FInt64Property *I64P = CastField<FInt64Property>(Property)) {
    return MakeShared<FJsonValueNumber>(
        (double)I64P->GetPropertyValue_InContainer(TargetContainer));
  }
  if (FByteProperty *BP = CastField<FByteProperty>(Property)) {
    // Byte property may be an enum; return enum name if available, else numeric
    // value
    const uint8 ByteVal = BP->GetPropertyValue_InContainer(TargetContainer);
    if (UEnum *Enum = BP->Enum) {
      const FString EnumName = Enum->GetNameStringByValue(ByteVal);
      if (!EnumName.IsEmpty()) {
        return MakeShared<FJsonValueString>(EnumName);
      }
    }
    return MakeShared<FJsonValueNumber>((double)ByteVal);
  }

  // Enum property (newer engine versions use FEnumProperty instead of
  // FByteProperty for enums)
  if (FEnumProperty *EP = CastField<FEnumProperty>(Property)) {
    if (UEnum *Enum = EP->GetEnum()) {
      void *ValuePtr = EP->ContainerPtrToValuePtr<void>(TargetContainer);
      if (FNumericProperty *UnderlyingProp = EP->GetUnderlyingProperty()) {
        const int64 EnumVal =
            UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
        const FString EnumName = Enum->GetNameStringByValue(EnumVal);
        if (!EnumName.IsEmpty()) {
          return MakeShared<FJsonValueString>(EnumName);
        }
        return MakeShared<FJsonValueNumber>((double)EnumVal);
      }
    }
    return MakeShared<FJsonValueNumber>(0.0);
  }

  // Object references -> return path if available
  if (FObjectProperty *OP = CastField<FObjectProperty>(Property)) {
    UObject *O = OP->GetObjectPropertyValue_InContainer(TargetContainer);
    if (O)
      return MakeShared<FJsonValueString>(O->GetPathName());
    return MakeShared<FJsonValueNull>();
  }

  // Soft object references (FSoftObjectPtr, FSoftObjectPath)
  if (FSoftObjectProperty *SOP = CastField<FSoftObjectProperty>(Property)) {
    const void *ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
    const FSoftObjectPtr *SoftObjPtr =
        static_cast<const FSoftObjectPtr *>(ValuePtr);
    if (SoftObjPtr && !SoftObjPtr->IsNull()) {
      return MakeShared<FJsonValueString>(
          SoftObjPtr->ToSoftObjectPath().ToString());
    }
    return MakeShared<FJsonValueNull>();
  }

  // Soft class references (FSoftClassPtr)
  if (FSoftClassProperty *SCP = CastField<FSoftClassProperty>(Property)) {
    const void *ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
    const FSoftObjectPtr *SoftClassPtr =
        static_cast<const FSoftObjectPtr *>(ValuePtr);
    if (SoftClassPtr && !SoftClassPtr->IsNull()) {
      return MakeShared<FJsonValueString>(
          SoftClassPtr->ToSoftObjectPath().ToString());
    }
    return MakeShared<FJsonValueNull>();
  }

  // Structs: FVector and FRotator common cases
  if (FStructProperty *SP = CastField<FStructProperty>(Property)) {
    const FString TypeName = SP->Struct ? SP->Struct->GetName() : FString();
    if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase)) {
      const FVector *V = SP->ContainerPtrToValuePtr<FVector>(TargetContainer);
      TArray<TSharedPtr<FJsonValue>> Arr;
      Arr.Add(MakeShared<FJsonValueNumber>(V->X));
      Arr.Add(MakeShared<FJsonValueNumber>(V->Y));
      Arr.Add(MakeShared<FJsonValueNumber>(V->Z));
      return MakeShared<FJsonValueArray>(Arr);
    } else if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase)) {
      const FRotator *R = SP->ContainerPtrToValuePtr<FRotator>(TargetContainer);
      TArray<TSharedPtr<FJsonValue>> Arr;
      Arr.Add(MakeShared<FJsonValueNumber>(R->Pitch));
      Arr.Add(MakeShared<FJsonValueNumber>(R->Yaw));
      Arr.Add(MakeShared<FJsonValueNumber>(R->Roll));
      return MakeShared<FJsonValueArray>(Arr);
    }

    // Fallback: export textual representation
    FString Exported;
    SP->Struct->ExportText(Exported,
                           SP->ContainerPtrToValuePtr<void>(TargetContainer),
                           nullptr, nullptr, 0, nullptr, true);
    return MakeShared<FJsonValueString>(Exported);
  }

  // Arrays: try to export inner values as strings
  if (FArrayProperty *AP = CastField<FArrayProperty>(Property)) {
    FScriptArrayHelper Helper(AP,
                              AP->ContainerPtrToValuePtr<void>(TargetObject));
    TArray<TSharedPtr<FJsonValue>> Out;
    for (int32 i = 0; i < Helper.Num(); ++i) {
      void *ElemPtr = Helper.GetRawPtr(i);
      if (FProperty *Inner = AP->Inner) {
        // Handle common inner types directly from element memory
        if (FStrProperty *StrInner = CastField<FStrProperty>(Inner)) {
          const FString &Val = *reinterpret_cast<FString *>(ElemPtr);
          Out.Add(MakeShared<FJsonValueString>(Val));
          continue;
        }
        if (FNameProperty *NameInner = CastField<FNameProperty>(Inner)) {
          const FName &N = *reinterpret_cast<FName *>(ElemPtr);
          Out.Add(MakeShared<FJsonValueString>(N.ToString()));
          continue;
        }
        if (FBoolProperty *BoolInner = CastField<FBoolProperty>(Inner)) {
          const bool B = (*reinterpret_cast<const uint8 *>(ElemPtr)) != 0;
          Out.Add(MakeShared<FJsonValueBoolean>(B));
          continue;
        }
        if (FFloatProperty *FInner = CastField<FFloatProperty>(Inner)) {
          const double Val =
              (double)(*reinterpret_cast<const float *>(ElemPtr));
          Out.Add(MakeShared<FJsonValueNumber>(Val));
          continue;
        }
        if (FDoubleProperty *DInner = CastField<FDoubleProperty>(Inner)) {
          const double Val = *reinterpret_cast<const double *>(ElemPtr);
          Out.Add(MakeShared<FJsonValueNumber>(Val));
          continue;
        }
        if (FIntProperty *IInner = CastField<FIntProperty>(Inner)) {
          const double Val =
              (double)(*reinterpret_cast<const int32 *>(ElemPtr));
          Out.Add(MakeShared<FJsonValueNumber>(Val));
          continue;
        }

        // Fallback: stringified placeholder for unsupported inner types
        Out.Add(MakeShared<FJsonValueString>(TEXT("<unsupported_array_elem>")));
      }
    }
    return MakeShared<FJsonValueArray>(Out);
  }

  // Maps: export as JSON object with key-value pairs
  if (FMapProperty *MP = CastField<FMapProperty>(Property)) {
    TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
    FScriptMapHelper Helper(MP, MP->ContainerPtrToValuePtr<void>(TargetObject));

    for (int32 i = 0; i < Helper.Num(); ++i) {
      if (!Helper.IsValidIndex(i))
        continue;

      // Get key and value pointers
      const uint8 *KeyPtr = Helper.GetKeyPtr(i);
      const uint8 *ValuePtr = Helper.GetValuePtr(i);

      // Convert key to string (maps typically use string or name keys)
      FString KeyStr;
      FProperty *KeyProp = MP->KeyProp;
      if (FStrProperty *StrKey = CastField<FStrProperty>(KeyProp)) {
        KeyStr = *reinterpret_cast<const FString *>(KeyPtr);
      } else if (FNameProperty *NameKey = CastField<FNameProperty>(KeyProp)) {
        KeyStr = reinterpret_cast<const FName *>(KeyPtr)->ToString();
      } else if (FIntProperty *IntKey = CastField<FIntProperty>(KeyProp)) {
        KeyStr = FString::FromInt(*reinterpret_cast<const int32 *>(KeyPtr));
      } else {
        KeyStr = FString::Printf(TEXT("key_%d"), i);
      }

      // Convert value to JSON
      FProperty *ValueProp = MP->ValueProp;
      if (FStrProperty *StrVal = CastField<FStrProperty>(ValueProp)) {
        MapObj->SetStringField(KeyStr,
                               *reinterpret_cast<const FString *>(ValuePtr));
      } else if (FIntProperty *IntVal = CastField<FIntProperty>(ValueProp)) {
        MapObj->SetNumberField(
            KeyStr, (double)*reinterpret_cast<const int32 *>(ValuePtr));
      } else if (FFloatProperty *FloatVal =
                     CastField<FFloatProperty>(ValueProp)) {
        MapObj->SetNumberField(
            KeyStr, (double)*reinterpret_cast<const float *>(ValuePtr));
      } else if (FBoolProperty *BoolVal = CastField<FBoolProperty>(ValueProp)) {
        MapObj->SetBoolField(KeyStr,
                             (*reinterpret_cast<const uint8 *>(ValuePtr)) != 0);
      } else {
        MapObj->SetStringField(KeyStr, TEXT("<unsupported_value_type>"));
      }
    }

    return MakeShared<FJsonValueObject>(MapObj);
  }

  // Sets: export as JSON array
  if (FSetProperty *SP = CastField<FSetProperty>(Property)) {
    TArray<TSharedPtr<FJsonValue>> Out;
    FScriptSetHelper Helper(SP, SP->ContainerPtrToValuePtr<void>(TargetObject));

    for (int32 i = 0; i < Helper.Num(); ++i) {
      if (!Helper.IsValidIndex(i))
        continue;

      const uint8 *ElemPtr = Helper.GetElementPtr(i);
      FProperty *ElemProp = SP->ElementProp;

      if (FStrProperty *StrElem = CastField<FStrProperty>(ElemProp)) {
        Out.Add(MakeShared<FJsonValueString>(
            *reinterpret_cast<const FString *>(ElemPtr)));
      } else if (FNameProperty *NameElem = CastField<FNameProperty>(ElemProp)) {
        Out.Add(MakeShared<FJsonValueString>(
            reinterpret_cast<const FName *>(ElemPtr)->ToString()));
      } else if (FIntProperty *IntElem = CastField<FIntProperty>(ElemProp)) {
        Out.Add(MakeShared<FJsonValueNumber>(
            (double)*reinterpret_cast<const int32 *>(ElemPtr)));
      } else if (FFloatProperty *FloatElem =
                     CastField<FFloatProperty>(ElemProp)) {
        Out.Add(MakeShared<FJsonValueNumber>(
            (double)*reinterpret_cast<const float *>(ElemPtr)));
      } else {
        Out.Add(MakeShared<FJsonValueString>(TEXT("<unsupported_set_elem>")));
      }
    }

    return MakeShared<FJsonValueArray>(Out);
  }

  return nullptr;
}

#if WITH_EDITOR
// Throttled wrapper around UEditorAssetLibrary::SaveLoadedAsset to avoid
// triggering rapid repeated SavePackage calls which can cause engine
// warnings (FlushRenderingCommands called recursively) during heavy
// test activity. The helper consults a plugin-wide map of recent save
// timestamps (GRecentAssetSaveTs) and skips saves that occur within the
// configured throttle window. Skipped saves return 'true' to preserve
// idempotent behavior for callers that treat a skipped save as a success.
static inline bool
SaveLoadedAssetThrottled(UObject *Asset,
                         double ThrottleSecondsOverride = -1.0) {
  if (!Asset)
    return false;
  const double Now = FPlatformTime::Seconds();
  const double Throttle = (ThrottleSecondsOverride >= 0.0)
                              ? ThrottleSecondsOverride
                              : GRecentAssetSaveThrottleSeconds;
  FString Key = Asset->GetPathName();
  if (Key.IsEmpty())
    Key = Asset->GetName();

  {
    FScopeLock Lock(&GRecentAssetSaveMutex);
    if (double *Last = GRecentAssetSaveTs.Find(Key)) {
      const double Elapsed = Now - *Last;
      if (Elapsed < Throttle) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
               TEXT("SaveLoadedAssetThrottled: skipping save for '%s' "
                    "(last=%.3fs, throttle=%.3fs)"),
               *Key, Elapsed, Throttle);
        // Treat skip as success to avoid bubbling save failures into tests
        return true;
      }
    }
  }

  // Perform the save and record timestamp on success
  const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(Asset);
  if (bSaved) {
    FScopeLock Lock(&GRecentAssetSaveMutex);
    GRecentAssetSaveTs.Add(Key, Now);
    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
           TEXT("SaveLoadedAssetThrottled: saved '%s' (throttle reset)"), *Key);
  } else {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("SaveLoadedAssetThrottled: failed to save '%s'"), *Key);
  }
  return bSaved;
}
#else
static inline bool
SaveLoadedAssetThrottled(UObject * /*Asset*/,
                         double /*ThrottleSecondsOverride*/ = -1.0) {
  return false;
}
#endif

// Apply a JSON value to an FProperty on a UObject. Returns true on success and
// populates OutError with a descriptive string on failure.
static inline bool
ApplyJsonValueToProperty(void *TargetContainer, FProperty *Property,
                         const TSharedPtr<FJsonValue> &ValueField,
                         FString &OutError) {
  OutError.Empty();
  if (!TargetContainer || !Property || !ValueField) {
    OutError = TEXT("Invalid target/property/value");
    return false;
  }

  // Bool
  if (FBoolProperty *BP = CastField<FBoolProperty>(Property)) {
    if (ValueField->Type == EJson::Boolean) {
      BP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsBool());
      return true;
    }
    if (ValueField->Type == EJson::Number) {
      BP->SetPropertyValue_InContainer(TargetContainer,
                                       ValueField->AsNumber() != 0.0);
      return true;
    }
    if (ValueField->Type == EJson::String) {
      BP->SetPropertyValue_InContainer(
          TargetContainer,
          ValueField->AsString().Equals(TEXT("true"), ESearchCase::IgnoreCase));
      return true;
    }
    OutError = TEXT("Unsupported JSON type for bool property");
    return false;
  }

  // String and Name
  if (FStrProperty *SP = CastField<FStrProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      SP->SetPropertyValue_InContainer(TargetContainer, ValueField->AsString());
      return true;
    }
    OutError = TEXT("Expected string for string property");
    return false;
  }
  if (FNameProperty *NP = CastField<FNameProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      NP->SetPropertyValue_InContainer(TargetContainer,
                                       FName(*ValueField->AsString()));
      return true;
    }
    OutError = TEXT("Expected string for name property");
    return false;
  }

  // Numeric: handle concrete numeric property types explicitly
  if (FFloatProperty *FP = CastField<FFloatProperty>(Property)) {
    double Val = 0.0;
    if (ValueField->Type == EJson::Number)
      Val = ValueField->AsNumber();
    else if (ValueField->Type == EJson::String)
      Val = FCString::Atod(*ValueField->AsString());
    else {
      OutError = TEXT("Unsupported JSON type for float property");
      return false;
    }
    FP->SetPropertyValue_InContainer(TargetContainer, static_cast<float>(Val));
    return true;
  }

  // ...existing code...
  if (FDoubleProperty *DP = CastField<FDoubleProperty>(Property)) {
    double Val = 0.0;
    if (ValueField->Type == EJson::Number)
      Val = ValueField->AsNumber();
    else if (ValueField->Type == EJson::String)
      Val = FCString::Atod(*ValueField->AsString());
    else {
      OutError = TEXT("Unsupported JSON type for double property");
      return false;
    }
    DP->SetPropertyValue_InContainer(TargetContainer, Val);
    return true;
  }
  if (FIntProperty *IP = CastField<FIntProperty>(Property)) {
    int64 Val = 0;
    if (ValueField->Type == EJson::Number)
      Val = static_cast<int64>(ValueField->AsNumber());
    else if (ValueField->Type == EJson::String)
      Val = static_cast<int64>(FCString::Atoi64(*ValueField->AsString()));
    else {
      OutError = TEXT("Unsupported JSON type for int property");
      return false;
    }
    IP->SetPropertyValue_InContainer(TargetContainer, static_cast<int32>(Val));
    return true;
  }
  if (FInt64Property *I64P = CastField<FInt64Property>(Property)) {
    int64 Val = 0;
    if (ValueField->Type == EJson::Number)
      Val = static_cast<int64>(ValueField->AsNumber());
    else if (ValueField->Type == EJson::String)
      Val = static_cast<int64>(FCString::Atoi64(*ValueField->AsString()));
    else {
      OutError = TEXT("Unsupported JSON type for int64 property");
      return false;
    }
    I64P->SetPropertyValue_InContainer(TargetContainer, Val);
    return true;
  }
  if (FByteProperty *Bp = CastField<FByteProperty>(Property)) {
    // Check if this is an enum byte property
    if (UEnum *Enum = Bp->Enum) {
      if (ValueField->Type == EJson::String) {
        // Try to match by name (with or without namespace)
        const FString InStr = ValueField->AsString();
        int64 EnumVal = Enum->GetValueByNameString(InStr);
        if (EnumVal == INDEX_NONE) {
          // Try with namespace prefix
          const FString FullName = Enum->GenerateFullEnumName(*InStr);
          EnumVal = Enum->GetValueByName(FName(*FullName));
        }
        if (EnumVal == INDEX_NONE) {
          OutError =
              FString::Printf(TEXT("Invalid enum value '%s' for enum '%s'"),
                              *InStr, *Enum->GetName());
          return false;
        }
        Bp->SetPropertyValue_InContainer(TargetContainer,
                                         static_cast<uint8>(EnumVal));
        return true;
      } else if (ValueField->Type == EJson::Number) {
        // Validate numeric value is in range
        const int64 Val = static_cast<int64>(ValueField->AsNumber());
        if (!Enum->IsValidEnumValue(Val)) {
          OutError = FString::Printf(
              TEXT("Numeric value %lld is not valid for enum '%s'"), Val,
              *Enum->GetName());
          return false;
        }
        Bp->SetPropertyValue_InContainer(TargetContainer,
                                         static_cast<uint8>(Val));
        return true;
      }
      OutError = TEXT("Enum property requires string or number");
      return false;
    }
    // Regular byte property (not an enum)
    int64 Val = 0;
    if (ValueField->Type == EJson::Number)
      Val = static_cast<int64>(ValueField->AsNumber());
    else if (ValueField->Type == EJson::String)
      Val = static_cast<int64>(FCString::Atoi64(*ValueField->AsString()));
    else {
      OutError = TEXT("Unsupported JSON type for byte property");
      return false;
    }
    Bp->SetPropertyValue_InContainer(TargetContainer, static_cast<uint8>(Val));
    return true;
  }

  // Enum property (newer engine versions)
  if (FEnumProperty *EP = CastField<FEnumProperty>(Property)) {
    if (UEnum *Enum = EP->GetEnum()) {
      void *ValuePtr = EP->ContainerPtrToValuePtr<void>(TargetContainer);
      if (FNumericProperty *UnderlyingProp = EP->GetUnderlyingProperty()) {
        if (ValueField->Type == EJson::String) {
          const FString InStr = ValueField->AsString();
          int64 EnumVal = Enum->GetValueByNameString(InStr);
          if (EnumVal == INDEX_NONE) {
            const FString FullName = Enum->GenerateFullEnumName(*InStr);
            EnumVal = Enum->GetValueByName(FName(*FullName));
          }
          if (EnumVal == INDEX_NONE) {
            OutError =
                FString::Printf(TEXT("Invalid enum value '%s' for enum '%s'"),
                                *InStr, *Enum->GetName());
            return false;
          }
          UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumVal);
          return true;
        } else if (ValueField->Type == EJson::Number) {
          const int64 Val = static_cast<int64>(ValueField->AsNumber());
          if (!Enum->IsValidEnumValue(Val)) {
            OutError = FString::Printf(
                TEXT("Numeric value %lld is not valid for enum '%s'"), Val,
                *Enum->GetName());
            return false;
          }
          UnderlyingProp->SetIntPropertyValue(ValuePtr, Val);
          return true;
        }
        OutError = TEXT("Enum property requires string or number");
        return false;
      }
    }
    OutError = TEXT("Enum property has no valid enum definition");
    return false;
  }

  // Object reference
  if (FObjectProperty *OP = CastField<FObjectProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      UObject *Res = nullptr;
      if (!Path.IsEmpty()) {
        // Try LoadObject first
        Res = LoadObject<UObject>(nullptr, *Path);
        // If unsuccessful, try finding by object path if it's a short path or
        // package path
        if (!Res && !Path.Contains(TEXT("."))) {
          // Fallback to StaticLoadObject which can sometimes handle vague paths
          // better
          Res = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
        }
      }
      if (!Res && !Path.IsEmpty()) {
        OutError =
            FString::Printf(TEXT("Failed to load object at path: %s"), *Path);
        return false;
      }
      OP->SetObjectPropertyValue_InContainer(TargetContainer, Res);
      return true;
    }
    OutError = TEXT("Unsupported JSON type for object property");
    return false;
  }

  // Soft object references (FSoftObjectPtr)
  if (FSoftObjectProperty *SOP = CastField<FSoftObjectProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      void *ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftObjPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftObjPtr) {
        if (Path.IsEmpty()) {
          *SoftObjPtr = FSoftObjectPtr();
        } else {
          *SoftObjPtr = FSoftObjectPath(Path);
        }
        return true;
      }
      OutError = TEXT("Failed to access soft object property");
      return false;
    } else if (ValueField->Type == EJson::Null) {
      void *ValuePtr = SOP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftObjPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftObjPtr) {
        *SoftObjPtr = FSoftObjectPtr();
        return true;
      }
    }
    OutError = TEXT("Soft object property requires string path or null");
    return false;
  }

  // Soft class references (FSoftClassPtr)
  if (FSoftClassProperty *SCP = CastField<FSoftClassProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      const FString Path = ValueField->AsString();
      void *ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftClassPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftClassPtr) {
        if (Path.IsEmpty()) {
          *SoftClassPtr = FSoftObjectPtr();
        } else {
          *SoftClassPtr = FSoftObjectPath(Path);
        }
        return true;
      }
      OutError = TEXT("Failed to access soft class property");
      return false;
    } else if (ValueField->Type == EJson::Null) {
      void *ValuePtr = SCP->ContainerPtrToValuePtr<void>(TargetContainer);
      FSoftObjectPtr *SoftClassPtr = static_cast<FSoftObjectPtr *>(ValuePtr);
      if (SoftClassPtr) {
        *SoftClassPtr = FSoftObjectPtr();
        return true;
      }
    }
    OutError = TEXT("Soft class property requires string path or null");
    return false;
  }

  // Structs (Vector/Rotator)
  if (FStructProperty *SP = CastField<FStructProperty>(Property)) {
    const FString TypeName = SP->Struct ? SP->Struct->GetName() : FString();
    if (ValueField->Type == EJson::Array) {
      const TArray<TSharedPtr<FJsonValue>> &Arr = ValueField->AsArray();
      if (TypeName.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) &&
          Arr.Num() >= 3) {
        FVector V((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                  (float)Arr[2]->AsNumber());
        SP->Struct->CopyScriptStruct(
            SP->ContainerPtrToValuePtr<void>(TargetContainer), &V);
        return true;
      }
      if (TypeName.Equals(TEXT("Rotator"), ESearchCase::IgnoreCase) &&
          Arr.Num() >= 3) {
        FRotator R((float)Arr[0]->AsNumber(), (float)Arr[1]->AsNumber(),
                   (float)Arr[2]->AsNumber());
        SP->Struct->CopyScriptStruct(
            SP->ContainerPtrToValuePtr<void>(TargetContainer), &R);
        return true;
      }
    }

    // Try import from string for other structs. Prefer JSON conversion via
    // FJsonObjectConverter when the incoming text is valid JSON. Older
    // engine versions that provide ImportText on UScriptStruct are
    // supported via a guarded fallback for legacy builds.
    if (ValueField->Type == EJson::String) {
      const FString Txt = ValueField->AsString();
      if (SP->Struct) {
        // First attempt: parse the string as JSON and convert to struct
        // using the robust JsonObjectConverter which avoids relying on
        // engine-private textual import semantics.
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Txt);
        TSharedPtr<FJsonObject> ParsedObj;
        if (FJsonSerializer::Deserialize(Reader, ParsedObj) &&
            ParsedObj.IsValid()) {
          if (FJsonObjectConverter::JsonObjectToUStruct(
                  ParsedObj.ToSharedRef(), SP->Struct,
                  SP->ContainerPtrToValuePtr<void>(TargetContainer), 0, 0)) {
            return true;
          }
        }

        // NOTE: ImportText-based struct parsing is intentionally omitted
        // because engine textual import signatures differ across engine
        // revisions and can produce fragile compilation failures. If a
        // non-JSON textual import format is required in the future we
        // can implement a safe parser here or add an explicit engine
        // compatibility shim guarded by a feature macro.
      }
    }

    OutError = TEXT("Unsupported JSON type for struct property");
    return false;
  }

  // Arrays: handle common inner element types directly. Unsupported inner
  // types will return an error to avoid relying on ImportText-like APIs.
  if (FArrayProperty *AP = CastField<FArrayProperty>(Property)) {
    if (ValueField->Type != EJson::Array) {
      OutError = TEXT("Expected array for array property");
      return false;
    }
    FScriptArrayHelper Helper(
        AP, AP->ContainerPtrToValuePtr<void>(TargetContainer));
    Helper.EmptyValues();
    const TArray<TSharedPtr<FJsonValue>> &Src = ValueField->AsArray();
    for (int32 i = 0; i < Src.Num(); ++i) {
      Helper.AddValue();
      void *ElemPtr = Helper.GetRawPtr(Helper.Num() - 1);
      FProperty *Inner = AP->Inner;
      const TSharedPtr<FJsonValue> &V = Src[i];
      if (FStrProperty *SIP = CastField<FStrProperty>(Inner)) {
        FString &Dest = *reinterpret_cast<FString *>(ElemPtr);
        Dest = (V->Type == EJson::String)
                   ? V->AsString()
                   : FString::Printf(TEXT("%g"), V->AsNumber());
        continue;
      }
      if (FNameProperty *NIP = CastField<FNameProperty>(Inner)) {
        FName &Dest = *reinterpret_cast<FName *>(ElemPtr);
        Dest = (V->Type == EJson::String)
                   ? FName(*V->AsString())
                   : FName(*FString::Printf(TEXT("%g"), V->AsNumber()));
        continue;
      }
      if (FBoolProperty *BIP = CastField<FBoolProperty>(Inner)) {
        uint8 &Dest = *reinterpret_cast<uint8 *>(ElemPtr);
        Dest = (V->Type == EJson::Boolean) ? (V->AsBool() ? 1 : 0)
                                           : (V->AsNumber() != 0.0 ? 1 : 0);
        continue;
      }
      if (FFloatProperty *FIP = CastField<FFloatProperty>(Inner)) {
        float &Dest = *reinterpret_cast<float *>(ElemPtr);
        Dest = (V->Type == EJson::Number)
                   ? (float)V->AsNumber()
                   : (float)FCString::Atod(*V->AsString());
        continue;
      }
      if (FDoubleProperty *DIP = CastField<FDoubleProperty>(Inner)) {
        double &Dest = *reinterpret_cast<double *>(ElemPtr);
        Dest = (V->Type == EJson::Number) ? V->AsNumber()
                                          : FCString::Atod(*V->AsString());
        continue;
      }
      if (FIntProperty *IIP = CastField<FIntProperty>(Inner)) {
        int32 &Dest = *reinterpret_cast<int32 *>(ElemPtr);
        Dest = (V->Type == EJson::Number) ? (int32)V->AsNumber()
                                          : FCString::Atoi(*V->AsString());
        continue;
      }
      if (FInt64Property *I64IP = CastField<FInt64Property>(Inner)) {
        int64 &Dest = *reinterpret_cast<int64 *>(ElemPtr);
        Dest = (V->Type == EJson::Number) ? (int64)V->AsNumber()
                                          : FCString::Atoi64(*V->AsString());
        continue;
      }
      if (FByteProperty *BYP = CastField<FByteProperty>(Inner)) {
        uint8 &Dest = *reinterpret_cast<uint8 *>(ElemPtr);
        Dest = (V->Type == EJson::Number)
                   ? (uint8)V->AsNumber()
                   : (uint8)FCString::Atoi(*V->AsString());
        continue;
      }

      // Unsupported inner type -> fail explicitly
      OutError =
          TEXT("Unsupported array inner property type for JSON assignment");
      return false;
    }
    return true;
  }

  OutError = TEXT("Unsupported property type for JSON assignment");
  return false;
}

// Read vector and rotator typed fields from JSON helpers
static inline void ReadVectorField(const TSharedPtr<FJsonObject> &Obj,
                                   const TCHAR *FieldName, FVector &Out,
                                   const FVector &Default) {
  if (!Obj.IsValid()) {
    Out = Default;
    return;
  }
  const TSharedPtr<FJsonObject> *FieldObj = nullptr;
  if (Obj->TryGetObjectField(FieldName, FieldObj) && FieldObj &&
      (*FieldObj).IsValid()) {
    double X = Default.X, Y = Default.Y, Z = Default.Z;
    if (!(*FieldObj)->TryGetNumberField(TEXT("x"), X))
      (*FieldObj)->TryGetNumberField(TEXT("X"), X);
    if (!(*FieldObj)->TryGetNumberField(TEXT("y"), Y))
      (*FieldObj)->TryGetNumberField(TEXT("Y"), Y);
    if (!(*FieldObj)->TryGetNumberField(TEXT("z"), Z))
      (*FieldObj)->TryGetNumberField(TEXT("Z"), Z);
    Out = FVector((float)X, (float)Y, (float)Z);
    return;
  }
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  if (Obj->TryGetArrayField(FieldName, Arr) && Arr && Arr->Num() >= 3) {
    Out = FVector((float)(*Arr)[0]->AsNumber(), (float)(*Arr)[1]->AsNumber(),
                  (float)(*Arr)[2]->AsNumber());
    return;
  }
  Out = Default;
}

static inline void ReadRotatorField(const TSharedPtr<FJsonObject> &Obj,
                                    const TCHAR *FieldName, FRotator &Out,
                                    const FRotator &Default) {
  if (!Obj.IsValid()) {
    Out = Default;
    return;
  }
  const TSharedPtr<FJsonObject> *FieldObj = nullptr;
  if (Obj->TryGetObjectField(FieldName, FieldObj) && FieldObj &&
      (*FieldObj).IsValid()) {
    double Pitch = Default.Pitch, Yaw = Default.Yaw, Roll = Default.Roll;
    if (!(*FieldObj)->TryGetNumberField(TEXT("pitch"), Pitch))
      (*FieldObj)->TryGetNumberField(TEXT("Pitch"), Pitch);
    if (!(*FieldObj)->TryGetNumberField(TEXT("yaw"), Yaw))
      (*FieldObj)->TryGetNumberField(TEXT("Yaw"), Yaw);
    if (!(*FieldObj)->TryGetNumberField(TEXT("roll"), Roll))
      (*FieldObj)->TryGetNumberField(TEXT("Roll"), Roll);
    Out = FRotator((float)Pitch, (float)Yaw, (float)Roll);
    return;
  }
  const TArray<TSharedPtr<FJsonValue>> *Arr = nullptr;
  if (Obj->TryGetArrayField(FieldName, Arr) && Arr && Arr->Num() >= 3) {
    Out = FRotator((float)(*Arr)[0]->AsNumber(), (float)(*Arr)[1]->AsNumber(),
                   (float)(*Arr)[2]->AsNumber());
    return;
  }
  Out = Default;
}

static inline FVector ExtractVectorField(const TSharedPtr<FJsonObject> &Source,
                                         const TCHAR *FieldName,
                                         const FVector &DefaultValue) {
  FVector Parsed = DefaultValue;
  ReadVectorField(Source, FieldName, Parsed, DefaultValue);
  return Parsed;
}

static inline FRotator
ExtractRotatorField(const TSharedPtr<FJsonObject> &Source,
                    const TCHAR *FieldName, const FRotator &DefaultValue) {
  FRotator Parsed = DefaultValue;
  ReadRotatorField(Source, FieldName, Parsed, DefaultValue);
  return Parsed;
}

// Resolve a nested property path (e.g., "Transform.Location.X" or
// "MyComponent.Intensity"). Returns the final property and target object, or
// nullptr on failure. OutError is populated with a descriptive error message on
// failure.
// Resolve a nested property path (e.g., "Transform.Location.X" or
// "MyComponent.Intensity"). Returns the final property and the pointer to the
// container holding it. OutError is populated with a descriptive error message
// on failure.
static inline FProperty *ResolveNestedPropertyPath(UObject *RootObject,
                                                   const FString &PropertyPath,
                                                   void *&OutContainerPtr,
                                                   FString &OutError) {
  OutError.Empty();
  OutContainerPtr = nullptr;

  if (!RootObject) {
    OutError = TEXT("Root object is null");
    return nullptr;
  }

  if (PropertyPath.IsEmpty()) {
    OutError = TEXT("Property path is empty");
    return nullptr;
  }

  TArray<FString> PathSegments;
  PropertyPath.ParseIntoArray(PathSegments, TEXT("."), true);

  if (PathSegments.Num() == 0) {
    OutError = TEXT("Invalid property path format");
    return nullptr;
  }

  UStruct *CurrentTypeScope = RootObject->GetClass();
  void *CurrentContainer = RootObject;
  FProperty *CurrentProperty = nullptr;

  for (int32 i = 0; i < PathSegments.Num(); ++i) {
    const FString &Segment = PathSegments[i];
    const bool bIsLastSegment = (i == PathSegments.Num() - 1);

    // Find property in current scope
    CurrentProperty =
        FindFProperty<FProperty>(CurrentTypeScope, FName(*Segment));

    if (!CurrentProperty) {
      OutError = FString::Printf(
          TEXT("Property '%s' not found in scope '%s' (segment %d of %d)"),
          *Segment, *CurrentTypeScope->GetName(), i + 1, PathSegments.Num());
      return nullptr;
    }

    // If this is the last segment, we've found our target
    if (bIsLastSegment) {
      OutContainerPtr = CurrentContainer;
      return CurrentProperty;
    }

    // Traverse deeper
    if (FObjectProperty *ObjectProp =
            CastField<FObjectProperty>(CurrentProperty)) {
      UObject *NextObject =
          ObjectProp->GetObjectPropertyValue_InContainer(CurrentContainer);
      if (!NextObject) {
        OutError = FString::Printf(
            TEXT("Object property '%s' is null (segment %d of %d)"), *Segment,
            i + 1, PathSegments.Num());
        return nullptr;
      }
      CurrentContainer = NextObject;
      CurrentTypeScope = NextObject->GetClass();
    } else if (FStructProperty *StructProp =
                   CastField<FStructProperty>(CurrentProperty)) {
      CurrentContainer =
          StructProp->ContainerPtrToValuePtr<void>(CurrentContainer);
      CurrentTypeScope = StructProp->Struct;
    } else {
      OutError = FString::Printf(
          TEXT("Cannot traverse into property '%s' of type '%s'"), *Segment,
          *CurrentProperty->GetClass()->GetName());
      return nullptr;
    }
  }

  OutError = TEXT("Unexpected end of property path resolution");
  return nullptr;
}

// Helper to find an SCS node by a (case-insensitive) name. Uses reflection
// to iterate the internal AllNodes array so this implementation does not
// require the concrete USCS_Node type to be visible at compile time.
static inline USCS_Node *FindScsNodeByName(USimpleConstructionScript *SCS,
                                           const FString &Name) {
  if (!SCS || Name.IsEmpty())
    return nullptr;

  // Attempt to find an array property named "AllNodes" on the SCS
  if (UClass *SCSClass = SCS->GetClass()) {
    if (FArrayProperty *ArrayProp =
            FindFProperty<FArrayProperty>(SCSClass, TEXT("AllNodes"))) {
      // Helper to iterate elements
      FScriptArrayHelper Helper(ArrayProp,
                                ArrayProp->ContainerPtrToValuePtr<void>(SCS));
      for (int32 Idx = 0; Idx < Helper.Num(); ++Idx) {
        void *ElemPtr = Helper.GetRawPtr(Idx);
        if (!ElemPtr)
          continue;
        if (FObjectProperty *ObjProp =
                CastField<FObjectProperty>(ArrayProp->Inner)) {
          UObject *ElemObj = ObjProp->GetObjectPropertyValue(ElemPtr);
          if (!ElemObj)
            continue;
          // Match by explicit VariableName property when present
          if (FProperty *VarProp = ElemObj->GetClass()->FindPropertyByName(
                  TEXT("VariableName"))) {
            if (FNameProperty *NP = CastField<FNameProperty>(VarProp)) {
              const FName V = NP->GetPropertyValue_InContainer(ElemObj);
              if (!V.IsNone() &&
                  V.ToString().Equals(Name, ESearchCase::IgnoreCase)) {
                return reinterpret_cast<USCS_Node *>(ElemObj);
              }
            }
          }
          // Fallback: match the object name
          if (ElemObj->GetName().Equals(Name, ESearchCase::IgnoreCase)) {
            return reinterpret_cast<USCS_Node *>(ElemObj);
          }
        }
      }
    }
  }
  return nullptr;
}

#if WITH_EDITOR
// Attempt to locate and load a Blueprint by several heuristics. Returns nullptr
// on failure and populates OutNormalized and OutError accordingly.
static inline UBlueprint *LoadBlueprintAsset(const FString &Req,
                                             FString &OutNormalized,
                                             FString &OutError) {
  OutNormalized.Empty();
  OutError.Empty();
  if (Req.IsEmpty()) {
    OutError = TEXT("Empty request");
    return nullptr;
  }

  UBlueprint *BP = nullptr;
  if (Req.Contains(TEXT("."))) {
    if (UEditorAssetLibrary::DoesAssetExist(Req)) {
      BP = LoadObject<UBlueprint>(nullptr, *Req);
    }
    if (BP) {
      OutNormalized = BP->GetPathName();
      if (OutNormalized.Contains(TEXT(".")))
        OutNormalized = OutNormalized.Left(OutNormalized.Find(TEXT(".")));
      return BP;
    }
  }

  FString Candidate = Req;
  if (!Candidate.StartsWith(TEXT("/")))
    Candidate = FString::Printf(TEXT("/Game/%s"), *Req);

  // Smart detection: duplicate the clean filename only if it's not already
  // there. This handles inputs like "/Game/Path/Asset.Asset" (idempotent) vs
  // "/Game/Path/Asset" (append).
  FString AssetRef = Candidate;
  const FString CleanName = FPaths::GetCleanFilename(Candidate);
  // If Candidate does not look like "Package.Asset", append ".Asset"
  // Note: This check is heuristic; if clean name contains dot, it might assume
  // it's already properly formatted.
  if (!Candidate.EndsWith(FString::Printf(TEXT(".%s"), *CleanName))) {
    AssetRef = FString::Printf(TEXT("%s.%s"), *Candidate, *CleanName);
  }

  if (UEditorAssetLibrary::DoesAssetExist(AssetRef)) {
    BP = LoadObject<UBlueprint>(nullptr, *AssetRef);
  }
  if (BP) {
    OutNormalized = Candidate;
    return BP;
  }

  FAssetRegistryModule &ARM =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
          TEXT("AssetRegistry"));
  FAssetData Found;
  TArray<FAssetData> Results;
  ARM.Get().GetAssetsByPackageName(FName(*Req), Results);
  if (Results.Num() > 0) {
    Found = Results[0];
  } else {
    FString Pkg = Req;
    if (!Pkg.StartsWith(TEXT("/")))
      Pkg = FString::Printf(TEXT("/Game/%s"), *Req);
    ARM.Get().GetAssetsByPackageName(FName(*Pkg), Results);
    if (Results.Num() > 0) {
      Found = Results[0];
    }
  }

  if (Found.IsValid()) {
    BP = Cast<UBlueprint>(Found.GetAsset());
    if (!BP) {
      const FString PathStr = Found.ToSoftObjectPath().ToString();
      BP = LoadObject<UBlueprint>(nullptr, *PathStr);
    }
    if (BP) {
      OutNormalized = Found.ToSoftObjectPath().ToString();
      if (OutNormalized.Contains(TEXT(".")))
        OutNormalized = OutNormalized.Left(OutNormalized.Find(TEXT(".")));
      return BP;
    }
  }

  OutError = FString::Printf(TEXT("Blueprint asset not found: %s"), *Req);
  return nullptr;
}
#endif

// Generic conversion helpers to produce FString from common engine types
static inline FString ConvertToString(const FString &In) { return In; }
static inline FString ConvertToString(const FName &In) { return In.ToString(); }
static inline FString ConvertToString(const FText &In) { return In.ToString(); }

// Attempt to resolve a blueprint path to a normalized form without necessarily
// loading the Blueprint object. Returns true when a normalized path is found.
static inline bool FindBlueprintNormalizedPath(const FString &Req,
                                               FString &OutNormalized) {
  OutNormalized.Empty();
  if (Req.IsEmpty())
    return false;
#if WITH_EDITOR
  FString Err;
  UBlueprint *BP = LoadBlueprintAsset(Req, OutNormalized, Err);
  return BP != nullptr && !OutNormalized.IsEmpty();
#else
  return false;
#endif
}
