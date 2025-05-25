// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TurboSequence_Data_Lf.h"
#include "TurboSequence_Manager_Lf.generated.h"

USTRUCT(BlueprintType)
struct FTurboSequence_AttachInfo
{
	GENERATED_BODY()

	// This calls the UFUNCTION in the owning UObject
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(GetOptions="GetBoneAndSocketNames")) 
	FName Name; //Bone or socket name

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform Transform;
};


UCLASS()
class TURBOSEQUENCE_LF_API ATurboSequence_Manager_Lf : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATurboSequence_Manager_Lf();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	
	virtual bool ShouldTickIfViewportsOnly() const override;
	
	// Called every frame
	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
	
	/*
	< - - - - - - - - - - - - - - - - - - - - >
					DATA
	< - - - - - - - - - - - - - - - - - - - - >
*/

	// For the Frustum Culling we need Camera Transforms
	inline static TMap<uint8, FTransform> LastFrameCameraTransforms;
	// The Global Library which holds all Game Thread Data
	UPROPERTY()
	FSkinnedMeshGlobalLibrary_Lf GlobalLibrary;
	// The Global Library which holds all Render Thread Data
	inline static FSkinnedMeshGlobalLibrary_RenderThread_Lf GlobalLibrary_RenderThread;

	// The Instance
	inline static TObjectPtr<ATurboSequence_Manager_Lf> Instance;

	
	static TObjectPtr<UTurboSequence_MeshAsset_Lf> AttachmentAsset; 
	

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	
	UPROPERTY(VisibleAnywhere)
	// The Global Data which keeps track of internal Texture Data
	TObjectPtr<UTurboSequence_GlobalData_Lf> GlobalData;
	
protected:
	UPROPERTY()
	// The Current Thread Context, Please Call GetThreadContext()
	TObjectPtr<UTurboSequence_ThreadContext_Lf> CurrentThreadContext_Runtime;

	//Locked when we are reading the library in the render thread or modifying it in the game thread
	FCriticalSection RenderThreadDataConsistency; 

public:
	/**
	 * Get the Thread Context | Any Thread
	 * @return The Thread Context with the Critical Section
	 */
	TObjectPtr<UTurboSequence_ThreadContext_Lf> GetThreadContext()
	{
		if (IsInGameThread())
		{
			if (!IsValid(CurrentThreadContext_Runtime))
			{
				CurrentThreadContext_Runtime = NewObject<UTurboSequence_ThreadContext_Lf>();
			}
		}

		if (!IsValid(CurrentThreadContext_Runtime))
		{
			UE_LOG(LogTurboSequence_Lf, Error,
			       TEXT(
				       "Make sure to refresh the Thread Context in the main thread..., to fix this call the Game Thread Functions in the Game Thread..."
			       ));
		}

		return CurrentThreadContext_Runtime;
	}


	/**
	 * Refreshes the Thread Context, usually called in Tick from the Manager 
	 */
	void RefreshThreadContext_GameThread()
	{
		CurrentThreadContext_Runtime = NewObject<UTurboSequence_ThreadContext_Lf>();
	}

	
	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=( WorldContext = "WorldContextObject"))
	static FBaseSkeletalMeshHandle AddSkinnedMeshInstance(UTurboSequence_MeshAsset_Lf* FromAsset,
	                                                                 const FTransform& SpawnTransform,
	                                                                 UObject* WorldContextObject,
	                                                                 const TArray<UMaterialInterface*>& OverrideMaterials, FLightingChannels LightingChannels, bool bNewReceivesDecals = false, bool bInRenderInCustomDepth = false, int32 InStencilValue = 0);
	static void RemoveRenderHandle(
		const FTurboSequenceRenderHandle RenderHandle,
		const FAttachmentMeshHandle MeshHandle,
		const TObjectPtr<UTurboSequence_MeshAsset_Lf> DataAsset, bool
		& bRemovedASkeleton);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static bool RemoveSkinnedMeshInstance(FBaseSkeletalMeshHandle MeshID);

	static bool GetAttachmentComponentSpaceTransform(FTransform& AttachmentComponentSpaceTransform, int32& BoneIndexGPU,
	                                                 const FName
	                                                 BoneOrSocketName,
	                                                 const TObjectPtr<UTurboSequence_MeshAsset_Lf>& MeshAsset,
	                                                 const FTransform& Transform, const EBoneControlSpace AttachSpace, const FTransform& WorldSpaceTransform);

	/**
	 * 
	 * @param MeshID Skeleton to attach to
	 * @param StaticMesh Static mesh to attach to it
	 * @param SocketOrBoneName Bone name to attach to
	 * @param Transform Transform in component space
	 * @param OverrideMaterials
	 * @param LightingChannels
	 * @param bNewReceivesDecals
	 * @param bInRenderInCustomDepth
	 * @param InStencilValue
	 * @param AttachSpace
	 * @return Attachment handle
	 */
	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static FAttachmentMeshHandle AddInstanceAttachment(const FBaseSkeletalMeshHandle MeshID, UStaticMesh* StaticMesh, FName SocketOrBoneName, const FTransform& Transform, const
	                                                   TArray<UMaterialInterface*>& OverrideMaterials, FLightingChannels LightingChannels = FLightingChannels(), bool bNewReceivesDecals = false, bool bInRenderInCustomDepth = false, int32 InStencilValue = 0, EBoneControlSpace AttachSpace = BCS_BoneSpace);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static void RemoveInstanceAttachment(const FAttachmentMeshHandle AttachmentHandle);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static UNiagaraComponent* SpawnParticleAttachedSingle(const FBaseSkeletalMeshHandle MeshID, UNiagaraSystem* RendererSystem, const FName SocketOrBoneName, const FTransform& Transform, const EBoneControlSpace
                                                        																		AttachSpace);
	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static UNiagaraComponent* SpawnParticleAttachedMulti(const FBaseSkeletalMeshHandle MeshID, UNiagaraSystem* RendererSystem, const TArray<FTurboSequence_AttachInfo>& Attachments, EBoneControlSpace
														AttachSpace);


	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static UNiagaraComponent* SpawnParticle(FBaseSkeletalMeshHandle MeshID, UNiagaraSystem* NiagaraSystem);
	

	
	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static bool RemoveParticle(FBaseSkeletalMeshHandle MeshID, UNiagaraComponent* NiagaraComponent, bool bDestroy = true);

