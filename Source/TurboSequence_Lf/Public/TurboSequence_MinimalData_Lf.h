// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TurboSequence_MeshAsset_Lf.h"
#include "UObject/Object.h"

#include "TurboSequence_MinimalData_Lf.generated.h"

/*	==============================================================================================================
												DATA
	==============================================================================================================	*/

UENUM(BlueprintType)
enum class ETurboSequence_AnimationForceMode_Lf : uint8
{
	None,
	PerLayer,
	AllLayers
};

UENUM(BlueprintType)
enum class ETurboSequence_RootMotionMode_Lf : uint8
{
	None,
	OnRootBoneAnimated,
	Force
};

UENUM(BlueprintType)
enum class ETurboSequence_ManagementMode_Lf : uint8
{
	Auto,
	SelfManaged
};

UCLASS(BlueprintType)
class TURBOSEQUENCE_LF_API UTurboSequence_ThreadContext_Lf : public UObject
{
	GENERATED_BODY()

public:
	FCriticalSection CriticalSection;

	UFUNCTION(BlueprintCallable)
	void LockThread()
	{
		CriticalSection.Lock();
	}

	UFUNCTION(BlueprintCallable)
	void UnlockThread()
	{
		CriticalSection.Unlock();
	}
};

USTRUCT(BlueprintType)
struct TURBOSEQUENCE_LF_API FTurboSequence_BoneLayer_Lf
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName BoneLayerName = FName();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 BoneDepth = 2;
};


struct FBoneMaskSourceHandle
{
	bool operator==(const FBoneMaskSourceHandle& LayerMaskHandle) const = default;
	bool operator!=(const FBoneMaskSourceHandle& LayerMaskHandle) const = default;
	
	uint32 Hash = 0;
};

FORCEINLINE uint32 GetTypeHash(const FBoneMaskSourceHandle& BoneMaskSourceHandle)
{
	return BoneMaskSourceHandle.Hash;
}

struct FBoneMaskBuiltProxyHandle
{
	bool operator==(const FBoneMaskBuiltProxyHandle& LayerMaskHandle) const = default;
	bool operator!=(const FBoneMaskBuiltProxyHandle& LayerMaskHandle) const = default;
	
	uint32 Hash = 0;
};

FORCEINLINE uint32 GetTypeHash(const FBoneMaskBuiltProxyHandle& BoneMaskSourceHandle)
{
	return BoneMaskSourceHandle.Hash;
}

USTRUCT(BlueprintType)
struct FBaseSkeletalMeshHandle
{
	GENERATED_BODY()

	FBaseSkeletalMeshHandle() : MeshID(INDEX_NONE), AttachmentID(0) {}

	explicit FBaseSkeletalMeshHandle(const int32 InMeshID) : MeshID(InMeshID), AttachmentID(0) {}

	union
	{
		struct 
		{
			int32 MeshID : 24;
			int32 AttachmentID : 8; //Always 0
		};

		int32 ID;
	};

	bool IsValid() const { return MeshID != INDEX_NONE; }
	
	friend uint32 GetTypeHash(const FBaseSkeletalMeshHandle& TurboSequenceMeshHandle);

	friend bool operator==(const FBaseSkeletalMeshHandle& Lhs, const FBaseSkeletalMeshHandle& RHS)
	{
		return Lhs.ID == RHS.ID;
	}

	friend bool operator!=(const FBaseSkeletalMeshHandle& Lhs, const FBaseSkeletalMeshHandle& RHS)
	{
		return !(Lhs == RHS);
	}
};

#if UE_BUILD_DEBUG
uint32 GetTypeHash(const FTurboSequenceMeshHandle& TurboSequenceMeshHandle);
#else // optimize by inlining in shipping and development builds
FORCEINLINE uint32 GetTypeHash(const FBaseSkeletalMeshHandle& TurboSequenceMeshHandle)
{
	return GetTypeHash(TurboSequenceMeshHandle.ID);
}
#endif

USTRUCT(BlueprintType)
struct FAttachmentMeshHandle : public FBaseSkeletalMeshHandle
{
	GENERATED_BODY()

	FAttachmentMeshHandle()
	{
		MeshID = -1;
		AttachmentID = 0; 
	}

	FAttachmentMeshHandle(const FBaseSkeletalMeshHandle BaseHandle)
	{
		MeshID = BaseHandle.MeshID;
	}

	explicit FAttachmentMeshHandle(const FBaseSkeletalMeshHandle BaseHandle, int32 InAttachmentID)
	{
		MeshID = BaseHandle.MeshID;
		AttachmentID = InAttachmentID;
	}

	FBaseSkeletalMeshHandle GetBaseHandle() const { return FBaseSkeletalMeshHandle(MeshID); }
	
