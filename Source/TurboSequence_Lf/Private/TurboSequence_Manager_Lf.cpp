// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.


#include "TurboSequence_Manager_Lf.h"

#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "TurboSequence_ComputeShaders_Lf.h"
#include "TurboSequence_GlobalData_Lf.h"
#include "TurboSequence_RenderData.h"
#include "TurboSequence_Utility_Lf.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMeshSocket.h"
#include "NiagaraFunctionLibrary.h"


DECLARE_STATS_GROUP(TEXT("TurboSequenceManager_Lf"), STATGROUP_TurboSequenceManager_Lf, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("Solve_TurboSequenceMeshes_Lf"), STAT_Solve_TurboSequenceMeshes_Lf,
                   STATGROUP_TurboSequenceManager_Lf);
DECLARE_CYCLE_STAT(TEXT("Add_TurboSequenceMeshInstances_Lf"), Add_TurboSequenceMeshInstances_Lf,
                   STATGROUP_TurboSequenceManager_Lf);
DECLARE_CYCLE_STAT(TEXT("Remove_TurboSequenceMeshInstances_Lf"), Remove_TurboSequenceMeshInstances_Lf,
				   STATGROUP_TurboSequenceManager_Lf);
DECLARE_CYCLE_STAT(TEXT("Solve_TurboSequenceMeshes_RT_Lf"), STAT_Solve_TurboSequenceMeshes_RT_Lf,
				   STATGROUP_TurboSequenceManager_Lf);

DECLARE_DWORD_COUNTER_STAT(TEXT("Total Mesh Count"), STAT_TotalMeshCount, STATGROUP_TurboSequenceManager_Lf);
DECLARE_DWORD_COUNTER_STAT(TEXT("Visible Mesh Count"), STAT_VisibleMeshCount, STATGROUP_TurboSequenceManager_Lf);

TObjectPtr<UTurboSequence_MeshAsset_Lf> ATurboSequence_Manager_Lf::AttachmentAsset;


FTurboSequenceRenderHandle::FTurboSequenceRenderHandle(const TArray<UMaterialInterface*>& OverrideMaterials,
                                                       const UNiagaraSystem* InSystem, const UStaticMesh* InStaticMesh, const bool bRenderInCustomDepth, const int32 StencilValue, const bool bInReceivesDecals, FLightingChannels LightingChannels)
{
	const int32 MeshMaterialCount = InStaticMesh->GetStaticMaterials().Num();
	//Can't override more materials than the mesh has
	const int32 MaterialsToCheck = FMath::Min(MeshMaterialCount, OverrideMaterials.Num()); 

	for (int32 MaterialIndex = 0; MaterialIndex < MaterialsToCheck; ++MaterialIndex)
	{
		Hash = HashCombine(Hash,GetTypeHash(OverrideMaterials[MaterialIndex]));
	}
	
	Hash = HashCombine(Hash,GetTypeHash(InSystem));
	Hash = HashCombine(Hash,GetTypeHash(InStaticMesh));
	Hash = HashCombine(Hash,GetTypeHash(bInReceivesDecals));

	Hash = HashCombine(Hash,GetTypeHash(LightingChannels.bChannel0));
	Hash = HashCombine(Hash,GetTypeHash(LightingChannels.bChannel1));
	Hash = HashCombine(Hash,GetTypeHash(LightingChannels.bChannel2));
	

	if(bRenderInCustomDepth)
	{
		Hash = HashCombine(Hash,GetTypeHash(StencilValue));
	}
}

// Sets default values
ATurboSequence_Manager_Lf::ATurboSequence_Manager_Lf()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	// Allow ticking in the editor when the game is not running
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	// Setup root component
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	SetRootComponent(Root);

	if (!AttachmentAsset)
	{
		const ConstructorHelpers::FObjectFinder<UTurboSequence_MeshAsset_Lf> AttachmentAssetFinder(TEXT("/TurboSequence_Lf/Resources/TS_AttachmentMeshAsset"));

		if(AttachmentAssetFinder.Succeeded())
		{
			AttachmentAsset = AttachmentAssetFinder.Object;
		}
	}
}

void ATurboSequence_Manager_Lf::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	CleanManager_GameThread(true);
}

bool ATurboSequence_Manager_Lf::ShouldTickIfViewportsOnly() const
{
	return true;
}

// Called every frame
void ATurboSequence_Manager_Lf::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime,TickType,ThisTickFunction);

	UWorld* World = Instance->GetWorld();

	const float Time = TickType == LEVELTICK_All ? DeltaTime : 0.f;

	SolveMeshes_GameThread(Time, World);

	for (const auto & PerReferenceData : GlobalLibrary.PerReferenceData)
	{
		//PerReferenceData.Value->PrintRenderData();
			
		PerReferenceData.Value->UpdateNiagaraEmitter();
	}
	
	if (GlobalLibrary.RuntimeSkinnedMeshes.Num() && IsValid(GlobalData) && IsValid(GlobalData->AnimationLibraryTexture))
	{
		GlobalLibrary_RenderThread.AnimationLibraryParams.ShaderID = GetTypeHash(GlobalData);
		GlobalLibrary_RenderThread.AnimationLibraryParams.bIsAdditiveWrite = true;
		GlobalLibrary_RenderThread.AnimationLibraryParams.bUse32BitTexture = false;

		FSettingsCompute_Shader_Execute_Lf::Dispatch(GlobalLibrary_RenderThread.AnimationLibraryParams,
		                                             GlobalData->AnimationLibraryTexture);

		FSkinnedMeshGlobalLibrary_RenderThread_Lf& Library_RenderThread = GlobalLibrary_RenderThread;
		ENQUEUE_RENDER_COMMAND(TurboSequence_ClearAnimLibraryInput_Lf)(
			[&Library_RenderThread](FRHICommandListImmediate& RHICmdList)
			{
				Library_RenderThread.AnimationLibraryParams.SettingsInput.Reset();
				Library_RenderThread.AnimationLibraryParams.AdditiveWriteBaseIndex = Library_RenderThread.
					AnimationLibraryMaxNum;
			});

		GlobalLibrary_RenderThread.BoneTransformParams.AnimationLibraryTexture = GlobalData->AnimationLibraryTexture;
		GlobalLibrary_RenderThread.BoneTransformParams.AnimationOutputTexturePrevious = GlobalData->
			TransformTexture_PreviousFrame;

		FMeshUnit_Compute_Shader_Execute_Lf::Dispatch(
			[&](FRHICommandListImmediate& RHICmdList)
			{
				SolveMeshes_RenderThread(RHICmdList);
			},
			GlobalLibrary_RenderThread.BoneTransformParams,
			Instance->GlobalData->TransformTexture_CurrentFrame, /*Instance->GlobalData->CustomDataTexture,*/
			[&](TArray<float>& DebugValue)
			{
#if TURBO_SEQUENCE_DEBUG_GPU_READBACK
				if (DebugValue.Num() && !FMath::IsNaN(DebugValue[0]) && FMath::IsNearlyEqual(
					DebugValue[0], 777.0f, 0.1f))
				{
					UE_LOG(LogTurboSequence_Lf, Warning, TEXT(" ----- > Start Stack < -----"))
					for (float Value : DebugValue)
					{
						UE_LOG(LogTurboSequence_Lf, Warning, TEXT("%f"), Value);
					}
					UE_LOG(LogTurboSequence_Lf, Warning, TEXT(" ----- > End Stack < -----"))
				}
#endif
			});
	}


	if (GlobalLibrary.RuntimeSkinnedMeshes.Num())
	{
		// Otherwise the Critical Section grow in size until we run out of memory when we don't refresh it...
		Instance->RefreshThreadContext_GameThread();
	}
}


