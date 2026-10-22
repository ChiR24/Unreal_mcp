#pragma once

#include "PCGSettings.h"
#include "Elements/Blueprint/PCGBlueprintBaseElement.h"
#include "PCGValidationElements.generated.h"

class UPCGComponent;

UCLASS()
class PCGVALIDATION_API UPCGValidationDelaySettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
    double DelaySeconds = 2.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
    bool bCreateTransientChild = true;

    UFUNCTION(BlueprintCallable, Category = "Validation")
    static int64 ReplaceInFlightGeneration(UPCGComponent* Component);

protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override;
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
    virtual FPCGElementPtr CreateElement() const override;
};

UCLASS(BlueprintType, Blueprintable)
class PCGVALIDATION_API UPCGValidationBlueprintElement : public UPCGBlueprintBaseElement
{
    GENERATED_BODY()
public:
    UPCGValidationBlueprintElement();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Validation")
    FString OutputTag = TEXT("MCP_CustomNode");

    virtual void Execute_Implementation(const FPCGDataCollection& Input, FPCGDataCollection& Output) override;
};
