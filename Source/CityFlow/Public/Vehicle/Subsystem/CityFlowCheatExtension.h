#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "CityFlowCheatExtension.generated.h"

UCLASS()
class CITYFLOW_API UCityFlowCheatExtension : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_StartSimulation();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_EndSimulation();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_RestartPlanning();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_TriggerLSystem();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_SpawnVehicle();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_ClearVehicles();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_TogglePathDebug();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_ToggleIntersectionDebug();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_ToggleCongestionDebug();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_SetBudget(int32 Amount);

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_AddBudget(int32 Amount);

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_ShowGridStats();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_ShowVehicleStats();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_ShowScoreStats();

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_AddScore(int32 Amount);

	UFUNCTION(Exec, Category = "CityFlow|Debug")
	void CF_SetSimulationSpeed(float Speed);

private:
	class ACityFlowGameMode* GetGameMode() const;
	class UGridManager* GetGridManager() const;
	class UVehicleManager* GetVehicleManager() const;
	class UScoringManager* GetScoringManager() const;
	class ULSystemManager* GetLSystemManager() const;
};