FBaseSkeletalMeshHandle ATurboSequence_Manager_Lf::AddSkinnedMeshInstance(
	UTurboSequence_MeshAsset_Lf* FromAsset,
	const FTransform& SpawnTransform,
	UObject* WorldContextObject,
	const TArray<UMaterialInterface*>& OverrideMaterials, FLightingChannels LightingChannels, bool bNewReceivesDecals, bool bInRenderInCustomDepth, int32 InStencilValue)
{
	SCOPE_CYCLE_COUNTER(Add_TurboSequenceMeshInstances_Lf);

	if (!IsValid(FromAsset))
	{
		return FBaseSkeletalMeshHandle();
	}

	if (!IsValid(FromAsset->GlobalData))
	{
		const FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
			"AssetRegistry");

		TArray<FAssetData> GlobalAssetData;
		AssetRegistry.Get().GetAssetsByClass(
			FTopLevelAssetPath(UTurboSequence_GlobalData_Lf::StaticClass()->GetPathName()), GlobalAssetData);
		if (GlobalAssetData.Num())
		{
			FTurboSequence_Helper_Lf::SortAssetsByPathName(GlobalAssetData);

			FromAsset->GlobalData = Cast<UTurboSequence_GlobalData_Lf>(GlobalAssetData[0].GetAsset());
		}

		if (!IsValid(FromAsset->GlobalData))
		{
			UE_LOG(LogTurboSequence_Lf, Warning,
			       TEXT("Can't create Mesh Instance, there is no global data available...."));
			UE_LOG(LogTurboSequence_Lf, Error,
			       TEXT(
				       "Can not find the Global Data asset -> This is really bad, without it Turbo Sequence does not work, you can recover it by creating an UTurboSequence_GlobalData_Lf Data Asset, Right click in the content browser anywhere in the Project, select Data Asset and choose UTurboSequence_GlobalData_Lf, save it and restart the editor"
			       ));
			return FBaseSkeletalMeshHandle();
		}
	}

	if (!IsValid(FromAsset->GlobalData->TransformTexture_CurrentFrame) || !IsValid(
		FromAsset->GlobalData->TransformTexture_PreviousFrame))
	{
		UE_LOG(LogTurboSequence_Lf, Warning,
		       TEXT(
			       "Can't create Mesh Instance, The Transform Texture is missing in the global settings... that's pretty bad"
		       ));
		UE_LOG(LogTurboSequence_Lf, Error,
		       TEXT(
			       "Can not find Transform Texture ... "
		       ));
		return FBaseSkeletalMeshHandle();
	}

	if (!IsValid(WorldContextObject))
	{
		UE_LOG(LogTurboSequence_Lf, Warning, TEXT("Can't create Mesh Instance, the World is not valid..."));
		return FBaseSkeletalMeshHandle();
	}

	if (!IsValid(Instance))
	{
		Instance = Cast<ATurboSequence_Manager_Lf>(
			UGameplayStatics::GetActorOfClass(WorldContextObject, StaticClass()));

		if (!IsValid(Instance)) // Is not in the map, create it
		{
			Instance = Cast<ATurboSequence_Manager_Lf>(
				WorldContextObject->GetWorld()->SpawnActor(StaticClass()));

			if (!IsValid(Instance))
			{
				UE_LOG(LogTurboSequence_Lf, Warning,
				       TEXT(
					       "Can't create Mesh Instance because Instance is not valid, make sure to have a ATurboSequence_Manager_Lf in the map"
				       ));
				return FBaseSkeletalMeshHandle();
			}
		}

		GlobalLibrary_RenderThread.BoneTransformParams.ShaderID = GetTypeHash(Instance->GlobalData);
	}

	if (IsValid(Instance) && !IsValid(Instance->GetRootComponent()))
	{
		UE_LOG(LogTurboSequence_Lf, Warning,
		       TEXT(
			       "Can't create Mesh Instance, make sure to have ATurboSequence_Manager_Lf as a blueprint in the map"
		       ));
		return FBaseSkeletalMeshHandle();
	}

	Instance->GlobalData = FromAsset->GlobalData;

	if (!FromAsset->IsMeshAssetValid())
	{
		return FBaseSkeletalMeshHandle();
	}
	
	FScopeLock ScopeLock(&Instance->RenderThreadDataConsistency);

	const FTurboSequenceRenderHandle RenderHandle(OverrideMaterials,FromAsset->RendererSystem, FromAsset->StaticMesh, bInRenderInCustomDepth, InStencilValue, bNewReceivesDecals, LightingChannels );

	UTurboSequence_RenderData** TurboSequenceRenderDataPtr = Instance->GlobalLibrary.PerReferenceData.Find(RenderHandle);

	UTurboSequence_RenderData* RenderData; 
	
	if(TurboSequenceRenderDataPtr)
	{
		RenderData = *TurboSequenceRenderDataPtr;
	}
	else
	{
		UE_LOG(LogTurboSequence_Lf, Display, TEXT("Adding Mesh Asset to Library at Path | -> %s"), *FromAsset->GetPathName());

		RenderData = UTurboSequence_RenderData::CreateObject(Instance,FromAsset,FromAsset->StaticMesh );

		RenderData->SpawnNiagaraComponent(OverrideMaterials.Num() > 0 ? OverrideMaterials : FromAsset->Materials,Instance->Root,FromAsset->RendererSystem,FromAsset->StaticMesh, bNewReceivesDecals, LightingChannels, bInRenderInCustomDepth, InStencilValue);
		
		Instance->GlobalLibrary.PerReferenceData.Add(RenderHandle, RenderData);
	}
	
	if (!Instance->GlobalLibrary.PerReferenceDataKeys.Contains(FromAsset))
	{
		Instance->GlobalLibrary.PerReferenceDataKeys.Add(FromAsset);

		FTurboSequence_Utility_Lf::UpdateMaxBones(Instance->GlobalLibrary);
		
		Instance->GlobalLibrary.bRefreshAsyncChunkedMeshData = true;
	}
	
	// Now it's time to add the actual instance
	const FBaseSkeletalMeshHandle MeshID(Instance->GlobalLibrary.GlobalMeshCounter++);
	const int32 SizeInBoneTexture = FromAsset->GetNumGPUBones() * 3; //Translation + Rotation + Scale
	const int32 BoneTextureSkeletonIndex = Instance->GlobalLibrary.BoneTextureAllocator.Allocate(SizeInBoneTexture); 

	//UE_LOG(LogTurboSequence_Lf, Display, TEXT("Allocation %d in the bone texture at %d"), SizeInBoneTexture, BoneTextureSkeletonIndex);

	FSkinnedMeshRuntime_Lf Runtime = FSkinnedMeshRuntime_Lf(MeshID, FromAsset, RenderHandle, BoneTextureSkeletonIndex);
	Runtime.WorldSpaceTransform = SpawnTransform;

	RenderData->AddRenderInstance(MeshID, SpawnTransform, BoneTextureSkeletonIndex);
	
	Instance->GlobalLibrary.RuntimeSkinnedMeshes.Add(MeshID, Runtime);
	
	FTurboSequence_AnimPlaySettings_Lf PlaySetting = FTurboSequence_AnimPlaySettings_Lf();
	PlaySetting.ForceMode = ETurboSequence_AnimationForceMode_Lf::AllLayers;
	PlaySetting.RootMotionMode = ETurboSequence_RootMotionMode_Lf::None;

	PlayAnimation(MeshID, FromAsset->OverrideDefaultAnimation, PlaySetting);
	
	return MeshID;
}

void ATurboSequence_Manager_Lf::RemoveRenderHandle(const FTurboSequenceRenderHandle RenderHandle, const FAttachmentMeshHandle MeshHandle, const TObjectPtr<
                                                   UTurboSequence_MeshAsset_Lf> DataAsset, bool
                                                   & bRemovedASkeleton)
{

	bRemovedASkeleton = false;

	UTurboSequence_RenderData** TurboSequenceRenderDataPtr = Instance->GlobalLibrary.PerReferenceData.Find(RenderHandle);

	ensure(TurboSequenceRenderDataPtr);
	
	if(TurboSequenceRenderDataPtr)
	{
		UTurboSequence_RenderData* RenderData = *TurboSequenceRenderDataPtr;
	
		RenderData->RemoveRenderInstance(MeshHandle);
		// Time to remove the reference, we assume here the instance has a mesh,
		// If no renderers using the component anymore, remove it

		if (RenderData->GetActiveNum() == 0)
		{
			RenderData->DestroyNiagaraComponent();
			
			Instance->GlobalLibrary.PerReferenceData.Remove(RenderHandle);

			bool bStillUsingDataAsset = false;
			
			for (const auto &PerReferenceData : Instance->GlobalLibrary.PerReferenceData)
			{
				if(PerReferenceData.Value->DataAsset == DataAsset)
				{
					bStillUsingDataAsset = true;
					break;
				}
			}

			if(!bStillUsingDataAsset)
			{
				Instance->GlobalLibrary.PerReferenceDataKeys.Remove(DataAsset);
				bRemovedASkeleton = true;
			}
		}
	}
}

