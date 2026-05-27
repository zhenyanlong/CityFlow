#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CityFlowPawn.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS()
class CITYFLOW_API ACityFlowPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ACityFlowPawn();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const struct FInputActionValue& Value);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeed = 1500.0f;
};