protected:
	
	static void SolveMeshes_GameThread(float DeltaTime, UWorld* InWorld);

	static void SolveMeshes_RenderThread(FRHICommandListImmediate& RHICmdList);

public:
	/**
	 * Clean the Manager, NOTE: The manager is auto manage itself, you don't need to implement this function
	 * @param bIsEndPlay Is end of the play session or map change
	 */
	static void CleanManager_GameThread(bool bIsEndPlay);

	/*
    < - - - - - - - - - - - - - - - - - - - - >
                    Helpers
    < - - - - - - - - - - - - - - - - - - - - >
*/
	static FORCEINLINE_DEBUGGABLE void ClearBuffers()
	{
		LastFrameCameraTransforms.Empty();
		GlobalLibrary_RenderThread = FSkinnedMeshGlobalLibrary_RenderThread_Lf();
	}

	UFUNCTION(BlueprintPure, Category="Turbo Sequence", meta=(ReturnDisplayName="World Space Transform"))
	static FTransform GetMeshWorldSpaceTransform(FBaseSkeletalMeshHandle MeshID);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static void SetMeshWorldSpaceTransform(FBaseSkeletalMeshHandle MeshID, const FTransform& Transform);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool GetRootMotionTransform(FTransform& OutRootMotion,
	                                                    FBaseSkeletalMeshHandle MeshID,
	                                                    float DeltaTime,
	                                                    const EBoneSpaces::Type Space = EBoneSpaces::WorldSpace);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static void MoveMeshWithRootMotion(FBaseSkeletalMeshHandle MeshID,
	                                                    float DeltaTime,
	                                                    bool ZeroZAxis = false,
	                                                    bool bIncludeScale = false);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence",
		meta=(ReturnDisplayName="Animation Data"))
	static FTurboSequence_AnimMinimalData_Lf PlayAnimation(const FBaseSkeletalMeshHandle MeshID,
	                                                                        UAnimSequence* Animation,
	                                                                        const FTurboSequence_AnimPlaySettings_Lf& AnimSettings,
	                                                                        const bool bForceLoop = false, const bool bForceFront = false
	);


	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Blend Space Data"))
	static FTurboSequence_AnimMinimalBlendSpace_Lf PlayBlendSpace(FBaseSkeletalMeshHandle MeshID,
		UBlendSpace* BlendSpace,
		const FTurboSequence_AnimPlaySettings_Lf& AnimSettings
	);


	UFUNCTION(BlueprintPure, Category="Turbo Sequence", meta=(ReturnDisplayName="Animation Sequence"))
	static UAnimSequence* GetHighestPriorityPlayingAnimation_RawID_Concurrent(FBaseSkeletalMeshHandle MeshID);

	static void GetAnimNotifies(FBaseSkeletalMeshHandle MeshID, FTurboSequence_AnimNotifyQueue_Lf& NotifyQueue);
	
	/**
	 * Tweaks an Animation with new Settings
	 * @param TweakSettings The new Animation Settings
	 * @param AnimationData The Animation ID
	 * @return True if Successful
	 */
	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool TweakAnimation(const FTurboSequence_AnimPlaySettings_Lf& TweakSettings,
	                                      const FTurboSequence_AnimMinimalData_Lf& AnimationData);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool TweakBlendSpace(FBaseSkeletalMeshHandle MeshID,
	                                             const FTurboSequence_AnimMinimalBlendSpace_Lf& BlendSpaceData,
	                                             const FVector3f& WantedPosition);

	/**
	 * Get the Current Animation Setting of an Animation as ref
	 * @param AnimationSettings Out Animation Setting
	 * @param AnimationData From Animation Data
	 * @return True if Successful
	 */
	UFUNCTION(BlueprintCallable, Category="Turbo Sequence",meta=(ReturnDisplayName="Success"))
	static bool GetAnimationSettings(FTurboSequence_AnimPlaySettings_Lf& AnimationSettings,
	                                            const FTurboSequence_AnimMinimalData_Lf& AnimationData);

	static bool GetAnimationMetaData(FAnimationMetaData_Lf& AnimationMetaData,
	                                            const FTurboSequence_AnimMinimalData_Lf& AnimationData);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence")
	static void SetAnimationTick(FBaseSkeletalMeshHandle MeshID, const bool bInTickEnabled);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool GetBoneTransform(FTransform& OutIKTransform,
	                                            FBaseSkeletalMeshHandle MeshID,
	                                            const FName& BoneName,
	                                            const EBoneSpaces::Type Space = EBoneSpaces::WorldSpace);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool UpdateOverrideBoneTransform(FBaseSkeletalMeshHandle MeshID,
	                                            const FName& BoneName,
	                                            const FTransform& Transform,
	                                            const EBoneSpaces::Type Space = EBoneSpaces::WorldSpace);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool RemoveOverrideBoneTransform(FBaseSkeletalMeshHandle MeshID,
												const FName& BoneName);

	
	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool GetSocketTransform(FTransform& OutSocketTransform,
	                                                FBaseSkeletalMeshHandle MeshID,
	                                                const FName& SocketName,
	                                                const EBoneSpaces::Type Space = EBoneSpaces::WorldSpace);

	static bool GetBoneTransforms_RawID_Concurrent(TArray<FTransform>& OutBoneTransforms, FBaseSkeletalMeshHandle MeshID,
	                                               const TArray<FName>& BoneNames,
	                                               const EBoneSpaces::Type Space = EBoneSpaces::WorldSpace);


	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Curve"))
	static FTurboSequence_PoseCurveData_Lf GetAnimationCurveValue(FBaseSkeletalMeshHandle MeshID,
		const FName& CurveName,
		UAnimSequence* Animation);
	

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool SetInstanceCustomData(const FBaseSkeletalMeshHandle MeshID, const int Index,
	                                    float Value, const bool bSetAttachments = true);

	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool SetInstanceCustomDataArray(
										const FBaseSkeletalMeshHandle MeshID, const int StartIndex,
										const TArray<float>& Values, const bool bSetAttachments = true); 
	


	/**
	 * Sets the Turbo Sequence Mesh to an equallent UE Mesh using the Ts IK system, make sure the UE Mesh has the same skeleton
	 * @param TsMeshID The Mesh ID
	 * @param UEMesh The Unreal Engine Mesh Instance, SkeletalMesh or PoseableMesh
	 * @param UEMeshPercentage The Percentage between 0 = TS Mesh and 1 = UE Mesh
	 * @param AnimationDeltaTime The TS Mesh Animation Delta Time
	 * @return Ture if Successful
	 */
	UFUNCTION(BlueprintCallable, Category="Turbo Sequence", meta=(ReturnDisplayName="Success"))
	static bool SetTransitionTsMeshToUEMesh(const FBaseSkeletalMeshHandle TsMeshID, USkinnedMeshComponent* UEMesh,
	                                        const float UEMeshPercentage, const float AnimationDeltaTime);
};