bool ATurboSequence_Manager_Lf::RemoveSkinnedMeshInstance(FBaseSkeletalMeshHandle MeshID)
{
	FScopeLock ScopeLock(&Instance->RenderThreadDataConsistency);

	SCOPE_CYCLE_COUNTER(Remove_TurboSequenceMeshInstances_Lf);
	
	if (!IsValid(Instance))
	{
		return false;
	}

	FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID);

	ensure(Runtime);
	
	if (!Runtime)
	{
		return false;
	}
	
	FTurboSequence_Utility_Lf::ClearAnimations(*Runtime, Instance->GlobalLibrary, ETurboSequence_AnimationForceMode_Lf::AllLayers, TArray<FTurboSequence_BoneLayer_Lf>(),
	                                           [](const FAnimationMetaData_Lf& Animation)
	                                           {
		                                           return true;
	                                           });
	// Since we have the first animation as rest pose
	FTurboSequence_Utility_Lf::RemoveAnimation(*Runtime, Instance->GlobalLibrary, 0);
	
	bool bRemovedASkeleton;
	bool bRemovedAnySkeleton = false;

	//Remove Attachments
	for (const FSkinnedMeshAttachmentRuntime &Attachment : Runtime->Attachments)
	{
		RemoveRenderHandle(Attachment.RenderHandle, Attachment.AttachmentHandle,nullptr, bRemovedASkeleton);

		bRemovedAnySkeleton |= bRemovedASkeleton;
	}

	Runtime->Attachments.Empty();
	
	Runtime->RemoveAllAttachedParticles();
	
	RemoveRenderHandle(Runtime->RenderHandle, MeshID, Runtime->DataAsset, bRemovedASkeleton);

	bRemovedAnySkeleton |= bRemovedASkeleton;

	const int32 SizeInBoneTexture = Runtime->DataAsset->GetNumGPUBones() * 3;

	//UE_LOG(LogTurboSequence_Lf, Display, TEXT("Freeing %d in the bone texture at %d"), SizeInBoneTexture, Runtime->BoneTextureSkeletonIndex);
	Instance->GlobalLibrary.BoneTextureAllocator.Free(Runtime->BoneTextureSkeletonIndex, SizeInBoneTexture);
	
	Instance->GlobalLibrary.RuntimeSkinnedMeshes.Remove(MeshID); Runtime = nullptr;
	
	if(bRemovedAnySkeleton)
	{
		FTurboSequence_Utility_Lf::UpdateMaxBones(Instance->GlobalLibrary);
		
		Instance->GlobalLibrary.bRefreshAsyncChunkedMeshData = true;
	}

	return true;
}

// Function to get the component space reference pose of a bone
FTransform GetComponentSpaceRefPose(const FReferenceSkeleton& RefSkeleton, const int32 BoneIndex)
{
	if (BoneIndex < 0 || BoneIndex >= RefSkeleton.GetNum())
	{
		return FTransform::Identity;
	}

	// Start with the bone's transform in local space
	FTransform BoneTransform = RefSkeleton.GetRefBonePose()[BoneIndex];

	// Traverse up the hierarchy to convert to component space
	int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
	while (ParentIndex != INDEX_NONE)
	{
		BoneTransform *= RefSkeleton.GetRefBonePose()[ParentIndex];
		ParentIndex = RefSkeleton.GetParentIndex(ParentIndex);
	}

	return BoneTransform;
}

bool ATurboSequence_Manager_Lf::GetAttachmentComponentSpaceTransform(FTransform& AttachmentComponentSpaceTransform,
                                                                     int32& BoneIndexGPU, const FName
                                                                     BoneOrSocketName,
                                                                     const TObjectPtr<UTurboSequence_MeshAsset_Lf>& MeshAsset, const FTransform& Transform, const EBoneControlSpace AttachSpace, const FTransform& WorldSpaceTransform)
{
	FName AttachBoneName;
	FTransform AttachTransform;
	
	if(const USkeletalMeshSocket* Socket = MeshAsset->GetSkeleton()->FindSocket(BoneOrSocketName))
	{
		AttachBoneName = Socket->BoneName;
		AttachTransform = Socket->GetSocketLocalTransform() * Transform;
	}
	else
	{
		AttachBoneName = BoneOrSocketName;
		AttachTransform = Transform;
	}
	
	const int32 BoneIndexCPU = MeshAsset->GetBoneCPUIndex(AttachBoneName);
	
	if(BoneIndexCPU == INDEX_NONE)
	{
		BoneIndexGPU = INDEX_NONE;
		return false; //Can't find the bone
	}
	
	BoneIndexGPU = MeshAsset->GetBoneGPUIndex(BoneIndexCPU);

	if (BoneIndexGPU == INDEX_NONE )
	{
		return false; //Bone is not available in the skinned subset of bones (and parents)
	}

	const FReferenceSkeleton& ReferenceSkeleton = MeshAsset->GetReferenceSkeleton();
	
	switch (AttachSpace)
	{
	case BCS_WorldSpace:
		AttachmentComponentSpaceTransform = WorldSpaceTransform.Inverse() * AttachTransform;
		break;
	default:
	case BCS_ComponentSpace:
		AttachmentComponentSpaceTransform = AttachTransform;
		break;
	case BCS_ParentBoneSpace:
		{
			const int32 ParentIndex = ReferenceSkeleton.GetParentIndex(BoneIndexCPU);

			ensure(ParentIndex != INDEX_NONE);

			if(ParentIndex != INDEX_NONE)
			{
				AttachmentComponentSpaceTransform = AttachTransform * GetComponentSpaceRefPose( ReferenceSkeleton,ParentIndex) ;
			}
		}
		break;
	case BCS_BoneSpace:
		AttachmentComponentSpaceTransform = AttachTransform * GetComponentSpaceRefPose( ReferenceSkeleton,BoneIndexCPU) ;
		break;
	}

	return true;
}

FAttachmentMeshHandle ATurboSequence_Manager_Lf::AddInstanceAttachment(const FBaseSkeletalMeshHandle MeshID,
                                                                       UStaticMesh* StaticMesh, const FName SocketOrBoneName, const FTransform& Transform,
                                                                       const TArray<UMaterialInterface*>& OverrideMaterials, FLightingChannels LightingChannels, bool bNewReceivesDecals, const bool bInRenderInCustomDepth, const int32 InStencilValue, const EBoneControlSpace AttachSpace)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		FTransform AttachmentComponentSpaceTransform;
		int32 BoneIndexGPU;
		
		if(!GetAttachmentComponentSpaceTransform(AttachmentComponentSpaceTransform, BoneIndexGPU, SocketOrBoneName, Runtime->DataAsset, Transform,AttachSpace, Runtime->WorldSpaceTransform))
		{
			UE_LOG(LogTurboSequence_Lf, Warning, TEXT("Unable to find attachment: %s"), *SocketOrBoneName.ToString());
			return FAttachmentMeshHandle(); //Can't find the bone or is not available in the skinned subset of bones (and parents)
		}
		
		const FTurboSequenceRenderHandle AttachmentRenderHandle(OverrideMaterials,AttachmentAsset->RendererSystem, StaticMesh, bInRenderInCustomDepth, InStencilValue, bNewReceivesDecals, LightingChannels );
		UTurboSequence_RenderData** TurboSequenceRenderDataPtr = Instance->GlobalLibrary.PerReferenceData.Find(AttachmentRenderHandle);

		UTurboSequenceRenderAttachmentData* RenderData;
		if(TurboSequenceRenderDataPtr)
		{
			RenderData = Cast<UTurboSequenceRenderAttachmentData>(*TurboSequenceRenderDataPtr);
		}
		else
		{
			//Add on the bounds of the mesh we attach to
			//We don't know what orientation the mesh will be attached, so we should take the maximum absolute extrema, or maybe the radius
			//FVector MeshMaxBounds = StaticMesh->GetBounds().GetBoxExtrema(true);
			//FVector MeshMinBounds = StaticMesh->GetBounds().GetBoxExtrema(false);

			FVector MeshMaxBounds = FVector(StaticMesh->GetBounds().SphereRadius);
			FVector MeshMinBounds = FVector(-StaticMesh->GetBounds().SphereRadius);
		
			if(const UTurboSequence_RenderData* BaseRenderData = Instance->GlobalLibrary.PerReferenceData[Runtime->RenderHandle])
			{
				MeshMaxBounds += BaseRenderData->GetMeshMaxBounds();
				MeshMinBounds += BaseRenderData->GetMeshMinBounds();
			}
		
			RenderData = UTurboSequenceRenderAttachmentData::CreateObject(Instance,Runtime->DataAsset, MeshMinBounds,MeshMaxBounds);

			RenderData->SpawnNiagaraComponent(OverrideMaterials,Instance->GetRootComponent(), AttachmentAsset->RendererSystem, StaticMesh, bNewReceivesDecals, LightingChannels, bInRenderInCustomDepth, InStencilValue);

			Instance->GlobalLibrary.PerReferenceData.Add(AttachmentRenderHandle, RenderData);
		}
		
		// Now it's time to add the actual instance
		const FAttachmentMeshHandle AttachmentMeshHandle = Runtime->AddAttachment(AttachmentRenderHandle);
		RenderData->AddAttachmentRenderInstance(AttachmentMeshHandle, Runtime->WorldSpaceTransform,FTransform3f(AttachmentComponentSpaceTransform), BoneIndexGPU, Runtime->BoneTextureSkeletonIndex);
		return AttachmentMeshHandle;
	}

	return FAttachmentMeshHandle();
}

