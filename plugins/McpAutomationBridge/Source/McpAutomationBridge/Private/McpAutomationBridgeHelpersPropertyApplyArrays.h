#pragma once

static inline bool ApplyJsonArrayValueToProperty(void *TargetContainer, FProperty *Property,
                                                 const TSharedPtr<FJsonValue> &ValueField,
                                                 FString &OutError) {
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

      FString InnerError;
      if (ApplyJsonValueToProperty(ElemPtr, Inner, V, InnerError)) {
        continue;
      }

      // Unsupported inner type -> fail explicitly
      OutError =
          TEXT("Unsupported array inner property type for JSON assignment");
      return false;
    }
    return true;
  }
  return false;
}
