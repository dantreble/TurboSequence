// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TurboSequence_MeshAsset_Lf.generated.h"

class UNiagaraSystem;
class UTurboSequence_GlobalData_Lf;
/**
 * 
 */
UCLASS(BlueprintType)
class TURBOSEQUENCE_LF_API UTurboSequence_MeshAsset_Lf : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTurboSequence_MeshAsset_Lf();

	/*	==============================================================================================================
												GAMEPLAY
	==============================================================================================================	*/
	UPROPERTY(EditAnywhere, Category="Global")
	bool bExcludeFromSystem = false;

	UPROPERTY(EditAnywhere, Category="Global")
	TObjectPtr<UTurboSequence_GlobalData_Lf> GlobalData;

	UPROPERTY(EditAnywhere, Category="Reference")
	TObjectPtr<UNiagaraSystem> RendererSystem;

	UPROPERTY(EditAnywhere, Category="Reference")
	TObjectPtr<USkeletalMesh> ReferenceMeshNative;

	UPROPERTY(EditAnywhere, Category="Reference", meta=(GetOptions="GetSocketNames"))
	TArray<FName> KeepSockets;
	
	UPROPERTY(EditAnywhere, Category="Optimization",
		meta=(ClampMin = "0.001", ClampMax = "0.2", ToolTip=
			"Turbo Sequence makes linear Keyframe Reduction, 1 Keyframe happens in this interval, ( Quality | Memory Usage ) <- -> ( Low Memory Usage )"
		))
	// Turbo Sequence makes linear Keyframe Reduction, 1 Keyframe happens in this interval, ( Quality | Memory Usage ) <- -> ( Low Memory Usage )
	float TimeBetweenAnimationLibraryFrames = 0.05f;
	
	UPROPERTY(EditAnywhere, Category="Instance",
		meta=(ToolTip=
			"The baked Static Mesh for this asset, right click to bake/update"
		))
	TObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, Meta=(ToolTip="Turbo sequence materials"))
	TArray<TObjectPtr<class UMaterialInterface>> Materials;
	
	UPROPERTY(VisibleAnywhere)
	TMap<int32, int32> CPUBoneToGPUBoneIndicesMap;

	//The local cache (slot/line) where we will read our parent bone from
	//INDEX_NONE no parent (reset), 0 previous bone parent, 1 and beyond local bone cache
	UPROPERTY(VisibleAnywhere)
	TArray<int32> GPUParentCacheRead;

	//INDEX_NONE no write, 0 nothing, 1 and beyond local bone cache
	UPROPERTY(VisibleAnywhere)
	TArray<int32> GPUParentCacheWrite;

	UPROPERTY(EditAnywhere, Category="Settings",
		meta=(ToolTip= "Does the Mesh needs to be enabled for frustum culling, it makes sense to not cull the LOD 0 because of the shadows"))
	// Does the Mesh needs to be enabled for frustum culling, it makes sense to not cull the LOD 0 because of the shadows
	bool bIsFrustumCullingEnabled = true;

	UPROPERTY(EditAnywhere, Category="Animation")
	TObjectPtr<UAnimSequence> OverrideDefaultAnimation;
	
	const FReferenceSkeleton& GetReferenceSkeleton() const
	{
		return ReferenceMeshNative->GetRefSkeleton();
	}

	TObjectPtr<USkeleton> GetSkeleton() const
	{
		return ReferenceMeshNative->GetSkeleton();
	}

	int32 GetNumCPUBones() const
	{
		if(ReferenceMeshNative != nullptr)
		{
			return ReferenceMeshNative->GetRefSkeleton().GetNum();
		}

		return 0;
	}

	int32 GetNumGPUBones() const
	{
		return CPUBoneToGPUBoneIndicesMap.Num();
	}

	int32 GetBoneCPUIndex(const FName BoneName) const
	{
		const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton();

		return ReferenceSkeleton.FindBoneIndex(BoneName);
	}
	
	int32 GetBoneGPUIndex(const int32 CPUIndex) const
	{
		if(CPUIndex != INDEX_NONE)
		{
			if(const int* GPUIndex = CPUBoneToGPUBoneIndicesMap.Find(CPUIndex))
			{
				return *GPUIndex;
			}
		}

		return INDEX_NONE;
	}
	
	int32 GetBoneGPUIndex(const FName BoneName) const 
	{
		return GetBoneGPUIndex(GetBoneCPUIndex(BoneName));
	}
	
	bool IsMeshAssetValid() const;

	UFUNCTION()
	TArray<FString> GetSocketNames() const;

	void PrecachePSOs();
	
	virtual void PostLoad() override;
};