	friend bool operator==(const FAttachmentMeshHandle& Lhs, const FAttachmentMeshHandle& RHS) = default;
	friend bool operator!=(const FAttachmentMeshHandle& Lhs, const FAttachmentMeshHandle& RHS) = default;
	friend uint32 GetTypeHash(const FAttachmentMeshHandle& AttachmentMeshHandle);
};

#if UE_BUILD_DEBUG
uint32 GetTypeHash(const FAttachmentMeshHandle& TurboSequenceMeshHandle);
#else // optimize by inlining in shipping and development builds
FORCEINLINE uint32 GetTypeHash(const FAttachmentMeshHandle& AttachmentMeshHandle)
{
	return GetTypeHash(AttachmentMeshHandle.ID);
}
#endif


USTRUCT(BlueprintType)
struct FTurboSequence_MaskDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FTurboSequence_BoneLayer_Lf> BoneLayerMasks;

	FBoneMaskSourceHandle GetHandle() const;

	FBoneMaskBuiltProxyHandle GetBuiltProxyHandle(UTurboSequence_MeshAsset_Lf *DataAsset) const; 
};

USTRUCT(BlueprintType)
struct TURBOSEQUENCE_LF_API FTurboSequence_AnimPlaySettings_Lf
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTurboSequence_MaskDefinition MaskDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAnimationTimeSelfManaged = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bNormalizeWeightInGroup = true;		//Layering a masked animation over the top, I don't want its weight normalised, I want it to be able to fade in/out 		

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AnimationWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AnimationPlayTimeInSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AnimationSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETurboSequence_AnimationForceMode_Lf ForceMode = ETurboSequence_AnimationForceMode_Lf::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StartTransitionTimeInSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float EndTransitionTimeInSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETurboSequence_RootMotionMode_Lf RootMotionMode = ETurboSequence_RootMotionMode_Lf::OnRootBoneAnimated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETurboSequence_ManagementMode_Lf AnimationManagementMode = ETurboSequence_ManagementMode_Lf::Auto;
};


struct FAnimationMetaDataHandle
{
	FAnimationMetaDataHandle() = default;

	explicit FAnimationMetaDataHandle(const uint32 InAnimationID) { this->AnimationID = InAnimationID; }
	
	friend bool operator==(const FAnimationMetaDataHandle& Lhs, const FAnimationMetaDataHandle& RHS)
	{
		return Lhs.AnimationID == RHS.AnimationID;
	}

	friend bool operator!=(const FAnimationMetaDataHandle& Lhs, const FAnimationMetaDataHandle& RHS)
	{
		return !(Lhs == RHS);
	}

	
	uint32 AnimationID = 0; //Unique id within the SkinnedMeshRuntime that owns it
};

#if UE_BUILD_DEBUG
uint32 GetTypeHash(const FAnimationMetaDataHandle& AnimationMetaDataHandle);
#else // optimize by inlining in shipping and development builds
FORCEINLINE uint32 GetTypeHash(const FAnimationMetaDataHandle& AnimationMetaDataHandle)
{
	return AnimationMetaDataHandle.AnimationID;
}
#endif

//
USTRUCT(BlueprintType)
struct TURBOSEQUENCE_LF_API FTurboSequence_AnimMinimalData_Lf
{
	GENERATED_BODY()

	FTurboSequence_AnimMinimalData_Lf() {}

	FTurboSequence_AnimMinimalData_Lf(const FAnimationMetaDataHandle& AnimationID, FBaseSkeletalMeshHandle BelongsToMeshID)
		: AnimationID(AnimationID),
		  BelongsToMeshID(BelongsToMeshID)
	{
	}

	explicit FTurboSequence_AnimMinimalData_Lf(bool bValid)
	{
		bIsValid = bValid;
	}

	friend bool operator==(const FTurboSequence_AnimMinimalData_Lf& Lhs, const FTurboSequence_AnimMinimalData_Lf& RHS)
	{
		return Lhs.AnimationID == RHS.AnimationID
			&& Lhs.BelongsToMeshID == RHS.BelongsToMeshID;
	}

	friend bool operator!=(const FTurboSequence_AnimMinimalData_Lf& Lhs, const FTurboSequence_AnimMinimalData_Lf& RHS)
	{
		return !(Lhs == RHS);
	}
	
	FAnimationMetaDataHandle AnimationID;
	FBaseSkeletalMeshHandle BelongsToMeshID;

protected:
	bool bIsValid = false;

public:
	FORCEINLINE bool IsAnimationValid() const
	{
		return bIsValid;
	}
};

#if UE_BUILD_DEBUG
uint32 GetTypeHash(const FTurboSequence_AnimMinimalData_Lf& AnimMinimalData);
#else // optimize by inlining in shipping and development builds
FORCEINLINE uint32 GetTypeHash(const FTurboSequence_AnimMinimalData_Lf& AnimMinimalData)
{
	return HashCombine(::GetTypeHash(AnimMinimalData.AnimationID),::GetTypeHash(AnimMinimalData.BelongsToMeshID) );
}
#endif