void ATurboSequence_Manager_Lf::RemoveInstanceAttachment(const FAttachmentMeshHandle AttachmentHandle)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(AttachmentHandle.GetBaseHandle()))
	{
		for(int32 AttachmentIndex = Runtime->Attachments.Num() - 1; AttachmentIndex >= 0; --AttachmentIndex)
		{
			FSkinnedMeshAttachmentRuntime& Attachment = Runtime->Attachments[AttachmentIndex];
			if(Attachment.AttachmentHandle == AttachmentHandle)
			{
				bool bRemovedASkeleton;

				RemoveRenderHandle(Attachment.RenderHandle, Attachment.AttachmentHandle,nullptr, bRemovedASkeleton);
				Runtime->Attachments.RemoveAtSwap(AttachmentIndex);
				break;
			}
		}
	}
}

UNiagaraComponent* ATurboSequence_Manager_Lf::SpawnParticleAttachedSingle(const FBaseSkeletalMeshHandle MeshID, UNiagaraSystem* RendererSystem, const FName SocketOrBoneName, const FTransform& Transform, const EBoneControlSpace
                                                                          AttachSpace)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		FTransform AttachmentComponentSpaceTransform;
		int32 BoneIndexGPU;
		
		if(!GetAttachmentComponentSpaceTransform(AttachmentComponentSpaceTransform, BoneIndexGPU, SocketOrBoneName, Runtime->DataAsset, Transform, AttachSpace, Runtime->WorldSpaceTransform))
		{
			UE_LOG(LogTurboSequence_Lf, Warning, TEXT("Unable to find attachment: %s"), *SocketOrBoneName.ToString());
			return nullptr; //Can't find the bone or is not available in the skinned subset of bones (and parents)
		}
		
		if(UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Instance->GetWorld(), RendererSystem, static_cast<const FTransform&>(Runtime->WorldSpaceTransform).GetLocation(),static_cast<const FTransform&>(Runtime->WorldSpaceTransform).Rotator(),static_cast<const FTransform&>(Runtime->WorldSpaceTransform).GetScale3D() ))
		{
			NiagaraComponent->SetIntParameter("User.SkeletonIndex", Runtime->BoneTextureSkeletonIndex);
			NiagaraComponent->SetIntParameter("User.BoneIndex", BoneIndexGPU);
			NiagaraComponent->SetVectorParameter("User.ParticleAttachment_Position", AttachmentComponentSpaceTransform.GetTranslation());
			NiagaraComponent->SetVariableVec4("User.ParticleAttachment_Rotation", FTurboSequence_Helper_Lf::ConvertQuaternionToVector4(AttachmentComponentSpaceTransform.GetRotation()));
			NiagaraComponent->SetVectorParameter("User.ParticleAttachment_Scale", AttachmentComponentSpaceTransform.GetScale3D());

			Runtime->AddAttachedParticle(NiagaraComponent);

			return NiagaraComponent;
		}
	}
	
	return nullptr;
}

UNiagaraComponent* ATurboSequence_Manager_Lf::SpawnParticleAttachedMulti(const FBaseSkeletalMeshHandle MeshID,
	UNiagaraSystem* RendererSystem, const TArray<FTurboSequence_AttachInfo>& Attachments, const EBoneControlSpace AttachSpace)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		TArray<int32> BoneIndexes;
		TArray<FVector3f> ComponentTranslations;
		TArray<FVector4f> ComponentRotations;
		TArray<FVector3f> ComponentScales;

		const FTransform& WorldSpaceTransform = Runtime->WorldSpaceTransform;
		
		for (const FTurboSequence_AttachInfo& Attachment : Attachments)
		{
			FTransform AttachmentComponentSpaceTransform;
			int32 BoneIndexGPU;
			
			if(GetAttachmentComponentSpaceTransform(AttachmentComponentSpaceTransform, BoneIndexGPU, Attachment.Name, Runtime->DataAsset, Attachment.Transform, AttachSpace, WorldSpaceTransform))
			{
				BoneIndexes.Add(BoneIndexGPU);
				ComponentTranslations.Add(FTurboSequence_Helper_Lf::ConvertVectorToVector3F(AttachmentComponentSpaceTransform.GetTranslation()));
				ComponentRotations.Add(FTurboSequence_Helper_Lf::ConvertQuaternionToVector4F(AttachmentComponentSpaceTransform.GetRotation()));
				ComponentScales.Add( FTurboSequence_Helper_Lf::ConvertVectorToVector3F(AttachmentComponentSpaceTransform.GetScale3D()));
			}
			else
			{
				UE_LOG(LogTurboSequence_Lf, Warning, TEXT("Unable to find attachment: %s"), *Attachment.Name.ToString());
			}
		}

		if(ComponentTranslations.Num() > 0)
		{
			if(UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Instance->GetWorld(), RendererSystem, WorldSpaceTransform.GetLocation(),WorldSpaceTransform.Rotator(),WorldSpaceTransform.GetScale3D() ))
			{
				NiagaraComponent->SetIntParameter("User.SkeletonIndex", Runtime->BoneTextureSkeletonIndex);

				UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32( NiagaraComponent, "User.BoneIndexes", BoneIndexes);
				UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector( NiagaraComponent, "User.ParticleAttachment_Positions", ComponentTranslations);
				UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4( NiagaraComponent, "User.ParticleAttachment_Rotations", ComponentRotations);
				UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector( NiagaraComponent, "User.ParticleAttachment_Scales", ComponentScales);
			
				Runtime->AddAttachedParticle(NiagaraComponent);

				return NiagaraComponent;
			}
		}
	}

	return nullptr;
}

UNiagaraComponent* ATurboSequence_Manager_Lf::SpawnParticle(FBaseSkeletalMeshHandle MeshID,
	UNiagaraSystem* NiagaraSystem)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		const FTransform& WorldSpaceTransform = Runtime->WorldSpaceTransform;
		
		if(UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Instance->GetWorld(), NiagaraSystem, WorldSpaceTransform.GetLocation(),WorldSpaceTransform.Rotator(),WorldSpaceTransform.GetScale3D() ))
		{
			UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMesh(NiagaraComponent,"User.StaticMesh",Runtime->DataAsset->StaticMesh  );
			NiagaraComponent->SetIntParameter("User.SkeletonIndex", Runtime->BoneTextureSkeletonIndex);
			
			Runtime->AddAttachedParticle(NiagaraComponent);

			return NiagaraComponent;
		}
	}

	return nullptr;
}

bool ATurboSequence_Manager_Lf::RemoveParticle(const FBaseSkeletalMeshHandle MeshID, UNiagaraComponent* NiagaraComponent,const bool bDestroy)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		return Runtime->RemoveAttachedParticle(NiagaraComponent, bDestroy);
	}

	return false;
}




