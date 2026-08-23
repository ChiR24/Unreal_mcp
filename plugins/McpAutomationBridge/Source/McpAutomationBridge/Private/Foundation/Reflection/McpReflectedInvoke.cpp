#include "Foundation/Reflection/McpReflectedInvoke.h"

#if WITH_EDITOR
#include "JsonObjectConverter.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

FMcpScopedParamBlock::FMcpScopedParamBlock(UFunction *InFunction)
    : Function(InFunction),
      Memory(static_cast<uint8 *>(
          FMemory::Malloc(FMath::Max<int32>(InFunction->ParmsSize, 1), 16))) {
  FMemory::Memzero(Memory, FMath::Max<int32>(Function->ParmsSize, 1));
  for (TFieldIterator<FProperty> It(Function);
       It && It->HasAnyPropertyFlags(CPF_Parm); ++It) {
    It->InitializeValue_InContainer(Memory);
  }
}

FMcpScopedParamBlock::~FMcpScopedParamBlock() {
  for (TFieldIterator<FProperty> It(Function);
       It && It->HasAnyPropertyFlags(CPF_Parm); ++It) {
    It->DestroyValue_InContainer(Memory);
  }
  FMemory::Free(Memory);
}

bool McpBindJsonArgsToParams(UFunction *Function,
                             const TSharedPtr<FJsonObject> &Args,
                             uint8 *ParamBlock,
                             TArray<TSharedPtr<FJsonValue>> &OutUnset,
                             FString &OutError) {
  if (Function == nullptr || ParamBlock == nullptr) {
    OutError = TEXT("No function or parameter block to bind.");
    return false;
  }

  for (TFieldIterator<FProperty> It(Function);
       It && It->HasAnyPropertyFlags(CPF_Parm); ++It) {
    FProperty *Property = *It;
    // Return values and pure out parameters are produced by the call, not
    // supplied to it, so they are never sourced from the caller's arguments.
    if (Property->HasAnyPropertyFlags(CPF_ReturnParm)) {
      continue;
    }
    if (Property->HasAnyPropertyFlags(CPF_OutParm) &&
        !Property->HasAnyPropertyFlags(CPF_ReferenceParm)) {
      continue;
    }

    const TSharedPtr<FJsonValue> Value =
        Args.IsValid() ? Args->TryGetField(Property->GetName()) : nullptr;
    if (!Value.IsValid()) {
      OutUnset.Add(MakeShared<FJsonValueString>(Property->GetName()));
      continue;
    }
    if (!FJsonObjectConverter::JsonValueToUProperty(
            Value, Property, Property->ContainerPtrToValuePtr<void>(ParamBlock),
            0, 0)) {
      OutError = FString::Printf(TEXT("Could not convert argument '%s' to %s."),
                                 *Property->GetName(), *Property->GetCPPType());
      return false;
    }
  }
  return true;
}

TSharedPtr<FJsonObject> McpReadParamOutputs(UFunction *Function,
                                            const uint8 *ParamBlock) {
  TSharedPtr<FJsonObject> Outputs = MakeShared<FJsonObject>();
  if (Function == nullptr || ParamBlock == nullptr) {
    return Outputs;
  }

  for (TFieldIterator<FProperty> It(Function);
       It && It->HasAnyPropertyFlags(CPF_Parm); ++It) {
    FProperty *Property = *It;
    if (!Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm)) {
      continue;
    }
    const TSharedPtr<FJsonValue> Value =
        FJsonObjectConverter::UPropertyToJsonValue(
            Property,
            Property->ContainerPtrToValuePtr<void>(
                const_cast<uint8 *>(ParamBlock)),
            0, 0);
    if (Value.IsValid()) {
      Outputs->SetField(Property->GetName(), Value);
    }
  }
  return Outputs;
}
#endif
