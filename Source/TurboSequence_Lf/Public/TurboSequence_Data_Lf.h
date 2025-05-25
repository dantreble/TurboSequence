// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TurboSequence_ComputeShaders_Lf.h"
#include "TurboSequence_Helper_Lf.h"
#include "TurboSequence_MinimalData_Lf.h"
#include "BestFitAllocator.h"
#include "TurboSequence_RenderData.h"


#include "TurboSequence_Data_Lf.generated.h"

struct FSkinnedMeshGlobalLibrary_Lf;
struct FSkinnedMeshRuntime_Lf;
class UNiagaraComponent;

/*	==============================================================================================================
												RENDERING
	==============================================================================================================	*/

USTRUCT()
struct TURBOSEQUENCE_LF_API FCameraView_Lf
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Aspect")
	FVector2f ViewportSize = FVector2f::ZeroVector;

	TOptional<EAspectRatioAxisConstraint> AspectRatioAxisConstraint;

	UPROPERTY(EditAnywhere, Category="Aspect")
	float Fov = 60;

	UPROPERTY(EditAnywhere, Category="Aspect")
	float NearClipPlane = 1;

	UPROPERTY(EditAnywhere, Category="Aspect")
	float FarClipPlane = 1000;

	UPROPERTY(EditAnywhere, Category="Aspect")
	bool bIsPerspective = true;

	UPROPERTY(EditAnywhere, Category="Aspect")
	float OrthoWidth = 10;

	UPROPERTY(EditAnywhere, Category="Transform")
	FTransform CameraTransform;

	FPlane Planes_Internal[6];
	FTransform InterpolatedCameraTransform_Internal;
};

USTRUCT()
struct FParticleFlags
{
	FParticleFlags() : Val(0) {}

	FParticleFlags(const bool Alive, const uint8 BoneIndex)
		: Alive(Alive),
		  BoneIndex(BoneIndex)
	{
	}

	GENERATED_BODY()

	union
	{
		struct 
		{
			uint8 Alive : 1;
			uint8 BoneIndex : 7;
		};

		uint8 Val;
	};
};


UCLASS()
class TURBOSEQUENCE_LF_API UTurboSequenceRenderAttachmentData : public UTurboSequence_RenderData
{
	GENERATED_BODY()

public:
	
	static UTurboSequenceRenderAttachmentData* CreateObject(UObject* Outer, UTurboSequence_MeshAsset_Lf* InMeshAsset, const FVector& MeshMinBounds, const FVector& MeshMaxBounds);
	
	virtual void PrintRenderData() const override;

	
	void AddAttachmentRenderInstance(const FAttachmentMeshHandle MeshHandle,
								 const FTransform& WorldSpaceTransform, const FTransform3f& AttachmentLocalTransform, uint8 BoneIndex, int32
								 SkeletonIndex);


	virtual void UpdateNiagaraEmitter() override;

private:
	
	//Attachment local transforms
	TArray<FVector3f> ParticleAttachmentPositions;
	TArray<FVector3f> ParticleAttachmentScales;
	TArray<FVector4f> ParticleAttachmentRotations;

	bool bChangedParticleAttachmentPosition = false;
	bool bChangedParticleAttachmentScale = false;
	bool bChangedParticleAttachmentRotation = false;


	
};

USTRUCT()
struct TURBOSEQUENCE_LF_API FCPUAnimationPose_Lf
{
	GENERATED_BODY()

	FAnimPose_Lf Pose;
};

USTRUCT()
struct TURBOSEQUENCE_LF_API FAnimationLibraryDataAllocationItem_Lf
{
	GENERATED_BODY()

	FVector4f Data = FVector4f::Zero();

	int32 ColIndex = 0;
	int32 RowIndex = 0;
};


USTRUCT()
struct TURBOSEQUENCE_LF_API FAnimationLibraryData_Lf
{
	GENERATED_BODY()

	int32 MaxFrames = 0;

	uint16 NumBones = 0;
	
	TMap<int32, FCPUAnimationPose_Lf> KeyframeIndexToPose;

	TArray<int32> KeyframesFilled;

	TMap<FName, int16> BoneNameToAnimationBoneIndex;
	
	FAnimPoseEvaluationOptions_Lf PoseOptions;

	bool bHasPoseData = false;
};

USTRUCT()
struct TURBOSEQUENCE_LF_API FAnimationMetaData_Lf
{
	GENERATED_BODY()

	FAnimationMetaData_Lf() {}
	

	UPROPERTY(EditAnywhere, Category="Animation")
	UAnimSequence* Animation = nullptr;

	UPROPERTY(EditAnywhere, Category="Settings")
	FTurboSequence_AnimPlaySettings_Lf Settings;

	UPROPERTY(EditAnywhere, Category="Current Frame")
	float AnimationTime = 0;
	float LastAnimationTime = 0;

	UPROPERTY(EditAnywhere, Category="Current Frame")
	float FinalAnimationWeight = 1;

	UPROPERTY(EditAnywhere, Category="Current Frame")
	float AnimationWeightTime = 0.25f;	//On start this gets set to 0 and then counts up by delta time to AnimationWeightStartTime (in auto mode)