/**
 * @brief Solves all meshes at once, call it only in the game thread
 * @param DeltaTime The DeltaTime in seconds
 * @param InWorld The world, we need it for the cameras and culling
 */
void ATurboSequence_Manager_Lf::SolveMeshes_GameThread(float DeltaTime, UWorld* InWorld)
{
	FScopeLock ScopeLock(&Instance->RenderThreadDataConsistency);
	
	SCOPE_CYCLE_COUNTER(STAT_Solve_TurboSequenceMeshes_Lf);
	const int32 SkinnedMeshCount = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Num();
	
	// When we are actually have any data, otherwise there is nothing to compute and we can return
	if (SkinnedMeshCount == 0)
	{
		return;
	}

	if (!IsValid(Instance))
	{
		return;
	}

	FTurboSequence_Utility_Lf::UpdateCameras_1(Instance->GlobalLibrary.CameraViews, LastFrameCameraTransforms, InWorld,
	                                           DeltaTime);

	if (!Instance->GlobalLibrary.CameraViews.Num())
	{
		return;
	}
	
	const int64 CurrentFrameCount = UKismetSystemLibrary::GetFrameCount();

	//Reset bounds
	for (auto& PerReferenceData : Instance->GlobalLibrary.PerReferenceData)
	{
		PerReferenceData.Value->ResetBounds();
	}

	INC_DWORD_STAT_BY(STAT_TotalMeshCount, Instance->GlobalLibrary.RuntimeSkinnedMeshes.Num());

	Instance->GlobalLibrary.AnimationLibraryDataAllocatedThisFrame.Empty();
	
	for (auto & RuntimeSkinnedMesh : Instance->GlobalLibrary.RuntimeSkinnedMeshes)
	{
		FSkinnedMeshRuntime_Lf& Runtime = RuntimeSkinnedMesh.Value;
		
		// Need to get always updated
		FTurboSequence_Utility_Lf::SolveAnimations(Runtime,
		                                           Instance->GlobalLibrary,
		                                           DeltaTime,
		                                           CurrentFrameCount);
			
		Runtime.bIsVisible = Runtime.DataAsset->bIsFrustumCullingEnabled ? FTurboSequence_Utility_Lf::IsMeshVisible(Runtime, Instance->GlobalLibrary.CameraViews) : true;

		
		//DrawDebugString(InWorld,Runtime.WorldSpaceTransform.GetLocation(), FString::Printf(TEXT("CPU %d Viz %d"),Runtime.BoneTextureSkeletonIndex, Runtime.bIsVisible),nullptr, FColor::Cyan,0 );

		if (Runtime.bIsVisible)
		{
			INC_DWORD_STAT(STAT_VisibleMeshCount);
		}
		
		UTurboSequence_RenderData* RenderData = Instance->GlobalLibrary.PerReferenceData[Runtime.RenderHandle];
			
		RenderData->UpdateRendererBounds(Runtime.WorldSpaceTransform);

		for (const FSkinnedMeshAttachmentRuntime& Attachment : Runtime.Attachments)
		{
			UTurboSequence_RenderData* AttachmentRenderData = Instance->GlobalLibrary.PerReferenceData[Attachment.RenderHandle];

			AttachmentRenderData->UpdateRendererBounds(Runtime.WorldSpaceTransform);
		}
	}

	if (Instance->GlobalLibrary.PerReferenceData.Num() && IsValid(Instance->GlobalData) && IsValid(
		Instance->GlobalData->TransformTexture_CurrentFrame))
	{
		GlobalLibrary_RenderThread.BoneTransformParams.NumDebugData = 10;

		GlobalLibrary_RenderThread.BoneTransformParams.bUse32BitTransformTexture = Instance->GlobalData->
			bUseHighPrecisionAnimationMode;

		const uint32 AnimationMaxNum = Instance->GlobalLibrary.AnimationLibraryMaxNum;

		if (Instance->GlobalLibrary.AnimationLibraryDataAllocatedThisFrame.Num())
		{
			TArray<FVector4f> AnimationData = Instance->GlobalLibrary.AnimationLibraryDataAllocatedThisFrame;

			FSkinnedMeshGlobalLibrary_RenderThread_Lf& Library_RenderThread = GlobalLibrary_RenderThread;
			
			ENQUEUE_RENDER_COMMAND(TurboSequence_AddLibraryAnimationChunked_Lf)(
				[&Library_RenderThread, AnimationData, AnimationMaxNum](FRHICommandListImmediate& RHICmdList)
				{
					Library_RenderThread.AnimationLibraryParams.SettingsInput.Append(AnimationData);

					Library_RenderThread.AnimationLibraryMaxNum = AnimationMaxNum;
				});
		}

		// for (auto & RuntimeSkinnedMesh : Instance->GlobalLibrary.RuntimeSkinnedMeshes)
		// {
		// 	const FSkinnedMeshRuntime_Lf& Runtime = RuntimeSkinnedMesh.Value;
		//
		// 	FColor LineColor(FColor::MakeRandomSeededColor(GetTypeHash(RuntimeSkinnedMesh.Key)));
		// 	LineColor.A = 255; 
		// 	FTurboSequence_Utility_Lf::DebugDrawSkeleton(Runtime, Instance->GlobalLibrary, LineColor,InWorld);
		//
		// 	//FTurboSequence_Utility_Lf::PrintAnimsToScreen(Runtime);
		// }
	}

	FTurboSequence_Utility_Lf::UpdateCameras_2(LastFrameCameraTransforms, Instance->GlobalLibrary.CameraViews);
}

