#pragma once

static inline bool ApplyJsonScalarValueToProperty(void *TargetContainer, FProperty *Property,
                                                  const TSharedPtr<FJsonValue> &ValueField,
                                                  FString &OutError) {
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
  if (FTextProperty *TP = CastField<FTextProperty>(Property)) {
    if (ValueField->Type == EJson::String) {
      TP->SetPropertyValue_InContainer(
          TargetContainer,
          FTextStringHelper::CreateFromBuffer(*ValueField->AsString()));
      return true;
    }
    OutError = TEXT("Expected string for text property");
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
  return false;
}