	UPROPERTY(EditAnywhere, Category="Current Frame")
	float AnimationWeightStartTime = 0.25f;

	UPROPERTY(EditAnywhere, Category="Current Frame")
	float AnimationRemoveTime = 0.25f;

	UPROPERTY(EditAnywhere, Category="Current Frame")
	float AnimationRemoveStartTime = 0.25f;

	UPROPERTY(EditAnywhere, Category="Animation")
	bool bIsLoop = false;

	UPROPERTY(EditAnywhere, Category="Animation")
	bool bSetForRemoval = false;
	
	FBoneMaskSourceHandle AnimationGroupLayerHash;	//Source mask hash

	FUintVector AnimationLibraryHash = FUintVector::ZeroValue; //FUintVector(GetTypeHash(Skeleton), GetTypeHash(Asset), GetTypeHash(Animation))

	float AnimationNormalizedTime = 0;
	float AnimationMaxPlayLength = 1;

	bool bIsRootBoneAnimation = false;	//My source mask contains the name of the root bone, hack for root motion

	int32 CPUAnimationIndex_0 = 0;
	int32 CPUAnimationIndex_1 = 0;

	int32 GPUAnimationIndex_0 = 0;
	int32 GPUAnimationIndex_1 = 0;
	
	float FrameAlpha = 0;

	FAnimationMetaDataHandle AnimationID; //Unique id within the SkinnedMeshRuntime
};


USTRUCT()
struct TURBOSEQUENCE_LF_API FOverrideBoneTransform_Lf
{
	GENERATED_BODY()

	FTransform OverrideTransform = FTransform::Identity;
};

USTRUCT()
struct TURBOSEQUENCE_LF_API FAnimationBlendSpaceData_Lf
{
	GENERATED_BODY()
	
	// < Animation ID | Sampler Index >
	TMap<FAnimationMetaDataHandle, int32> Points;

	UPROPERTY(EditAnywhere)
	FVector3f CurrentPosition = FVector3f::ZeroVector;

	TArray<FBlendSampleData> CachedBlendSampleData;
	FBlendFilter BlendFilter;

	float LongestPlayLength = 0;
	float CurrentTime = 0;

	FAnimTickRecord Tick;
	FMarkerTickRecord Record;
	FAnimNotifyQueue NotifyQueue;
	FDeltaTimeRecord DeltaTimeRecord = FDeltaTimeRecord();
};

/*	==============================================================================================================
												REFERENCE
	==============================================================================================================	*/
USTRUCT()
struct FTurboSequenceRenderHandle
{
	GENERATED_BODY()

	FTurboSequenceRenderHandle() {}
	
	//The combination of these things needs a unique niagara renderer
	FTurboSequenceRenderHandle(const TArray<UMaterialInterface*>& OverrideMaterials, const UNiagaraSystem *InSystem, const UStaticMesh *InStaticMesh, const bool bRenderInCustomDepth, const int32 StencilValue,const bool bInReceivesDecals,
							   FLightingChannels LightingChannels );
	
	bool operator==(const FTurboSequenceRenderHandle& LayerMaskHandle) const = default;
	bool operator!=(const FTurboSequenceRenderHandle& LayerMaskHandle) const = default;
	
	uint32 Hash = 0;
};

FORCEINLINE uint32 GetTypeHash(const FTurboSequenceRenderHandle& TurboSequenceRenderHandle)
{
	return TurboSequenceRenderHandle.Hash;
}

/*	==============================================================================================================
												RUNTIME
	==============================================================================================================	*/

USTRUCT()
struct TURBOSEQUENCE_LF_API FSkinnedMeshAttachmentRuntime
{
	GENERATED_BODY()

	FAttachmentMeshHandle AttachmentHandle;
	FTurboSequenceRenderHandle RenderHandle;
};

USTRUCT()
struct TURBOSEQUENCE_LF_API FSkinnedMeshRuntime_Lf 
{
	GENERATED_BODY()

	FSkinnedMeshRuntime_Lf()
	{
	}

	explicit FSkinnedMeshRuntime_Lf(const FBaseSkeletalMeshHandle InMeshID,
	                                const TObjectPtr<UTurboSequence_MeshAsset_Lf> Asset, const FTurboSequenceRenderHandle InRenderHandle,const int32 InBoneTextureSkeletonIndex )
	{
		MeshID = InMeshID;
		DataAsset = Asset;
		RenderHandle = InRenderHandle;
		BoneTextureSkeletonIndex = InBoneTextureSkeletonIndex;
	}

	FORCEINLINE bool operator==(const FSkinnedMeshRuntime_Lf& Rhs) const
	{
		return this->MeshID == Rhs.MeshID;
	}

	FORCEINLINE bool operator!=(const FSkinnedMeshRuntime_Lf& Rhs) const
	{
		return !(*this == Rhs);
	}

	bool RemoveAttachedParticle(UNiagaraComponent* NiagaraComponent, bool bDestroy = true);

	FBaseSkeletalMeshHandle MeshID;
	
	UPROPERTY()
	TObjectPtr<UTurboSequence_MeshAsset_Lf> DataAsset;