void ATurboSequence_Manager_Lf::SolveMeshes_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	SCOPE_CYCLE_COUNTER(STAT_Solve_TurboSequenceMeshes_RT_Lf);
	
	if (!IsValid(Instance))
	{
		return;
	}

	FScopeLock ScopeLock(&Instance->RenderThreadDataConsistency);

	if (!Instance->GlobalLibrary.RuntimeSkinnedMeshes.Num())
	{
		return;
	}
	
	FMeshUnitComputeShader_Params_Lf& MeshParams = GlobalLibrary_RenderThread.BoneTransformParams;

	if(Instance->GlobalLibrary.bMasksNeedRebuilding || Instance->GlobalLibrary.MasksBuiltBoneCount != Instance->GlobalLibrary.MaxNumCPUBones)
	{
		Instance->GlobalLibrary.ProxyToIndex.Reset();

		int32 MaskCount = 0;

		const int32 NumCPUBones = Instance->GlobalLibrary.MaxNumCPUBones;

		const int32 NumLayers = Instance->GlobalLibrary.AnimationBlendLayerMasks.Num() * NumCPUBones;
		//MeshParams.AnimationLayers_RenderThread.SetNumUninitialized(NumLayers);

		MeshParams.AnimationLayers_RenderThread.Reset(NumLayers);
		
		for (const auto & AnimationBlendLayerMask : Instance->GlobalLibrary.AnimationBlendLayerMasks)
		{
			Instance->GlobalLibrary.ProxyToIndex.Add(AnimationBlendLayerMask.Key, MaskCount++);

			for (uint16 RawAnimationLayer : AnimationBlendLayerMask.Value.RawAnimationLayers)
			{
				MeshParams.AnimationLayers_RenderThread.Add(RawAnimationLayer);
			}

			//Pad to NumCPUBones
			for(int32 BoneIndex = AnimationBlendLayerMask.Value.RawAnimationLayers.Num(); BoneIndex < NumCPUBones; ++BoneIndex)
			{
				MeshParams.AnimationLayers_RenderThread.Add(0);
			}
		}
		
		Instance->GlobalLibrary.bMasksNeedRebuilding = false;
		Instance->GlobalLibrary.MasksBuiltBoneCount = Instance->GlobalLibrary.MaxNumCPUBones;
	}

	if(Instance->GlobalLibrary.bRefreshAsyncChunkedMeshData)
	{
		FTurboSequence_Utility_Lf::RefreshAsyncChunkedMeshData(Instance->GlobalLibrary, GlobalLibrary_RenderThread);
		
		Instance->GlobalLibrary.bRefreshAsyncChunkedMeshData = false;
	}



	int32 NumMeshesVisibleCurrentFrame = 0;

	MeshParams.PerMeshCustomDataIndex_Global_RenderThread.Reset();
	MeshParams.PerMeshCustomDataCollectionIndex_RenderThread.Reset();
	MeshParams.AnimationStartIndex_RenderThread.Reset();
	MeshParams.AnimationFramePose0_RenderThread.Reset();
	MeshParams.AnimationFramePose1_RenderThread.Reset();
	MeshParams.AnimationFrameAlpha_RenderThread.Reset();
	MeshParams.AnimationWeights_RenderThread.Reset();
	MeshParams.AnimationLayerIndex_RenderThread.Reset();
	MeshParams.AnimationEndIndex_RenderThread.Reset();
	MeshParams.BoneSpaceAnimationIKData_RenderThread.Reset();
	MeshParams.BoneSpaceAnimationIKInput_RenderThread.Reset();
	MeshParams.BoneSpaceAnimationIKStartIndex_RenderThread.Reset();
	MeshParams.BoneSpaceAnimationIKEndIndex_RenderThread.Reset();
	
	for (auto & RuntimeSkinnedMesh : Instance->GlobalLibrary.RuntimeSkinnedMeshes)
	{
		FSkinnedMeshRuntime_Lf& Runtime = RuntimeSkinnedMesh.Value;
		
		if (!Runtime.bIsVisible)
		{
			continue;
		}
		
		NumMeshesVisibleCurrentFrame++;
		
		//Mesh to skeleton reference
		MeshParams.PerMeshCustomDataIndex_Global_RenderThread.Add(Runtime.BoneTextureSkeletonIndex);

		int32 SkeletonIndex = Instance->GlobalLibrary.PerReferenceDataKeys.Find(Runtime.DataAsset);
		
		ensure(SkeletonIndex != INDEX_NONE);
		
		MeshParams.PerMeshCustomDataCollectionIndex_RenderThread.Add(SkeletonIndex);
		
		//Animations
		int32 LastAnimationIndex = MeshParams.AnimationFramePose0_RenderThread.Num();
		
		MeshParams.AnimationStartIndex_RenderThread.Add(LastAnimationIndex);

		for (const FAnimationMetaData_Lf& Animation : Runtime.AnimationMetaData)
		{
			MeshParams.AnimationFramePose0_RenderThread.Add(Animation.GPUAnimationIndex_0);
			MeshParams.AnimationFramePose1_RenderThread.Add(Animation.GPUAnimationIndex_1);
			MeshParams.AnimationFrameAlpha_RenderThread.Add(Animation.FrameAlpha * 0x7FFF);
			MeshParams.AnimationWeights_RenderThread.Add(Animation.FinalAnimationWeight * 0x7FFF);

			FBoneMaskBuiltProxyHandle BoneMaskBuiltProxyHandle = Animation.Settings.MaskDefinition.GetBuiltProxyHandle(Runtime.DataAsset);

			int* LayerMaskIndex = Instance->GlobalLibrary.ProxyToIndex.Find(BoneMaskBuiltProxyHandle);

			MeshParams.AnimationLayerIndex_RenderThread.Add(*LayerMaskIndex);
		}

		int32 AnimationCount = Runtime.AnimationMetaData.Num();
		MeshParams.AnimationEndIndex_RenderThread.Add(AnimationCount);

		//ID data
		int32 NumIKBones = 0;
		int32 LastIKDataIndex = MeshParams.BoneSpaceAnimationIKData_RenderThread.Num();

		for (auto & IKData : Runtime.OverrideBoneTransforms)
		{
			const int32 CPUBoneIndex = IKData.Key;
			
			if(const int* GPUBoneIndex = Runtime.DataAsset->CPUBoneToGPUBoneIndicesMap.Find(CPUBoneIndex))
			{
				MeshParams.BoneSpaceAnimationIKData_RenderThread.Add(*GPUBoneIndex);

				const FMatrix& BoneMatrix = IKData.Value.OverrideTransform.ToMatrixWithScale();

				for (int32 M = 0; M < 3; ++M)
				{
					FVector4f BoneData;
					BoneData.X = BoneMatrix.M[0][M];
					BoneData.Y = BoneMatrix.M[1][M];
					BoneData.Z = BoneMatrix.M[2][M];
					BoneData.W = BoneMatrix.M[3][M];

					MeshParams.BoneSpaceAnimationIKInput_RenderThread.Add(BoneData);
				}
						
				NumIKBones++;
			}
		}

		MeshParams.BoneSpaceAnimationIKStartIndex_RenderThread.Add(LastIKDataIndex);
		MeshParams.BoneSpaceAnimationIKEndIndex_RenderThread.Add(NumIKBones);
	}

	MeshParams.ReferenceNumCPUBones_RenderThread.Reset();
	
	for (int32 i = 0; i < MeshParams.ReferenceNumCPUBones.Num(); ++i)
	{
		MeshParams.ReferenceNumCPUBones_RenderThread.Add(MeshParams.ReferenceNumCPUBones[i]);
	}

	MeshParams.NumMeshes = NumMeshesVisibleCurrentFrame;

	//Need elements in all arrays (seems to be a compute requirement)
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.ReferenceNumCPUBones_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.PerMeshCustomDataIndex_Global_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.PerMeshCustomDataCollectionIndex_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.AnimationStartIndex_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.AnimationEndIndex_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.AnimationFrameAlpha_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.AnimationFramePose0_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.AnimationFramePose1_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.AnimationWeights_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.AnimationLayerIndex_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.BoneSpaceAnimationIKData_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.BoneSpaceAnimationIKInput_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.BoneSpaceAnimationIKStartIndex_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.BoneSpaceAnimationIKEndIndex_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.AnimationLayers_RenderThread);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.CPUInverseReferencePose);
	FTurboSequence_Helper_Lf::CheckArrayHasSize(MeshParams.Indices);
}

void ATurboSequence_Manager_Lf::CleanManager_GameThread(bool bIsEndPlay)
{
	if (bIsEndPlay && IsValid(Instance))
	{
		ENQUEUE_RENDER_COMMAND(TurboSequence_EndPlayBufferClear_Lf)(
			[&](FRHICommandListImmediate& RHICmdList)
			{
				ClearBuffers();
			});

		Instance = nullptr;
	}
	else
	{
		ClearBuffers();
	}
}

FTransform ATurboSequence_Manager_Lf::GetMeshWorldSpaceTransform(FBaseSkeletalMeshHandle MeshID)
{
	if(const FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		return Runtime->WorldSpaceTransform;
	}

	return FTransform::Identity;
}

void ATurboSequence_Manager_Lf::SetMeshWorldSpaceTransform(
	const FBaseSkeletalMeshHandle MeshID, const FTransform& Transform)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		Runtime->WorldSpaceTransform = Transform;

		UTurboSequence_RenderData* RenderData = Instance->GlobalLibrary.PerReferenceData[Runtime->RenderHandle];

		RenderData->UpdateInstanceTransform(MeshID, Transform);

		//Also update the attachment transforms
		for (const auto& [AttachmentHandle, RenderHandle] : Runtime->Attachments)
		{
			UTurboSequence_RenderData* AttachmentRenderData = Instance->GlobalLibrary.PerReferenceData[RenderHandle];
			
			AttachmentRenderData->UpdateInstanceTransform(AttachmentHandle, Transform);
		}

		for (UNiagaraComponent* NiagaraComponent : Runtime->AttachedParticles)
		{
			NiagaraComponent->SetWorldTransform(Transform);
		}
	}
}

bool ATurboSequence_Manager_Lf::GetRootMotionTransform(FTransform& OutRootMotion, FBaseSkeletalMeshHandle MeshID,
                                                                        float DeltaTime, const EBoneSpaces::Type Space)
{
	if (!Instance->GlobalLibrary.RuntimeSkinnedMeshes.Contains(MeshID))
	{
		return false;
	}

	const FSkinnedMeshRuntime_Lf& Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes[MeshID];
	FTurboSequence_Utility_Lf::ExtractRootMotionFromAnimations(OutRootMotion, Runtime, DeltaTime);

	if (Space == EBoneSpaces::ComponentSpace)
	{
		return true;
	}

	OutRootMotion *= Runtime.WorldSpaceTransform;

	return true;
}

