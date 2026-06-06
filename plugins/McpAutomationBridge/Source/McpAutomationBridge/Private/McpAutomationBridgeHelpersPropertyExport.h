#pragma once

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

  if (FTextProperty *TP = CastField<FTextProperty>(Property)) {
    const FText TextValue = TP->GetPropertyValue_InContainer(TargetContainer);
    if (TextValue.IsCultureInvariant() && !TextValue.IsFromStringTable()) {
      return MakeShared<FJsonValueString>(TextValue.ToString());
    }
    FString ExportedText;
    FTextStringHelper::WriteToBuffer(ExportedText, TextValue);
    return MakeShared<FJsonValueString>(ExportedText);
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
    FScriptArrayHelper Helper(
        AP, AP->ContainerPtrToValuePtr<void>(TargetContainer));
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
        if (FTextProperty *TextInner = CastField<FTextProperty>(Inner)) {
          const FText TextValue = TextInner->GetPropertyValue(ElemPtr);
          if (TextValue.IsCultureInvariant() && !TextValue.IsFromStringTable()) {
            Out.Add(MakeShared<FJsonValueString>(TextValue.ToString()));
            continue;
          }
          FString ExportedText;
          FTextStringHelper::WriteToBuffer(ExportedText, TextValue);
          Out.Add(MakeShared<FJsonValueString>(ExportedText));
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

        // Fallback: use version-compatible export for unsupported inner types.
        FString ElemStr;
        MCP_PROPERTY_EXPORT_TEXT(Inner, ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
        Out.Add(MakeShared<FJsonValueString>(ElemStr));
      }
    }
    return MakeShared<FJsonValueArray>(Out);
  }

  // Maps: export as JSON object with key-value pairs
  if (FMapProperty *MP = CastField<FMapProperty>(Property)) {
    TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
    FScriptMapHelper Helper(MP,
                            MP->ContainerPtrToValuePtr<void>(TargetContainer));

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
        // Use version-compatible export for unsupported value types.
        FString ValueStr;
        MCP_PROPERTY_EXPORT_TEXT(ValueProp, ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
        MapObj->SetStringField(KeyStr, ValueStr);
      }
    }

    return MakeShared<FJsonValueObject>(MapObj);
  }

  // Sets: export as JSON array
  if (FSetProperty *SP = CastField<FSetProperty>(Property)) {
    TArray<TSharedPtr<FJsonValue>> Out;
    FScriptSetHelper Helper(SP,
                            SP->ContainerPtrToValuePtr<void>(TargetContainer));

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
        // Use version-compatible export for unsupported set element types.
        FString ElemStr;
        MCP_PROPERTY_EXPORT_TEXT(ElemProp, ElemStr, ElemPtr, nullptr, nullptr, PPF_None);
        Out.Add(MakeShared<FJsonValueString>(ElemStr));
      }
    }

    return MakeShared<FJsonValueArray>(Out);
  }

  return nullptr;
}
