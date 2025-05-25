// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TurboSequence_MinimalData_Lf.h"
#include "UObject/Object.h"
#include "TurboSequence_RenderData.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class UTurboSequence_MeshAsset_Lf;
struct FSkinnedMeshRuntime_Lf;

UCLASS()
class TURBOSEQUENCE_LF_API UTurboSequence_RenderData : public UObject
{
	GENERATED_BODY()

public: 

	static UTurboSequence_RenderData* CreateObject(UObject* Outer, UTurboSequence_MeshAsset_Lf* InMeshAsset, const UStaticMesh* StaticMesh);
	bool GetInstanceCustomDataBaseIndex(const FAttachmentMeshHandle MeshHandle, const int32 CustomDataIndex,
	                                    int32& InstanceBaseIndex);
	void SetCustomDataIndex(float CustomDataValue, uint32 CustomDataIndex);

	virtual void PrintRenderData() const;

	UPROPERTY()
	UTurboSequence_MeshAsset_Lf *DataAsset = nullptr;

	FName& GetEmitterName() const;

	FName& GetPositionName() const;

	FName& GetRotationName() const;

	FName& GetScaleName() const;

	FString& GetMeshName() const;

	FString& GetMaterialsName() const;

	FName& GetFlagsName() const;

	FName& GetCustomDataName() const;

	int32 GetActiveNum() const 
	{
		return InstanceMap.Num();
	}

	void ResetBounds ();
	FBox GetRenderBounds() const;
	void UpdateNiagaraBounds() const;

	virtual void UpdateNiagaraEmitter();

	void SetEmitterBounds(const FBox& RendererBounds) const;


	void SpawnNiagaraComponent(const TArray<UMaterialInterface*>& OverrideMaterials,
	                           USceneComponent* AttachToComponent,
	                           UNiagaraSystem
	                           * RendererSystem, UStaticMesh* StaticMesh, bool bNewReceivesDecals,
	                           FLightingChannels LightingChannels, bool bInRenderInCustomDepth = false, int32 InStencilValue = 0);

	void DestroyNiagaraComponent();
	
protected:
	void UpdateInstanceTransformInternal(int32 InstanceIndex, const FTransform& WorldSpaceTransform, bool bForceUpdate = false);

public:
	/**
	* @brief Updates one instance in an instanced static mesh component
	* @param AttachmentMeshHandle
	* @param WorldSpaceTransform The transform matrix we like to update
	*/
	void UpdateInstanceTransform(FAttachmentMeshHandle AttachmentMeshHandle,
	                                        const FTransform& WorldSpaceTransform);

protected:
	int32 AllocateRenderInstanceInternal(FAttachmentMeshHandle MeshHandle, bool& bReuse);
	void SetSkeletonIndexInternal(int32 InstanceIndex, int32 SkeletonIndex);

public:
	/**
	* @brief Adds an instance to the given renderer with custom data
	* @param MeshHandle
	* @param WorldSpaceTransform
	* @param SkeletonIndex
	*/
	void AddRenderInstance(const FAttachmentMeshHandle MeshHandle,
	                       const FTransform& WorldSpaceTransform, int32 SkeletonIndex);
	


	void UpdateRendererBounds(
		const FTransform& WorldSpaceTransform);

	/**
	 * Updates the custom data for a specific instance in the skinned mesh reference.
	 *
	 * @param MeshHandle
	 * @param Index The index for custom data.
	 * @param CustomDataValue The value to set for the custom data.
	 *
	 * @return true if the custom data was successfully set, false otherwise.
	 *
	 * @throws None
	 */
	bool SetCustomDataForInstance(
		FAttachmentMeshHandle MeshHandle, const int32 Index, float Value);

	bool SetCustomDataArrayForInstance(
		FAttachmentMeshHandle MeshHandle, const int32 StartIndex, const TArray<float>& CustomDataValues);

	
	/**
	 * @brief Removes an instance from the renderer
	 * @param Handle
	 */
	void RemoveRenderInstance(
		FAttachmentMeshHandle Handle);

	const FVector& GetMeshMinBounds() const {return MeshMinBounds;}
	const FVector& GetMeshMaxBounds() const {return MeshMaxBounds;}

protected:
	void SetGPUBoneIndexForInstanceInternal(uint8 BoneIndex, int Index);
	static void Init(UTurboSequence_MeshAsset_Lf* InMeshAsset,
	                 UTurboSequence_RenderData* TurboSequenceRenderData, const FVector& MeshMinBounds, const FVector& MeshMaxBounds);

	
	UPROPERTY()
	UNiagaraComponent* NiagaraComponent;

private:
	
	// ID
	TMap<FAttachmentMeshHandle, int32> InstanceMap; // < MeshID | Renderer Instance Index >

	TArray<int32> FreeList;

	// Transform
	TArray<FVector> ParticlePositions;
	TArray<FVector3f> ParticleScales;
	TArray<FVector4f> ParticleRotations;

	TArray<int32> SkeletonIndexes;
	
	// Flags
	TArray<uint8> ParticleFlags; 

	// Custom Data
	TArray<float> ParticleCustomData;

	// Bounds Checking
	FVector MinBounds = FVector(TNumericLimits<double>::Max());
	FVector MaxBounds = FVector(TNumericLimits<double>::Lowest());
	float MaxScale = 0;
	FVector MeshMinBounds;
	FVector MeshMaxBounds;
	
	bool bChangedPositionCollectionThisFrame = false;
	bool bChangedRotationCollectionThisFrame = false;
	bool bChangedScaleCollectionThisFrame = false;
	bool bChangedFlagsCollectionThisFrame = false;
	bool bChangedCustomDataCollectionThisFrame = false;
	bool bChangedSkeletonIndexesThisFrame = false;

protected:

	bool bChangedCollectionSizeThisFrame = false;
	
};