void ATurboSequence_Manager_Lf::MoveMeshWithRootMotion(FBaseSkeletalMeshHandle MeshID, float DeltaTime, bool ZeroZAxis,
                                                                        bool bIncludeScale)
{
	if (FTransform Atom; GetRootMotionTransform(Atom, MeshID, DeltaTime,
	                                                             EBoneSpaces::ComponentSpace))
	{
		if (ZeroZAxis)
		{
			FVector AtomLocation = Atom.GetLocation();
			AtomLocation.Z = 0;
			Atom.SetLocation(AtomLocation);
		}

		FSkinnedMeshRuntime_Lf& Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes[MeshID];
		Atom *= Runtime.WorldSpaceTransform;
		//Atom *= Runtime.DeltaOffsetTransform.Inverse();
		if (bIncludeScale)
		{
			Runtime.WorldSpaceTransform.SetScale3D(Atom.GetScale3D());
		}
		Runtime.WorldSpaceTransform.SetRotation(Atom.GetRotation());
		Runtime.WorldSpaceTransform.SetLocation(Atom.GetLocation());

		UTurboSequence_RenderData* RenderData = Instance->GlobalLibrary.PerReferenceData[Runtime.RenderHandle];
		
		RenderData->UpdateInstanceTransform(
			Runtime.MeshID, Runtime.WorldSpaceTransform);
		
	}
}

FTurboSequence_AnimMinimalData_Lf ATurboSequence_Manager_Lf::PlayAnimation(const FBaseSkeletalMeshHandle MeshID,
                                                                                            UAnimSequence* Animation, const FTurboSequence_AnimPlaySettings_Lf& AnimSettings, const bool bForceLoop, const bool
                                                                                            bForceFront)
{
	FScopeLock ScopeLock(&Instance->RenderThreadDataConsistency);
	
	FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID);

	if (!Runtime)
	{
		return FTurboSequence_AnimMinimalData_Lf(false);
	}
	
	bool bLoop = true;
	if (IsValid(Animation)) // Cause of rest pose will pass here
	{
		bLoop = Animation->bLoop || bForceLoop;
	}
	
	FTurboSequence_AnimMinimalData_Lf MinimalAnimation = FTurboSequence_AnimMinimalData_Lf(true);
	MinimalAnimation.BelongsToMeshID = MeshID;
	MinimalAnimation.AnimationID = FTurboSequence_Utility_Lf::PlayAnimation(
		Instance->GlobalLibrary, *Runtime, Animation, AnimSettings,
		bLoop, INDEX_NONE, INDEX_NONE, INDEX_NONE, bForceFront);
	return MinimalAnimation;
}

FTurboSequence_AnimMinimalBlendSpace_Lf ATurboSequence_Manager_Lf::PlayBlendSpace(FBaseSkeletalMeshHandle MeshID,
	UBlendSpace* BlendSpace, const FTurboSequence_AnimPlaySettings_Lf& AnimSettings)
{
	if (!IsValid(BlendSpace))
	{
		return FTurboSequence_AnimMinimalBlendSpace_Lf(false);
	}

	if (!Instance->GlobalLibrary.RuntimeSkinnedMeshes.Contains(MeshID))
	{
		return FTurboSequence_AnimMinimalBlendSpace_Lf(false);
	}

	FSkinnedMeshRuntime_Lf& Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes[MeshID];
	
	const TObjectPtr<UTurboSequence_ThreadContext_Lf> ThreadContext = Instance->GetThreadContext();
	return FTurboSequence_Utility_Lf::PlayBlendSpace(Instance->GlobalLibrary, Runtime, BlendSpace, ThreadContext,
	                                                 AnimSettings);
}

UAnimSequence* ATurboSequence_Manager_Lf::GetHighestPriorityPlayingAnimation_RawID_Concurrent(FBaseSkeletalMeshHandle MeshID)
{
	if (!Instance->GlobalLibrary.RuntimeSkinnedMeshes.Contains(MeshID))
	{
		return nullptr;
	}

	const FSkinnedMeshRuntime_Lf& Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes[MeshID];
	return FTurboSequence_Utility_Lf::GetHighestPriorityAnimation(Runtime);
}

void ATurboSequence_Manager_Lf::GetAnimNotifies(FBaseSkeletalMeshHandle MeshID, FTurboSequence_AnimNotifyQueue_Lf& NotifyQueue)
{
	if (const FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		FTurboSequence_Utility_Lf::GetAnimNotifies(*Runtime, NotifyQueue);
	}
}

bool ATurboSequence_Manager_Lf::TweakAnimation(const FTurboSequence_AnimPlaySettings_Lf& TweakSettings,
                                                          const FTurboSequence_AnimMinimalData_Lf& AnimationData)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(AnimationData.BelongsToMeshID))
	{
		return FTurboSequence_Utility_Lf::TweakAnimation(*Runtime, Instance->GlobalLibrary,
		                                                 TweakSettings, AnimationData.AnimationID);
	}
	
	return false;
}

bool ATurboSequence_Manager_Lf::TweakBlendSpace(FBaseSkeletalMeshHandle MeshID,
                                                                 const FTurboSequence_AnimMinimalBlendSpace_Lf&
                                                                 BlendSpaceData, const FVector3f& WantedPosition)
{
	if (!BlendSpaceData.IsAnimBlendSpaceValid())
	{
		return false;
	}

	if (!Instance->GlobalLibrary.RuntimeSkinnedMeshes.Contains(MeshID))
	{
		return false;
	}

	FSkinnedMeshRuntime_Lf& Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes[MeshID];

	return FTurboSequence_Utility_Lf::TweakBlendSpace(Runtime, BlendSpaceData,
	                                                  WantedPosition);
}

bool ATurboSequence_Manager_Lf::GetAnimationSettings(FTurboSequence_AnimPlaySettings_Lf& AnimationSettings,
                                                                const FTurboSequence_AnimMinimalData_Lf& AnimationData)
{
	if(const FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(AnimationData.BelongsToMeshID))
	{
		for (const FAnimationMetaData_Lf& AnimationMetaData : Runtime->AnimationMetaData)
		{
			if(AnimationMetaData.AnimationID == AnimationData.AnimationID)
			{
				AnimationSettings = AnimationMetaData.Settings;
				return true;
			}
		}
	}

	return false;
}

bool ATurboSequence_Manager_Lf::GetAnimationMetaData(FAnimationMetaData_Lf& AnimationMetaData,
                                                                const FTurboSequence_AnimMinimalData_Lf& AnimationData)
{
	if(const FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(AnimationData.BelongsToMeshID))
	{
		for (const FAnimationMetaData_Lf& RuntimeAnimationMetaData : Runtime->AnimationMetaData)
		{
			if(RuntimeAnimationMetaData.AnimationID == AnimationData.AnimationID)
			{
				AnimationMetaData = RuntimeAnimationMetaData;
				return true;
			}
		}
	}

	return false;
}

void ATurboSequence_Manager_Lf::SetAnimationTick(FBaseSkeletalMeshHandle MeshID, const bool bInTickEnabled)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		Runtime->bAnimTickEnabled = bInTickEnabled;
	}
}

bool ATurboSequence_Manager_Lf::GetBoneTransform(FTransform& OutIKTransform, const FBaseSkeletalMeshHandle MeshID,
                                                 const FName& BoneName,
                                                 const EBoneSpaces::Type Space)
{
	if(const FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		const FReferenceSkeleton& ReferenceSkeleton =
			FTurboSequence_Utility_Lf::GetReferenceSkeleton(Runtime->DataAsset);

		const int32 BoneIndexFromName = FTurboSequence_Utility_Lf::GetSkeletonBoneIndex(ReferenceSkeleton, BoneName);
		
		if (FTurboSequence_Utility_Lf::GetSkeletonIsValidIndex(ReferenceSkeleton, BoneIndexFromName))
		{
			FTurboSequence_Utility_Lf::GetBoneTransform(OutIKTransform, BoneIndexFromName, *Runtime,
														Instance->GlobalLibrary, Space);

			return true;
		}

	}

	OutIKTransform = FTransform::Identity;

	return false;
}