	FTurboSequenceRenderHandle RenderHandle;

	bool bIsVisible = true;

	TMap<uint16, FOverrideBoneTransform_Lf> OverrideBoneTransforms;

	int32 BoneTextureSkeletonIndex = INDEX_NONE;
	
	FTransform WorldSpaceTransform = FTransform::Identity;

	TArray<FSkinnedMeshAttachmentRuntime> Attachments;

	TArray<TObjectPtr<UNiagaraComponent>> AttachedParticles;
	
	int32 GetAnimIndex(FAnimationMetaDataHandle AnimationMetaDataHandle ) const;
	const FAnimationMetaData_Lf* GetAnimMetaData(FAnimationMetaDataHandle AnimationMetaDataHandle) const;
	FAnimationMetaData_Lf* GetAnimMetaData(FAnimationMetaDataHandle AnimationMetaDataHandle);

	FAttachmentMeshHandle AddAttachment(FTurboSequenceRenderHandle TurboSequenceRenderHandle);

	void AddAttachedParticle(TObjectPtr<UNiagaraComponent> NiagaraComponent);
	void RemoveAllAttachedParticles(bool bDestroy = true);

	bool bAnimTickEnabled = true;
	
	uint32 LastAnimationID = 0; //Ensures FAnimationMetaData_Lf AnimationMetaData gets a unique id
	TArray<FAnimationMetaData_Lf> AnimationMetaData;
	
	TMap<TObjectPtr<UBlendSpace>, FAnimationBlendSpaceData_Lf> AnimationBlendSpaceMetaData;

	int64 LastFrameAnimationSolved = 0;
	
};


/*	==============================================================================================================
												GLOBAL
	==============================================================================================================	*/

USTRUCT()
struct TURBOSEQUENCE_LF_API FAnimationBlendLayerMask_Lf
{
	GENERATED_BODY()

	// < Skeleton Layer of Max CPU Bones * 0xFFFF >
	TArray<uint16> RawAnimationLayers;
};


USTRUCT()
struct TURBOSEQUENCE_LF_API FSkinnedMeshGlobalLibrary_RenderThread_Lf
{
	GENERATED_BODY()

	FSkinnedMeshGlobalLibrary_RenderThread_Lf()
	{
		BoneTransformParams = FMeshUnitComputeShader_Params_Lf();
		AnimationLibraryParams = FSettingsComputeShader_Params_Lf();
	}
	
	FMeshUnitComputeShader_Params_Lf BoneTransformParams;
	FSettingsComputeShader_Params_Lf AnimationLibraryParams;
	
	uint32 AnimationLibraryMaxNum = 0;
};


USTRUCT()
struct TURBOSEQUENCE_LF_API FSkinnedMeshGlobalLibrary_Lf
{
	GENERATED_BODY()

	FSkinnedMeshGlobalLibrary_Lf() {}

	int32 GlobalMeshCounter = 0; //Used to generate unique FTurboSequenceMeshHandle globally

	UPROPERTY()
	TMap<FTurboSequenceRenderHandle, UTurboSequence_RenderData*> PerReferenceData;
	
	UPROPERTY()
	TArray<TObjectPtr<UTurboSequence_MeshAsset_Lf>> PerReferenceDataKeys; //The order we upload asset meshes on the gpu
	
	// -> Key -> < USkeleton | UTurboSequence_MeshAsset_Lf | UAnimSequence >
	TMap<FUintVector, FAnimationLibraryData_Lf> AnimationLibraryData;
	uint32 AnimationLibraryMaxNum = 0;
	TArray<FVector4f> AnimationLibraryDataAllocatedThisFrame;
	// // Sum of -> ( Values * Library Hash, Mesh Bones ) is the Keyframe index
	// // We need this construct to easy determinate the index when we remove an animation from the GPU
	// // We need an alpha type to copy the new Library Data over
	// TMap<FUintVector, int32> AnimationLibraryAlphaKeyframeDimensions;

	TArray<FCameraView_Lf> CameraViews;

	//Masks
	bool bMasksNeedRebuilding = false;
	int32 MasksBuiltBoneCount = -1; //Masks are added to AnimationLayers_RenderThread, separated by MaxNumCPUBones, need to rebuild when that changes 
	TMap<FBoneMaskBuiltProxyHandle, int32> MaskRefCount; //Ref count 
	TMap<FBoneMaskBuiltProxyHandle, FAnimationBlendLayerMask_Lf> AnimationBlendLayerMasks; //Currently in use built masks 

	TMap<FBoneMaskBuiltProxyHandle, int32> ProxyToIndex; //Order added to AnimationLayers_RenderThread
	
	int32 MaxNumCPUBones = 0;
	int32 MaxNumGPUBones = 0;

	// < MeshID | Index >
	TMap<FBaseSkeletalMeshHandle, FSkinnedMeshRuntime_Lf> RuntimeSkinnedMeshes; //Skeleton to runtime
	
	TBestFitAllocator<8, 512 * 512 > BoneTextureAllocator;
	
	bool bRefreshAsyncChunkedMeshData = false;
};