USTRUCT(BlueprintType)
struct TURBOSEQUENCE_LF_API FTurboSequence_AnimMinimalBlendSpace_Lf
{
	GENERATED_BODY()

	FTurboSequence_AnimMinimalBlendSpace_Lf() {}
	
	explicit FTurboSequence_AnimMinimalBlendSpace_Lf(bool bValid)
	{
		bIsValid = bValid;
	}

	UPROPERTY(EditAnywhere)
	TArray<uint32> Samples;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UBlendSpace> BlendSpace;

	UPROPERTY(EditAnywhere)
	FBaseSkeletalMeshHandle BelongsToMeshID;

protected:
	UPROPERTY(VisibleAnywhere)
	bool bIsValid = false;

public:
	FORCEINLINE bool IsAnimBlendSpaceValid() const
	{
		return bIsValid;
	}
};


USTRUCT(BlueprintType)
struct TURBOSEQUENCE_LF_API FTurboSequence_MeshMetaData_Lf
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UTurboSequence_MeshAsset_Lf> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
};

USTRUCT(BlueprintType)
struct TURBOSEQUENCE_LF_API FTurboSequence_PoseCurveData_Lf
{
	GENERATED_BODY()

	FTurboSequence_PoseCurveData_Lf() = default;
	
	FTurboSequence_PoseCurveData_Lf(const TObjectPtr<UAnimSequence>& CurveAnimation, const FName& CurveID,
	                                const float CurveFrame0)
		: CurveAnimation(CurveAnimation),
		  CurveName(CurveID),
		  CurveFrame_0(CurveFrame0)
	{
	}
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> CurveAnimation = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName CurveName = FName("");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CurveFrame_0 = 0;

	FORCEINLINE bool IsCurveValid() const
	{
		return IsValid(CurveAnimation);
	}
};


UENUM(BlueprintType)
enum class ETurboSequence_TransformSpace_Lf : uint8
{
	BoneSpace,
	ComponentSpace,
	WorldSpace
};

USTRUCT(BlueprintType)
struct TURBOSEQUENCE_LF_API FTurboSequence_UpdateGroup_Lf
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	TArray<FBaseSkeletalMeshHandle> RawIDs;

	UPROPERTY(VisibleAnywhere)
	TMap<FBaseSkeletalMeshHandle, int32> RawIDData;
};

USTRUCT(BlueprintType)
struct TURBOSEQUENCE_LF_API FTurboSequence_UpdateContext_Lf
{
	GENERATED_BODY()

	FTurboSequence_UpdateContext_Lf() {}
	
	FTurboSequence_UpdateContext_Lf(const int32 GroupIndex)
		: GroupIndex(GroupIndex)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 GroupIndex = 0;
};

USTRUCT()
struct TURBOSEQUENCE_LF_API FTurboSequence_AnimNotifyQueue_Lf
{
	GENERATED_BODY()

	FTurboSequence_AnimNotifyQueue_Lf()
	{
		RandomStream.Initialize(0x05629063);
	}

	/** Work out whether this notify should be triggered based on its chance of triggering value */
	bool PassesChanceOfTriggering(const FAnimNotifyEvent* Event) const;

	/** Add notify to queue*/
	void AddAnimNotify(const FAnimNotifyEvent* Notify, const UObject* NotifySource);

	/** Add anim notifies **/
	void AddAnimNotifies(bool bSrcIsLeader, const TArray<FAnimNotifyEventReference>& NewNotifies, const float InstanceWeight);

	/** Wrapper functions for when we aren't coming from a sync group **/
	void AddAnimNotifies(const TArray<FAnimNotifyEventReference>& NewNotifies, const float InstanceWeight) { AddAnimNotifies(true, NewNotifies, InstanceWeight); }

	/** Reset queue & update LOD level */
	void Reset(USkeletalMeshComponent* Component);

	/** Internal random stream */
	FRandomStream RandomStream;

	/** Animation Notifies that has been triggered in the latest tick **/
	UPROPERTY(transient)
	TArray<FAnimNotifyEventReference> AnimNotifies;

private:
	/** Implementation for adding notifies*/
	void AddAnimNotifiesToDest(bool bSrcIsLeader, const TArray<FAnimNotifyEventReference>& NewNotifies, TArray<FAnimNotifyEventReference>& DestArray, const float InstanceWeight);

	/** Adds the contents of the NewNotifies array to the DestArray (maintaining uniqueness of notify states*/
	void AddAnimNotifiesToDestNoFiltering(const TArray<FAnimNotifyEventReference>& NewNotifies, TArray<FAnimNotifyEventReference>& DestArray) const;
};