bool ATurboSequence_Manager_Lf::UpdateOverrideBoneTransform(FBaseSkeletalMeshHandle MeshID, const FName& BoneName,
                                                                const FTransform& Transform,
                                                                const EBoneSpaces::Type Space)
{
	FScopeLock ScopeLock(&Instance->RenderThreadDataConsistency);
	
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		if (!IsValid(Runtime->DataAsset))
		{
			return false;
		}

		const FReferenceSkeleton& ReferenceSkeleton = FTurboSequence_Utility_Lf::GetReferenceSkeleton(Runtime->DataAsset);

		const int32 BoneIndexFromName = FTurboSequence_Utility_Lf::GetSkeletonBoneIndex(ReferenceSkeleton, BoneName);
		
		if (FTurboSequence_Utility_Lf::GetSkeletonIsValidIndex(ReferenceSkeleton, BoneIndexFromName))
		{
			return FTurboSequence_Utility_Lf::OverrideBoneTransform(Transform, BoneIndexFromName, *Runtime,
			                                                        Space);
		}
	}
	
	return false;
}

bool ATurboSequence_Manager_Lf::RemoveOverrideBoneTransform(FBaseSkeletalMeshHandle MeshID, const FName& BoneName)
{
	FScopeLock ScopeLock(&Instance->RenderThreadDataConsistency);
	
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		if (!IsValid(Runtime->DataAsset))
		{
			return false;
		}

		const FReferenceSkeleton& ReferenceSkeleton = FTurboSequence_Utility_Lf::GetReferenceSkeleton(Runtime->DataAsset);

		const int32 BoneIndexFromName = FTurboSequence_Utility_Lf::GetSkeletonBoneIndex(ReferenceSkeleton, BoneName);
		
		if (FTurboSequence_Utility_Lf::GetSkeletonIsValidIndex(ReferenceSkeleton, BoneIndexFromName))
		{
			return FTurboSequence_Utility_Lf::RemoveOverrideBoneTransform(BoneIndexFromName, *Runtime);
		}
	}

	return false;
	
}

bool ATurboSequence_Manager_Lf::GetSocketTransform(FTransform& OutSocketTransform, FBaseSkeletalMeshHandle MeshID,
                                                                    const FName& SocketName,
                                                                    const EBoneSpaces::Type Space)
{
	
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{	
		if (IsValid(Runtime->DataAsset) && IsValid(Runtime->DataAsset->ReferenceMeshNative))
		{
			return FTurboSequence_Utility_Lf::GetSocketTransform(OutSocketTransform, SocketName, *Runtime,
														  Instance->GlobalLibrary, Space);
		}

	}

	OutSocketTransform = FTransform::Identity;

	return false;
}

bool ATurboSequence_Manager_Lf::GetBoneTransforms_RawID_Concurrent(TArray<FTransform>& OutBoneTransforms, const FBaseSkeletalMeshHandle MeshID,
                                                                   const TArray<FName>& BoneNames,
                                                                   const EBoneSpaces::Type Space)
{
	if(FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		if (IsValid(Runtime->DataAsset) && IsValid(Runtime->DataAsset->ReferenceMeshNative))
		{
			FTurboSequence_Utility_Lf::GetBoneTransforms(OutBoneTransforms, BoneNames, *Runtime,
													 Instance->GlobalLibrary, Space);

			return true;
		}
	}
	
	return false;
}


FTurboSequence_PoseCurveData_Lf ATurboSequence_Manager_Lf::GetAnimationCurveValue(FBaseSkeletalMeshHandle MeshID,
	const FName& CurveName, UAnimSequence* Animation)
{
	if (!Instance->GlobalLibrary.RuntimeSkinnedMeshes.Contains(MeshID))
	{
		return FTurboSequence_PoseCurveData_Lf();
	}

	const FSkinnedMeshRuntime_Lf& Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes[MeshID];
	//const FSkinnedMeshReference_Lf& Reference = Instance->GlobalLibrary.PerReferenceData[Runtime.DataAsset];

	if (!Instance->GlobalLibrary.AnimationLibraryData.Contains(
		FTurboSequence_Utility_Lf::GetAnimationLibraryKey(Runtime.DataAsset->GetSkeleton(), Runtime.DataAsset,
		                                                  Animation)))
	{
		return FTurboSequence_PoseCurveData_Lf();
	}

	FAnimationMetaData_Lf AnimationMetaData;
	for (const FAnimationMetaData_Lf& MetaData : Runtime.AnimationMetaData)
	{
		if (MetaData.Animation == Animation)
		{
			AnimationMetaData = MetaData;
			break;
		}
	}

	if (!IsValid(AnimationMetaData.Animation))
	{
		return FTurboSequence_PoseCurveData_Lf();
	}

	return FTurboSequence_Utility_Lf::GetAnimationCurveByAnimation(AnimationMetaData, Instance->GlobalLibrary, CurveName);
}

bool ATurboSequence_Manager_Lf::SetInstanceCustomData(
	const FBaseSkeletalMeshHandle MeshID, const int Index,
	const float Value, const bool bSetAttachments) 
{
	if(const FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		if(bSetAttachments)
		{
			for (const FSkinnedMeshAttachmentRuntime& Attachment : Runtime->Attachments)
			{
				UTurboSequence_RenderData* TurboSequenceRenderData = Instance->GlobalLibrary.PerReferenceData[Attachment.RenderHandle];
				
				TurboSequenceRenderData->SetCustomDataForInstance(Attachment.AttachmentHandle, Index, Value);
			}
		}
		
		UTurboSequence_RenderData* RenderData = Instance->GlobalLibrary.PerReferenceData[Runtime->RenderHandle];
		
		return RenderData->SetCustomDataForInstance(MeshID, Index, Value);
	}
	
	return false;
}

bool ATurboSequence_Manager_Lf::SetInstanceCustomDataArray(
	const FBaseSkeletalMeshHandle MeshID, const int StartIndex,
	const TArray<float>& Values, const bool bSetAttachments) 
{
	if(const FSkinnedMeshRuntime_Lf* Runtime = Instance->GlobalLibrary.RuntimeSkinnedMeshes.Find(MeshID))
	{
		if(bSetAttachments)
		{
			for (const FSkinnedMeshAttachmentRuntime& Attachment : Runtime->Attachments)
			{
				UTurboSequence_RenderData* TurboSequenceRenderData = Instance->GlobalLibrary.PerReferenceData[Attachment.RenderHandle];
				
				TurboSequenceRenderData->SetCustomDataArrayForInstance(Attachment.AttachmentHandle, StartIndex, Values);
			}
		}
		
		UTurboSequence_RenderData* RenderData = Instance->GlobalLibrary.PerReferenceData[Runtime->RenderHandle];
		
		return RenderData->SetCustomDataArrayForInstance(MeshID, StartIndex, Values);
	}
	
	return false;
}

bool ATurboSequence_Manager_Lf::SetTransitionTsMeshToUEMesh(const FBaseSkeletalMeshHandle TsMeshID, USkinnedMeshComponent* UEMesh,
                                                            const float UEMeshPercentage,
                                                            const float AnimationDeltaTime)
{
	if (!IsValid(UEMesh))
	{
		return false;
	}
	if (!Instance->GlobalLibrary.RuntimeSkinnedMeshes.Contains(TsMeshID))
	{
		return false;
	}


	const FReferenceSkeleton& ReferenceSkeleton =
		FTurboSequence_Utility_Lf::GetReferenceSkeleton_Raw(
			UEMesh->GetSkinnedAsset());

	const int32 NumBones = ReferenceSkeleton.GetNum();
	for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
	{
		const FName& BoneName = UEMesh->GetBoneName(BoneIdx);

		FTransform TurboSequenceIKTransform;
		GetBoneTransform(
			TurboSequenceIKTransform, TsMeshID, BoneName);

		const FTransform& SkeletalMeshIKTransform = UEMesh->GetBoneTransform(BoneIdx);

		TurboSequenceIKTransform.SetScale3D(FMath::Lerp(
			TurboSequenceIKTransform.GetScale3D(), SkeletalMeshIKTransform.GetScale3D(),
			UEMeshPercentage));
		TurboSequenceIKTransform.SetLocation(FMath::Lerp(
			TurboSequenceIKTransform.GetLocation(), SkeletalMeshIKTransform.GetLocation(),
			UEMeshPercentage));
		TurboSequenceIKTransform.SetRotation(FQuat::Slerp(
			TurboSequenceIKTransform.GetRotation(), SkeletalMeshIKTransform.GetRotation(),
			UEMeshPercentage));

		UpdateOverrideBoneTransform(
			TsMeshID, BoneName,
			TurboSequenceIKTransform);
	}

	return true;
}
