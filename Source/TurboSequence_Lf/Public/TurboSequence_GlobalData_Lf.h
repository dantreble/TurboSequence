﻿// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TurboSequence_ComputeShaders_Lf.h"

#include "TurboSequence_GlobalData_Lf.generated.h"

class UTextureRenderTarget2DArray;
/**
 * 
 */
UCLASS()
class TURBOSEQUENCE_LF_API UTurboSequence_GlobalData_Lf : public UObject
{
public:
	GENERATED_BODY()

	UTurboSequence_GlobalData_Lf();

	UPROPERTY(EditAnywhere)
	FName NameNiagaraEmitter = FName("FXE_TurboSequence_Mesh_Unit_Lf");

	UPROPERTY(EditAnywhere)
	FString NameNiagaraMeshObject = FString("User.Mesh_{0}");

	UPROPERTY(EditAnywhere)
	FString NameNiagaraMaterialObject = FString("User.Material_{0}");

	UPROPERTY(EditAnywhere)
	FName NameNiagaraParticleLocations = FName("User.Particle_Position");
	
	UPROPERTY(EditAnywhere)
	FName NameNiagaraParticleRotations = FName("User.Particle_Rotation");

	UPROPERTY(EditAnywhere)
	FName NameNiagaraParticleScales = FName("User.Particle_Scale");

	UPROPERTY(EditAnywhere)
	FName NameNiagaraFlags = FName("User.Particle_Flags");
	
	UPROPERTY(EditAnywhere)
	FName NameNiagaraCustomData = FName("User.CustomData");

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTextureRenderTarget2DArray> TransformTexture_CurrentFrame;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UTextureRenderTarget2DArray> TransformTexture_PreviousFrame;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UTextureRenderTarget2DArray> AnimationLibraryTexture;

	UPROPERTY(EditAnywhere)
	bool bUseHighPrecisionAnimationMode = true;

	FSettingsComputeShader_Params_Lf CachedMeshDataCreationSettingsParams;
};
