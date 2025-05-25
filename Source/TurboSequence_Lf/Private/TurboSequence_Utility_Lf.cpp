// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.

#include "TurboSequence_Utility_Lf.h"

#include "MeshDescription.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "TurboSequence_ComputeShaders_Lf.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "Engine/SkeletalMeshSocket.h"


float FTurboSequence_Utility_Lf::AnimationCodecTimeToIndex(float RelativePos, int32 NumKeys,
	EAnimInterpolationType Interpolation, int32& PosIndex0Out, int32& PosIndex1Out)
{
	float Alpha;

	if (NumKeys < 2)
	{
		checkSlow(NumKeys == 1); // check if data is empty for some reason.
		PosIndex0Out = 0;
		PosIndex1Out = 0;
		return 0.0f;
	}
	// Check for before-first-frame case.
	if (RelativePos <= 0.f)
	{
		PosIndex0Out = 0;
		PosIndex1Out = 0;
		Alpha = 0.0f;
	}
	else
	{
		NumKeys -= 1; // never used without the minus one in this case
		// Check for after-last-frame case.
		if (RelativePos >= 1.0f)
		{
			// If we're not looping, key n-1 is the final key.
			PosIndex0Out = NumKeys;
			PosIndex1Out = NumKeys;
			Alpha = 0.0f;
		}
		else
		{
			// For non-looping animation, the last frame is the ending frame, and has no duration.
			const float KeyPos = RelativePos * static_cast<float>(NumKeys);
			checkSlow(KeyPos >= 0.0f);
			const float KeyPosFloor = floorf(KeyPos);
			PosIndex0Out = FMath::Min(FMath::TruncToInt(KeyPosFloor), NumKeys);
			Alpha = (Interpolation == EAnimInterpolationType::Step) ? 0.0f : KeyPos - KeyPosFloor;
			PosIndex1Out = FMath::Min(PosIndex0Out + 1, NumKeys);
		}
	}
	return Alpha;
}


void FTurboSequence_Utility_Lf::UpdateCameras(TArray<FCameraView_Lf>& OutView, const UWorld* InWorld)
{
	const bool bIsSinglePlayer = UGameplayStatics::GetNumPlayerControllers(InWorld) < 2;
	const int32 MaxNumberPlayerControllers = UGameplayStatics::GetNumPlayerControllers(InWorld);
	OutView.SetNum(MaxNumberPlayerControllers);
	for (int32 ViewIdx = 0; ViewIdx < MaxNumberPlayerControllers; ++ViewIdx)
	{
		const TObjectPtr<APlayerController> PlayerController = UGameplayStatics::GetPlayerController(
			InWorld, ViewIdx);
		const TObjectPtr<ULocalPlayer> LocalPlayer = PlayerController->GetLocalPlayer();

		PlayerController->PlayerCameraManager->UpdateCamera(0);

		const FMinimalViewInfo& PlayerMinimalViewInfo = PlayerController->PlayerCameraManager->ViewTarget.POV;

		FVector2D ViewportSize;
		LocalPlayer->ViewportClient->GetViewportSize(ViewportSize);
		const FVector2D SplitScreenPlayerViewSize(LocalPlayer->Size.X * ViewportSize.X,
		                                          LocalPlayer->Size.Y * ViewportSize.Y);

		FVector2f ViewportDimensions = FVector2f(ViewportSize.X, ViewportSize.Y);

		float Fov = PlayerController->PlayerCameraManager->GetFOVAngle();
		if (!bIsSinglePlayer)
		{
			ViewportDimensions = FVector2f(SplitScreenPlayerViewSize.X / SplitScreenPlayerViewSize.Y);


			float AspectRatio;
			if (((ViewportSize.X > ViewportSize.Y) && (LocalPlayer->AspectRatioAxisConstraint ==
				AspectRatio_MajorAxisFOV)) || (LocalPlayer->AspectRatioAxisConstraint == AspectRatio_MaintainXFOV))
			{
				//if the viewport is wider than it is tall
				AspectRatio = ViewportDimensions.X / ViewportDimensions.Y;
			}
			else
			{
				//if the viewport is taller than it is wide
				AspectRatio = ViewportDimensions.Y / ViewportDimensions.X;
			}

			Fov = FMath::RadiansToDegrees(2 * FMath::Atan(
				FMath::Tan(FMath::DegreesToRadians(Fov) * 0.5f) * AspectRatio));
		}

		FCameraView_Lf View;
		View.Fov = Fov; // * 1.1f;
		View.ViewportSize = ViewportDimensions;
		View.AspectRatioAxisConstraint = LocalPlayer->AspectRatioAxisConstraint;
		View.bIsPerspective = PlayerMinimalViewInfo.ProjectionMode == ECameraProjectionMode::Perspective;
		View.OrthoWidth = PlayerMinimalViewInfo.OrthoWidth;
		View.FarClipPlane = PlayerMinimalViewInfo.OrthoFarClipPlane;
		// Near clipping plane need to have an offset backwards because otherwise characters disappear
		// in front of the camera
		View.NearClipPlane = -100;
		View.CameraTransform = FTransform(PlayerMinimalViewInfo.Rotation, PlayerMinimalViewInfo.Location,
		                                  FVector::OneVector);

		OutView[ViewIdx] = View;
	}
}

void FTurboSequence_Utility_Lf::UpdateCameras_1(TArray<FCameraView_Lf>& OutViews,
                                                const TMap<uint8, FTransform>& LastFrameCameraTransforms,
                                                const UWorld* InWorld, float DeltaTime)
{
	UpdateCameras(OutViews, InWorld);
	uint8 NumCameraViews = OutViews.Num();
	for (uint8 i = 0; i < NumCameraViews; ++i)
	{
		FCameraView_Lf& View = OutViews[i];

		float InterpolatedFov = View.Fov; // + 5 * DeltaTime;
		FTurboSequence_Helper_Lf::GetCameraFrustumPlanes_ObjectSpace(
			View.Planes_Internal, InterpolatedFov, View.ViewportSize, View.AspectRatioAxisConstraint,
			View.NearClipPlane, View.FarClipPlane,
			!View.bIsPerspective, View.OrthoWidth);

		if (LastFrameCameraTransforms.Contains(i))
		{
			constexpr float Interpolation = 1;
			const FVector InterpolatedCameraLocation = View.CameraTransform.GetLocation();
			const FQuat InterpolatedCameraRotation = FQuat::SlerpFullPath(
				LastFrameCameraTransforms[i].GetRotation(), View.CameraTransform.GetRotation(), Interpolation);

			View.InterpolatedCameraTransform_Internal = FTransform(InterpolatedCameraRotation,
			                                                       InterpolatedCameraLocation, FVector::OneVector);
		}
		else
		{
			View.InterpolatedCameraTransform_Internal = View.CameraTransform;
		}
	}
}

void FTurboSequence_Utility_Lf::UpdateCameras_2(TMap<uint8, FTransform>& OutLastFrameCameraTransforms,
                                                const TArray<FCameraView_Lf>& CameraViews)
{
	uint8 MaxNumberLastFrameCameras = FMath::Max(OutLastFrameCameraTransforms.Num(), CameraViews.Num());
	for (uint8 i = 0; i < MaxNumberLastFrameCameras; ++i)
	{
		if (OutLastFrameCameraTransforms.Contains(i))
		{
			if (CameraViews.Num() <= i)
			{
				OutLastFrameCameraTransforms.Remove(i);
			}
			else
			{
				OutLastFrameCameraTransforms[i] = CameraViews[i].CameraTransform;
			}
		}
		else
		{
			OutLastFrameCameraTransforms.Add(i, CameraViews[i].CameraTransform);
		}
	}
}

bool FTurboSequence_Utility_Lf::IsMeshVisible(const FSkinnedMeshRuntime_Lf& Runtime,
                                              const TArray<FCameraView_Lf>& PlayerViews)
{
	const FVector& MeshLocation = Runtime.WorldSpaceTransform.GetLocation();
	const FBoxSphereBounds& Bounds = Runtime.DataAsset->StaticMesh->GetBounds();
	const FBox Box(MeshLocation - Bounds.BoxExtent, MeshLocation + Bounds.BoxExtent);

	bool bIsVisibleOnAnyCamera = false;
	for (const FCameraView_Lf& View : PlayerViews)
	{
		if (FTurboSequence_Helper_Lf::Box_Intersects_With_Frustum(Box, View.Planes_Internal,
		                                                          View.InterpolatedCameraTransform_Internal,
		                                                          Bounds.SphereRadius))
		{
			bIsVisibleOnAnyCamera = true;
			break;
		}
	}

	return bIsVisibleOnAnyCamera;
}

void FTurboSequence_Utility_Lf::RefreshAsyncChunkedMeshData(
	FSkinnedMeshGlobalLibrary_Lf& Library,
	FSkinnedMeshGlobalLibrary_RenderThread_Lf& Library_RenderThread)
{
	CreateBoneMaps(Library, Library_RenderThread);
	CreateInverseReferencePose(Library, Library_RenderThread);
}

void FTurboSequence_Utility_Lf::UpdateMaxBones(FSkinnedMeshGlobalLibrary_Lf& Library)
{
	int32 MaxNumCPUBones = 0;
	int32 MaxNumGPUBones = 0;
	
	for (const UTurboSequence_MeshAsset_Lf* Asset : Library.PerReferenceDataKeys) 
	{
		MaxNumCPUBones = FMath::Max(MaxNumCPUBones, Asset->GetNumCPUBones());
		MaxNumGPUBones = FMath::Max(MaxNumGPUBones, Asset->GetNumGPUBones());
	}

	Library.MaxNumCPUBones = MaxNumCPUBones;
	Library.MaxNumGPUBones = MaxNumGPUBones;
}

void FTurboSequence_Utility_Lf::CreateBoneMaps(FSkinnedMeshGlobalLibrary_Lf& Library,
                                               FSkinnedMeshGlobalLibrary_RenderThread_Lf& Library_RenderThread)
{
	if (Library.PerReferenceData.Num() == 0)
	{
		return;
	}

	const int32 MaxNumCPUBones = Library.MaxNumCPUBones;
	const int32 MaxNumGPUBones = Library.MaxNumGPUBones;
		
	Library_RenderThread.BoneTransformParams.NumMaxCPUBones = MaxNumCPUBones;

	Library_RenderThread.BoneTransformParams.ReferenceNumCPUBones.Reset();

	for (const UTurboSequence_MeshAsset_Lf* TurboSequence_MeshAsset_Lf : Library.PerReferenceDataKeys)
	{
		Library_RenderThread.BoneTransformParams.ReferenceNumCPUBones.Add(TurboSequence_MeshAsset_Lf->GetNumCPUBones());
	}

	const int32 NumReferences = Library.PerReferenceDataKeys.Num();

	TArray<FVector4f> CachedIndices;
	const int32 NumBoneIndices = NumReferences * MaxNumCPUBones;
	CachedIndices.AddUninitialized(NumBoneIndices);

	FVector4f Pad;
					
	Pad.X = INDEX_NONE;
	Pad.Y = INDEX_NONE;
	Pad.Z = INDEX_NONE;
	Pad.W = INDEX_NONE;
	
	for (int32 RefIdx = 0; RefIdx < NumReferences; ++RefIdx)
	{
		const TObjectPtr<UTurboSequence_MeshAsset_Lf> Asset = Library.PerReferenceDataKeys[RefIdx];

		const int32 BaseIndex = RefIdx * MaxNumCPUBones;

		int32 Index = 0;
				
		for (const auto& CPUBoneToGPUBone : Asset->CPUBoneToGPUBoneIndicesMap)
		{
			FVector4f Data;

			Data.X = CPUBoneToGPUBone.Key;
			Data.Y = CPUBoneToGPUBone.Value;
			Data.Z = Asset->GPUParentCacheRead[Index];
			Data.W = Asset->GPUParentCacheWrite[Index];

			CachedIndices[BaseIndex + Index] = Data;
					
			++Index;
		}
				
		//Pad up till MaxNumCPUBones
		while(Index < MaxNumCPUBones)
		{		
			CachedIndices[BaseIndex + Index] = Pad;

			++Index;
		}
	}
		
	Library_RenderThread.BoneTransformParams.Indices = CachedIndices;
}

void FTurboSequence_Utility_Lf::CreateInverseReferencePose(FSkinnedMeshGlobalLibrary_Lf& Library,
                                                           FSkinnedMeshGlobalLibrary_RenderThread_Lf& Library_RenderThread)
{
	int32 NumReferences = Library.PerReferenceDataKeys.Num();
	TArray<FVector4f> InvRefPoseData;

	if (const int32 RestPoseBufferAddition = NumReferences * Library.MaxNumCPUBones * 3)
	{
		InvRefPoseData.AddUninitialized(RestPoseBufferAddition);
		
		for(int Index = 0; Index < Library.PerReferenceDataKeys.Num(); ++Index)
		{
			const TObjectPtr<UTurboSequence_MeshAsset_Lf> Asset = Library.PerReferenceDataKeys[Index];
			
			const int32 NumBones = Asset->GetNumCPUBones();
			const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(Asset);
			const TArray<FTransform>& ReferencePose = GetSkeletonRefPose(ReferenceSkeleton);
			const int32 InvRestPoseBaseIndex = Index * Library.MaxNumCPUBones * 3;
			for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
			{
				int32 RuntimeIndex = BoneIdx;
				FTransform RuntimeTransform = FTransform::Identity;
				while (RuntimeIndex != INDEX_NONE)
				{
					RuntimeTransform *= ReferencePose[RuntimeIndex];
					RuntimeIndex = GetSkeletonParentIndex(ReferenceSkeleton, RuntimeIndex);
				}
				const FMatrix InvBoneMatrix = RuntimeTransform.Inverse().ToMatrixWithScale();
				for (uint8 M = 0; M < 3; ++M)
				{
					FVector4f Row;
					Row.X = InvBoneMatrix.M[0][M];
					Row.Y = InvBoneMatrix.M[1][M];
					Row.Z = InvBoneMatrix.M[2][M];
					Row.W = InvBoneMatrix.M[3][M];

					InvRefPoseData[InvRestPoseBaseIndex + BoneIdx * 3 + M] = Row;
				}
			}
		}
	}

	Library_RenderThread.BoneTransformParams.CPUInverseReferencePose = InvRefPoseData;
}


int32 FTurboSequence_Utility_Lf::AddAnimationPoseToLibraryChunked(const int32 CPUIndex,
                                                                  FSkinnedMeshGlobalLibrary_Lf& Library,
                                                                  const FAnimationMetaData_Lf& Animation,
                                                                  FAnimationLibraryData_Lf& LibraryAnimData,
                                                                  const FReferenceSkeleton& ReferenceSkeleton,
                                                                  const FReferenceSkeleton& AnimationSkeleton)
{
	int32 GPUIndex = LibraryAnimData.KeyframesFilled[CPUIndex];

	if (GPUIndex > INDEX_NONE)
	{
		return GPUIndex;
	}

	GPUIndex = LibraryAnimData.KeyframesFilled[CPUIndex] = Library.AnimationLibraryMaxNum;

	if (!LibraryAnimData.KeyframeIndexToPose.Contains(CPUIndex))
	{
		const float FrameTime = static_cast<float>(CPUIndex) / static_cast<float>(LibraryAnimData.MaxFrames -
			1) * Animation.Animation->GetPlayLength();

		FAnimPose_Lf PoseData;
		FTurboSequence_Helper_Lf::GetPoseInfo(FrameTime, Animation.Animation, LibraryAnimData.PoseOptions,
		                                      PoseData);

		FCPUAnimationPose_Lf CPUPose;
		CPUPose.Pose = PoseData;
		LibraryAnimData.KeyframeIndexToPose.Add(CPUIndex, CPUPose);
	}

	uint32 AnimationDataIndex = Library.AnimationLibraryDataAllocatedThisFrame.Num();

	const int32 NumAllocations = LibraryAnimData.NumBones * 3;
	Library.AnimationLibraryMaxNum += NumAllocations;

	Library.AnimationLibraryDataAllocatedThisFrame.AddUninitialized(NumAllocations);
	//LibraryAnimData.AnimPoses[Pose0].RawData.AddUninitialized(NumAllocations);
	for (uint16 BoneIndex = 0; BoneIndex < LibraryAnimData.NumBones; ++BoneIndex)
	{
		const FTurboSequence_TransposeMatrix_Lf& BoneSpaceTransform = GetBoneTransformFromLocalPoses(
			BoneIndex, LibraryAnimData, ReferenceSkeleton, AnimationSkeleton, LibraryAnimData.KeyframeIndexToPose[CPUIndex].Pose,
			Animation.Animation);

		// NOTE: Keep in mind this matrix is not in correct order after uploading it
		//		 for performance reason we are using matrix calculations which match the order
		//		 in the Vertex Shader
		for (uint8 M = 0; M < 3; ++M)
		{
			Library.AnimationLibraryDataAllocatedThisFrame[AnimationDataIndex] = BoneSpaceTransform.Colum[M];
			//LibraryAnimData.AnimPoses[Pose0].RawData[b * 3 + M] = BoneData;

			AnimationDataIndex++;
		}
	}
	return GPUIndex;
}

bool FTurboSequence_Utility_Lf::AddAnimationToLibraryChunked(FSkinnedMeshGlobalLibrary_Lf& Library,
                                                             int32& CPUIndex0,
                                                             int32& GPUIndex0,
                                                             int32& CPUIndex1, int32& GPUIndex1, float& FrameAlpha,
                                                             const FSkinnedMeshRuntime_Lf& Runtime,
                                                             const FAnimationMetaData_Lf& Animation)
{
	CPUIndex0 = 0;
	CPUIndex1 = 0;
	GPUIndex0 = 0;
	GPUIndex1 = 0;
	FrameAlpha = 0;
	
	if (!Library.AnimationLibraryData.Contains(Animation.AnimationLibraryHash))
	{
		UE_LOG(LogTemp, Warning, TEXT("Don't Contains Animation Library Data"));
		return false;
	}


	FAnimationLibraryData_Lf& LibraryAnimData = Library.AnimationLibraryData[Animation.AnimationLibraryHash];

	if (IsValid(Animation.Animation))
	{
		if (!LibraryAnimData.KeyframesFilled.Num())
		{
			LibraryAnimData.MaxFrames = Animation.Animation->GetPlayLength() / Runtime.DataAsset->
				TimeBetweenAnimationLibraryFrames - 1;


			LibraryAnimData.KeyframesFilled.Init(INDEX_NONE, LibraryAnimData.MaxFrames);
			if (!LibraryAnimData.KeyframesFilled.Num())
			{
				return false;
			}
		}

		FrameAlpha = AnimationCodecTimeToIndex(Animation.AnimationNormalizedTime, LibraryAnimData.MaxFrames,
			Animation.Animation->Interpolation, CPUIndex0, CPUIndex1);

		if (!(LibraryAnimData.KeyframesFilled.IsValidIndex(CPUIndex0) && LibraryAnimData.KeyframesFilled.IsValidIndex(CPUIndex1)))
		{
			return false;
		}

		const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(Runtime.DataAsset);

		const FReferenceSkeleton& AnimationSkeleton = Animation.Animation->GetSkeleton()->GetReferenceSkeleton();

		if (!LibraryAnimData.bHasPoseData)
		{
			LibraryAnimData.bHasPoseData = true;

			LibraryAnimData.PoseOptions = FAnimPoseEvaluationOptions_Lf();
			LibraryAnimData.PoseOptions.bEvaluateCurves = true;
			LibraryAnimData.PoseOptions.bShouldRetarget = true;
			LibraryAnimData.PoseOptions.bExtractRootMotion = false;
			LibraryAnimData.PoseOptions.bRetrieveAdditiveAsFullPose = true;
			bool bNeedOptionalSkeletonMesh = true;
			const uint16 NumAnimationBones = AnimationSkeleton.GetNum();
			for (uint16 b = 0; b < NumAnimationBones; ++b)
			{
				const FName& BoneName = GetSkeletonBoneName(AnimationSkeleton, b);
				if (GetSkeletonBoneIndex(ReferenceSkeleton, BoneName) == INDEX_NONE)
				{
					bNeedOptionalSkeletonMesh = false;
					break;
				}
			}
			if (bNeedOptionalSkeletonMesh)
			{
				LibraryAnimData.PoseOptions.OptionalSkeletalMesh = Runtime.DataAsset->ReferenceMeshNative;
			}
			if (Animation.Animation->IsCompressedDataValid())
			{
				LibraryAnimData.PoseOptions.EvaluationType = EAnimDataEvalType_Lf::Compressed;
			}

			FAnimPose_Lf AlphaPose;
			FTurboSequence_Helper_Lf::GetPoseInfo(0, Animation.Animation, LibraryAnimData.PoseOptions,
			                                      AlphaPose);

			for (uint16 B = 0; B < LibraryAnimData.NumBones; ++B)
			{
				const FName& BoneName = GetSkeletonBoneName(ReferenceSkeleton, B);
				if (int32 PoseBoneIndex = FTurboSequence_Helper_Lf::GetAnimationBonePoseIndex(AlphaPose, BoneName);
					PoseBoneIndex > INDEX_NONE)
				{
					LibraryAnimData.BoneNameToAnimationBoneIndex.FindOrAdd(BoneName, PoseBoneIndex);
				}
			}
		}
		
		GPUIndex0 = AddAnimationPoseToLibraryChunked(CPUIndex0, Library, Animation, LibraryAnimData, ReferenceSkeleton, AnimationSkeleton);
		GPUIndex1 = AddAnimationPoseToLibraryChunked(CPUIndex1, Library, Animation, LibraryAnimData, ReferenceSkeleton, AnimationSkeleton);
	}
	else // Is Rest Pose
	{
		CPUIndex0 = CPUIndex1 = 0;

		if (LibraryAnimData.KeyframesFilled.Num() && LibraryAnimData.KeyframesFilled[0] > INDEX_NONE)
		{
			CPUIndex0 = CPUIndex1 = LibraryAnimData.KeyframesFilled[0];
			return true;
		}

		if (!LibraryAnimData.KeyframesFilled.Num())
		{
			LibraryAnimData.KeyframesFilled.Init(Library.AnimationLibraryMaxNum, 1);
			LibraryAnimData.MaxFrames = 1;

			//const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(Runtime.DataAsset);
			//LibraryAnimData.NumBones = GetSkeletonNumBones(ReferenceSkeleton);
		}
		else
		{
			LibraryAnimData.KeyframesFilled[0] = Library.AnimationLibraryMaxNum;
		}
		GPUIndex0 = GPUIndex1 = LibraryAnimData.KeyframesFilled[0];

		if (!LibraryAnimData.KeyframeIndexToPose.Contains(0))
		{
			FCPUAnimationPose_Lf CPUPose_0;
			LibraryAnimData.KeyframeIndexToPose.Add(0, CPUPose_0);
		}

		int32 NumAllocations = LibraryAnimData.NumBones * 3;
		Library.AnimationLibraryMaxNum += NumAllocations;

		uint32 AnimationDataIndex = Library.AnimationLibraryDataAllocatedThisFrame.Num();

		const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(Runtime.DataAsset);
		Library.AnimationLibraryDataAllocatedThisFrame.AddUninitialized(NumAllocations);
		for (uint16 b = 0; b < LibraryAnimData.NumBones; ++b)
		{
			const FMatrix& BoneMatrix = GetSkeletonRefPose(ReferenceSkeleton)[b].ToMatrixWithScale();
			// NOTE: Keep in mind this matrix is not in correct order after uploading it
			//		 for performance reason we are using matrix calculations which match the order
			//		 in the Vertex Shader
			for (uint8 M = 0; M < 3; ++M)
			{
				FVector4f BoneData;
				BoneData.X = BoneMatrix.M[0][M];
				BoneData.Y = BoneMatrix.M[1][M];
				BoneData.Z = BoneMatrix.M[2][M];
				BoneData.W = BoneMatrix.M[3][M];

				Library.AnimationLibraryDataAllocatedThisFrame[AnimationDataIndex] = BoneData;
				//LibraryAnimData.AnimPoses[0].RawData[b * 3 + M] = BoneData;

				AnimationDataIndex++;
			}
		}
	}

	return true;
}


UTurboSequenceRenderAttachmentData* UTurboSequenceRenderAttachmentData::CreateObject(UObject* Outer,
	UTurboSequence_MeshAsset_Lf* InMeshAsset, const FVector& MeshMinBounds, const FVector& MaxMaxBounds)
{
	UTurboSequenceRenderAttachmentData* TurboSequenceRenderAttachmentData = NewObject<UTurboSequenceRenderAttachmentData>(Outer);
	
	Init(InMeshAsset, TurboSequenceRenderAttachmentData, MeshMinBounds,MaxMaxBounds);
	
	return TurboSequenceRenderAttachmentData;

}

void UTurboSequenceRenderAttachmentData::AddAttachmentRenderInstance(const FAttachmentMeshHandle MeshHandle,
                                                                     const FTransform& WorldSpaceTransform, const FTransform3f& AttachmentLocalTransform, uint8 BoneIndex, int32
                                                                     SkeletonIndex)
{
	bool bReuse;
	const int32 InstanceIndex = AllocateRenderInstanceInternal(MeshHandle, bReuse);
	
	if (!bReuse)
	{
		ParticleAttachmentPositions.AddUninitialized(1);
		ParticleAttachmentScales.AddUninitialized(1);
		ParticleAttachmentRotations.AddUninitialized(1);
	}
	
	ParticleAttachmentPositions[InstanceIndex] = AttachmentLocalTransform.GetLocation();
	ParticleAttachmentScales[InstanceIndex] = AttachmentLocalTransform.GetScale3D();
	ParticleAttachmentRotations[InstanceIndex] = FTurboSequence_Helper_Lf::ConvertQuaternion4FToVector4F(AttachmentLocalTransform.GetRotation());
	
	bChangedParticleAttachmentPosition = true;
	bChangedParticleAttachmentScale = true;
	bChangedParticleAttachmentRotation = true;

	UpdateInstanceTransformInternal(InstanceIndex, WorldSpaceTransform, bReuse);

	SetSkeletonIndexInternal(InstanceIndex, SkeletonIndex);

	SetGPUBoneIndexForInstanceInternal(BoneIndex, InstanceIndex);
}

void UTurboSequenceRenderAttachmentData::UpdateNiagaraEmitter()
{
	if (bChangedParticleAttachmentPosition || bChangedCollectionSizeThisFrame)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
			NiagaraComponent, TEXT("User.ParticleAttachment_Position"), ParticleAttachmentPositions);
	}

	if (bChangedParticleAttachmentRotation || bChangedCollectionSizeThisFrame)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
			NiagaraComponent, TEXT("User.ParticleAttachment_Rotation"), ParticleAttachmentRotations);
	}
		
	if (bChangedParticleAttachmentScale || bChangedCollectionSizeThisFrame)
	{
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
			NiagaraComponent, TEXT("User.ParticleAttachment_Scale"), ParticleAttachmentScales);
	}

	bChangedParticleAttachmentPosition = false;
	bChangedParticleAttachmentRotation = false;
	bChangedParticleAttachmentScale = false;

	Super::UpdateNiagaraEmitter();
}


bool FTurboSequence_Utility_Lf::AnyMatchingSourceBoneNames(const TArray<FTurboSequence_BoneLayer_Lf>& A,
                                                           const TArray<FTurboSequence_BoneLayer_Lf>& B)
{
	if (!A.Num() && !B.Num())
	{
		return true;
	}
	for (const FTurboSequence_BoneLayer_Lf& Layer : A)
	{
		for (const FTurboSequence_BoneLayer_Lf& Other : B)
		{
			if (Layer.BoneLayerName == Other.BoneLayerName)
			{
				return true;
			}
		}
	}
	return false;
}

bool FTurboSequence_Utility_Lf::ContainsRootBoneName(const TArray<FTurboSequence_BoneLayer_Lf>& Collection,
                                                     const FSkinnedMeshRuntime_Lf& Runtime)
{
	const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(Runtime.DataAsset);

	const FName RootBoneName = GetSkeletonBoneName(ReferenceSkeleton, 0);
	
	for (const FTurboSequence_BoneLayer_Lf& Layer : Collection)
	{
		if (Layer.BoneLayerName == RootBoneName)
		{
			return true;
		}
	}
	return false;
}

void FTurboSequence_Utility_Lf::GenerateAnimationLayerMask(
	const FTurboSequence_MaskDefinition& MaskDefinition,
	TArray<uint16>& OutLayers,
	const TObjectPtr<UTurboSequence_MeshAsset_Lf>& MeshAsset)
{
	const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(MeshAsset);
	const int32 NumCPUBones = GetSkeletonNumBones(ReferenceSkeleton);

	OutLayers.Empty(NumCPUBones);

	if(MaskDefinition.BoneLayerMasks.Num())
	{
		TArray<FPerBoneBlendWeight> BoneBlendWeights;
		TArray<FInputBlendPose> BlendFilters;

		for (const FTurboSequence_BoneLayer_Lf& Bone : MaskDefinition.BoneLayerMasks)
		{
			FInputBlendPose BlendFilter;

			FBranchFilter BranchFilter;
			BranchFilter.BoneName = Bone.BoneLayerName;
			BranchFilter.BlendDepth = Bone.BoneDepth;
		
			BlendFilter.BranchFilters.Add(BranchFilter);
		
			BlendFilters.Add(BlendFilter);
		}
	
		FAnimationRuntime::CreateMaskWeights(BoneBlendWeights, BlendFilters,  MeshAsset->ReferenceMeshNative->GetSkeleton());
		
		const FReferenceSkeleton& MaskReferenceSkeleton = MeshAsset->ReferenceMeshNative->GetSkeleton()->GetReferenceSkeleton();

		//Have to remap right now
		for(int32 BoneIndex = 0; BoneIndex < NumCPUBones; ++BoneIndex)
		{
			const FName& BoneName = ReferenceSkeleton.GetBoneName(BoneIndex);
			const int32 RawBoneIndex = MaskReferenceSkeleton.FindBoneIndex(BoneName);

			int32 LayerValue = FMath::RoundToInt32(BoneBlendWeights[RawBoneIndex].BlendWeight * 0x7FFF);
			OutLayers.Add(LayerValue);
		}
	}
	else
	{
		for(int32 BoneIndex = 0; BoneIndex < NumCPUBones; ++BoneIndex)
		{
			int32 LayerValue = 0x7FFF;
		
			OutLayers.Add(LayerValue);
		}
	}
}

void FTurboSequence_Utility_Lf::AddAnimation(FSkinnedMeshRuntime_Lf& Runtime,
                                             FAnimationMetaData_Lf& Animation,
                                             FSkinnedMeshGlobalLibrary_Lf& Library, const bool bForceFront)
{
	

	if(bForceFront)
	{
		Runtime.AnimationMetaData.Insert(Animation,1); //0 is bind pose
	}
	else
	{
		Runtime.AnimationMetaData.Add(Animation);
	}
	
	FBoneMaskBuiltProxyHandle BoneMaskBuiltProxyHandle = Animation.Settings.MaskDefinition.GetBuiltProxyHandle(Runtime.DataAsset);

	if(int* MaskRefCount = Library.MaskRefCount.Find(BoneMaskBuiltProxyHandle))
	{
		(*MaskRefCount)++;
	}
	else
	{
		Library.MaskRefCount.Add(BoneMaskBuiltProxyHandle, 1);
		//Make the mask
		FAnimationBlendLayerMask_Lf AnimationBlendLayerMask;
		
		GenerateAnimationLayerMask(Animation.Settings.MaskDefinition, AnimationBlendLayerMask.RawAnimationLayers, Runtime.DataAsset);
		
		Library.AnimationBlendLayerMasks.Add(BoneMaskBuiltProxyHandle,AnimationBlendLayerMask );
		
		Library.bMasksNeedRebuilding = true;
	}
	
	if(!Library.AnimationLibraryData.Contains(Animation.AnimationLibraryHash))
	{
		FAnimationLibraryData_Lf Data = FAnimationLibraryData_Lf();
		Data.NumBones = Runtime.DataAsset->GetNumCPUBones();
		Library.AnimationLibraryData.Add(Animation.AnimationLibraryHash, Data);
	}
}

void FTurboSequence_Utility_Lf::RemoveAnimation(FSkinnedMeshRuntime_Lf& Runtime,
                                                FSkinnedMeshGlobalLibrary_Lf& Library,
                                                const int32 Index)
{
	const FAnimationMetaData_Lf& Animation = Runtime.AnimationMetaData[Index];

	const FBoneMaskBuiltProxyHandle BoneMaskBuiltProxyHandle = Animation.Settings.MaskDefinition.GetBuiltProxyHandle(Runtime.DataAsset);

	if(int* MaskRefCount = Library.MaskRefCount.Find(BoneMaskBuiltProxyHandle))
	{
		(*MaskRefCount)--;

		if(*MaskRefCount == 0)
		{
			Library.MaskRefCount.Remove(BoneMaskBuiltProxyHandle);
			
			Library.AnimationBlendLayerMasks.Remove(BoneMaskBuiltProxyHandle);
		
			Library.bMasksNeedRebuilding = true;
		}
	}
	

	Runtime.AnimationMetaData.RemoveAt(Index);
}

bool FTurboSequence_Utility_Lf::RefreshBlendSpaceState(const TObjectPtr<UBlendSpace> BlendSpace,
                                                       FAnimationBlendSpaceData_Lf& Data, float DeltaTime)
{
	Data.Tick = FAnimTickRecord(BlendSpace, FVector(Data.CurrentPosition), Data.CachedBlendSampleData,
	                            Data.BlendFilter, true, 1.0f, true, true, 1.0f, Data.CurrentTime, Data.Record);
	const TArray<FName> MarkerNames;
	FAnimAssetTickContext Context(DeltaTime, ERootMotionMode::IgnoreRootMotion, false, MarkerNames);
	if (!Data.Tick.DeltaTimeRecord)
	{
		Data.Tick.DeltaTimeRecord = &Data.DeltaTimeRecord;
	}
	Context.MarkerTickContext.SetMarkerSyncStartPosition(
		FMarkerSyncAnimPosition(FName("-1"), FName("-1"), 0));

	BlendSpace->TickAssetPlayer(Data.Tick, Data.NotifyQueue, Context);
	

	if (!Data.CachedBlendSampleData.Num())
	{
		return false;
	}

	return true;
}

FTurboSequence_AnimMinimalBlendSpace_Lf FTurboSequence_Utility_Lf::PlayBlendSpace(
	FSkinnedMeshGlobalLibrary_Lf& Library,
	FSkinnedMeshRuntime_Lf& Runtime,
	const TObjectPtr<UBlendSpace> BlendSpace, const TObjectPtr<UTurboSequence_ThreadContext_Lf>& ThreadContext,
	const FTurboSequence_AnimPlaySettings_Lf& AnimSettings, float OverrideWeight, float OverrideStartTime,
	float OverrideEndTime)
{
	FAnimationBlendSpaceData_Lf Data = FAnimationBlendSpaceData_Lf();
	FTurboSequence_AnimMinimalBlendSpace_Lf BlendSpaceData = FTurboSequence_AnimMinimalBlendSpace_Lf(true);
	BlendSpaceData.BlendSpace = BlendSpace;
	BlendSpaceData.BelongsToMeshID = Runtime.MeshID;
	BlendSpace->InitializeFilter(&Data.BlendFilter);

	int16 NumSamples = BlendSpace->GetBlendSamples().Num();

	RefreshBlendSpaceState(BlendSpace, Data, 0);

	FTurboSequence_AnimPlaySettings_Lf Settings = AnimSettings;
	Settings.AnimationManagementMode = ETurboSequence_ManagementMode_Lf::SelfManaged;
	Settings.bAnimationTimeSelfManaged = true;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		float PlayLength = BlendSpace->GetBlendSample(i).Animation->GetPlayLength();

		Data.LongestPlayLength = FMath::Max(Data.LongestPlayLength, PlayLength);

		FAnimationMetaDataHandle AnimID = PlayAnimation(Library, Runtime, BlendSpace->GetBlendSample(i).Animation, Settings,
		                                                BlendSpace->bLoop,
		                                                OverrideWeight, OverrideStartTime, OverrideEndTime);

		if (const FBlendSampleData* Sample = Data.CachedBlendSampleData.FindByPredicate(
			[&i](const FBlendSampleData& Other) { return Other.SampleDataIndex == i; }))
		{
			if(FAnimationMetaData_Lf* AnimationFrame = Runtime.GetAnimMetaData(AnimID))
			{
				AnimationFrame->Settings.AnimationWeight = Sample->GetClampedWeight();

				AnimationFrame->AnimationTime = FMath::Clamp(Sample->Time, 0.0f, Sample->Animation->GetPlayLength());

				TweakAnimation(Runtime, Library, AnimationFrame->Settings, AnimID);
			}
		}

		ThreadContext->CriticalSection.Lock();
		Data.Points.Add(AnimID, i);
		// Might add more info
		BlendSpaceData.Samples.Add(AnimID.AnimationID);
		ThreadContext->CriticalSection.Unlock();
	}

	int32 NumBlendSpaces = Runtime.AnimationBlendSpaceMetaData.Num();
	TArray<TObjectPtr<UBlendSpace>> BlendSpaceKeys;
	ThreadContext->CriticalSection.Lock();
	Runtime.AnimationBlendSpaceMetaData.GetKeys(BlendSpaceKeys);
	ThreadContext->CriticalSection.Unlock();
	for (int32 i = NumBlendSpaces - 1; i >= 0; --i)
	{
		const FAnimationBlendSpaceData_Lf& MetaData = Runtime.AnimationBlendSpaceMetaData[BlendSpaceKeys[i]];

		bool bIsValidBlendSpace = true;
		for (const auto& Point : MetaData.Points)
		{
			if (Runtime.GetAnimIndex(Point.Key) == INDEX_NONE)
			{
				bIsValidBlendSpace = false;
				break;
			}
		}
		if (!bIsValidBlendSpace)
		{
			ThreadContext->CriticalSection.Lock();
			Runtime.AnimationBlendSpaceMetaData.Remove(BlendSpaceKeys[i]);
			ThreadContext->CriticalSection.Unlock();
			continue;
		}

		for (const auto& Point : MetaData.Points)
		{
			if(FAnimationMetaData_Lf* AnimationFrame = Runtime.GetAnimMetaData(Point.Key))
			{
				if ( AnyMatchingSourceBoneNames(AnimSettings.MaskDefinition.BoneLayerMasks,
													 AnimationFrame->Settings.MaskDefinition.BoneLayerMasks))
				{
					AnimationFrame->bSetForRemoval = true;
				}
			}
		}
	}

	if (!BlendSpaceData.Samples.Num())
	{
		return FTurboSequence_AnimMinimalBlendSpace_Lf(false);
	}

	ThreadContext->CriticalSection.Lock();
	Runtime.AnimationBlendSpaceMetaData.Add(BlendSpace, Data);
	ThreadContext->CriticalSection.Unlock();

	return BlendSpaceData;
}

FAnimationMetaDataHandle FTurboSequence_Utility_Lf::PlayAnimation(
	FSkinnedMeshGlobalLibrary_Lf& Library,
	FSkinnedMeshRuntime_Lf& Runtime,
	UAnimSequence* Animation,
	const FTurboSequence_AnimPlaySettings_Lf& AnimSettings,
	const bool bIsLoop, float OverrideWeight,
	float OverrideStartTime, float OverrideEndTime, const bool bForceFront)
{
	const FUintVector& AnimationHash = GetAnimationLibraryKey(Runtime.DataAsset->GetSkeleton(),
	                                                          Runtime.DataAsset, Animation);

	const bool bIsRestPose = !IsValid(Animation);

	if (AnimSettings.ForceMode != ETurboSequence_AnimationForceMode_Lf::None)
	{
		ClearAnimations(Runtime, Library, AnimSettings.ForceMode, AnimSettings.MaskDefinition.BoneLayerMasks, [](const FAnimationMetaData_Lf& AnimationMeta)
		{
			return true;
		});
	}
	FAnimationMetaData_Lf Frame = FAnimationMetaData_Lf();
	Frame.Animation = Animation;
	Frame.AnimationTime = AnimSettings.AnimationPlayTimeInSeconds;
	Frame.bIsLoop = bIsLoop;

	Frame.AnimationWeightStartTime = AnimSettings.StartTransitionTimeInSeconds;
	Frame.AnimationWeightTime = 0;
	if (AnimSettings.ForceMode != ETurboSequence_AnimationForceMode_Lf::None)
	{
		if (OverrideWeight >= 0)
		{
			Frame.FinalAnimationWeight = OverrideWeight * AnimSettings.AnimationWeight;
		}
		else
		{
			Frame.FinalAnimationWeight = 1 * AnimSettings.AnimationWeight;
		}
		Frame.AnimationWeightTime = AnimSettings.StartTransitionTimeInSeconds;
	}
	else
	{
		if (OverrideWeight >= 0)
		{
			Frame.FinalAnimationWeight = OverrideWeight * AnimSettings.AnimationWeight;
		}
		else
		{
			Frame.FinalAnimationWeight = 0;
		}
	}
	if (OverrideStartTime >= 0)
	{
		Frame.AnimationWeightTime = OverrideStartTime;
	}
	Frame.AnimationRemoveTime = 0;
	if (OverrideEndTime >= 0)
	{
		Frame.AnimationRemoveTime = OverrideEndTime;
	}
	Frame.AnimationRemoveStartTime = AnimSettings.EndTransitionTimeInSeconds;
	Frame.bSetForRemoval = false;
	Frame.AnimationLibraryHash = AnimationHash;
	bIsRestPose ? Frame.AnimationMaxPlayLength = 1 : Frame.AnimationMaxPlayLength = Animation->GetPlayLength();
	
	Frame.AnimationNormalizedTime = FTurboSequence_Helper_Lf::GetPercentageBetweenMinMax(
		Frame.AnimationTime, 0, Frame.AnimationMaxPlayLength);

	Frame.bIsRootBoneAnimation = AnimSettings.MaskDefinition.BoneLayerMasks.Num() ? ContainsRootBoneName(AnimSettings.MaskDefinition.BoneLayerMasks, Runtime) : true;
	
	bool bIsSelfManagedAnimation = AnimSettings.AnimationManagementMode == ETurboSequence_ManagementMode_Lf::SelfManaged;
	Frame.AnimationGroupLayerHash = AnimSettings.MaskDefinition.GetHandle();

	Frame.AnimationID = FAnimationMetaDataHandle(Runtime.LastAnimationID++);
	Frame.Settings = AnimSettings;

	//Set any animations for removal that are not self-managed and
	//have a shared name in their source bone mask as me
	//Weird
	uint16 NumAnimationsPreAdd = Runtime.AnimationMetaData.Num();
	for (int32 AnimIdx = NumAnimationsPreAdd - 1; AnimIdx >= 1; --AnimIdx)
	{
		FAnimationMetaData_Lf& AnimationFrame = Runtime.AnimationMetaData[AnimIdx];
		if (!bIsSelfManagedAnimation && AnyMatchingSourceBoneNames(
			AnimSettings.MaskDefinition.BoneLayerMasks, AnimationFrame.Settings.MaskDefinition.BoneLayerMasks))
		{
			AnimationFrame.bSetForRemoval = true;
		}
	}
	
	AddAnimation(Runtime, Frame, Library, bForceFront);

	return Frame.AnimationID;
}

TObjectPtr<UAnimSequence> FTurboSequence_Utility_Lf::GetHighestPriorityAnimation(const FSkinnedMeshRuntime_Lf& Runtime)
{
	if (!Runtime.AnimationMetaData.Num())
	{
		return nullptr;
	}
	TObjectPtr<UAnimSequence> BestMatchAnimation = nullptr;
	float BestMatchWeight = -1;
	for (const FAnimationMetaData_Lf& Animation : Runtime.AnimationMetaData)
	{
		if (Animation.FinalAnimationWeight > BestMatchWeight)
		{
			BestMatchWeight = Animation.FinalAnimationWeight;
			BestMatchAnimation = Animation.Animation;
		}
	}
	return BestMatchAnimation;
}

void FTurboSequence_Utility_Lf::UpdateBlendSpaces(FSkinnedMeshRuntime_Lf& Runtime,
                                                  float DeltaTime, FSkinnedMeshGlobalLibrary_Lf& Library)
{
	for (TTuple<TObjectPtr<UBlendSpace>, FAnimationBlendSpaceData_Lf>& BlendSpace : Runtime.
	     AnimationBlendSpaceMetaData)
	{
		FAnimationBlendSpaceData_Lf& Data = BlendSpace.Value;

		if (!RefreshBlendSpaceState(BlendSpace.Key, Data, DeltaTime))
		{
			continue;
		}

		for (const auto& Point : Data.Points)
		{
			if (FAnimationMetaData_Lf* AnimationFrame = Runtime.GetAnimMetaData(Point.Key))
			{
				FTurboSequence_AnimPlaySettings_Lf Settings = AnimationFrame->Settings;

				float Weight = 0;
				float Time = AnimationFrame->AnimationTime;
				if (const FBlendSampleData* Sample = Data.CachedBlendSampleData.FindByPredicate(
					[&Point](const FBlendSampleData& Other) { return Other.SampleDataIndex == Point.Value; }))
				{
					Weight = Sample->GetClampedWeight();
					Time = Sample->Time;
				}

				float AnimLength = AnimationFrame->Animation->GetPlayLength();
				Settings.AnimationWeight = Weight;
				//Settings.Settings.AnimationSpeed = AnimLength / Data.LongestPlayLength;
				AnimationFrame->AnimationTime = FMath::Clamp(Time, 0.0f, AnimLength);

				TweakAnimation(Runtime, Library, Settings, AnimationFrame->AnimationID);
			}
		}
	}
}

bool FTurboSequence_Utility_Lf::TweakBlendSpace(FSkinnedMeshRuntime_Lf& Runtime,
                                                const FTurboSequence_AnimMinimalBlendSpace_Lf& BlendSpace,
                                                const FVector3f& WantedPosition)
{
	if (!Runtime.AnimationBlendSpaceMetaData.Contains(BlendSpace.BlendSpace))
	{
		return false;
	}
	FAnimationBlendSpaceData_Lf& Data = Runtime.AnimationBlendSpaceMetaData[BlendSpace.BlendSpace];
	Data.CurrentPosition = WantedPosition;
	return true;
}

bool FTurboSequence_Utility_Lf::TweakAnimation(FSkinnedMeshRuntime_Lf& Runtime,
                                               FSkinnedMeshGlobalLibrary_Lf& Library,
                                               const FTurboSequence_AnimPlaySettings_Lf& Settings, const FAnimationMetaDataHandle& AnimationID)
{
	if(FAnimationMetaData_Lf* AnimationFrame = Runtime.GetAnimMetaData(AnimationID))
	{
		if (Settings.ForceMode != ETurboSequence_AnimationForceMode_Lf::None)
		{
			ClearAnimations(Runtime, Library, Settings.ForceMode, Settings.MaskDefinition.BoneLayerMasks, [&AnimationFrame](const FAnimationMetaData_Lf& AnimationMetaData)
			{
				return AnimationFrame->AnimationID != AnimationMetaData.AnimationID;
			});

			AnimationFrame->AnimationWeightStartTime = Settings.StartTransitionTimeInSeconds;
			AnimationFrame->AnimationWeightTime = Settings.StartTransitionTimeInSeconds;
		}
		else
		{
			AnimationFrame->AnimationWeightStartTime = Settings.StartTransitionTimeInSeconds;
		}

		AnimationFrame->Settings = Settings;
		
		return true;
	}

	
	return false;
}

void FTurboSequence_Utility_Lf::SolveAnimations(FSkinnedMeshRuntime_Lf& Runtime, FSkinnedMeshGlobalLibrary_Lf& Library,
                                                const float DeltaTime, const int64 CurrentFrameCount)
{
	if (CurrentFrameCount == Runtime.LastFrameAnimationSolved)
	{
		return;
	}

	if(!Runtime.bAnimTickEnabled)
	{
		return;
	}
	
	Runtime.LastFrameAnimationSolved = CurrentFrameCount;
	
	UpdateBlendSpaces(Runtime, DeltaTime, Library);
	
	//We use bone masks to define the groups, then we scale the blend weight by the groups?
	// < Animation Group Layer Hash | Animation Group >
	TMap<FBoneMaskSourceHandle, float> AnimationGroupWeight;
	
	for (int32 AnimIdx = Runtime.AnimationMetaData.Num() - 1; AnimIdx >= 0; --AnimIdx)
	{
		FAnimationMetaData_Lf& Animation = Runtime.AnimationMetaData[AnimIdx];
		Animation.LastAnimationTime = Animation.AnimationTime;

		if (AnimIdx) //Skips the bind pose 
		{
			if (!Animation.Settings.bAnimationTimeSelfManaged)
			{
				float ScaledDeltaTime = DeltaTime * Animation.Settings.AnimationSpeed;

				float AnimationTimeNext = Animation.AnimationTime + ScaledDeltaTime;
				// Animation Time
				if (Animation.bIsLoop)
				{
					Animation.AnimationTime = FMath::Fmod(AnimationTimeNext,
					                                      Animation.AnimationMaxPlayLength);
				}
				else
				{
					Animation.AnimationTime = AnimationTimeNext;
				}
			}

			// Frame
			Animation.AnimationNormalizedTime = FTurboSequence_Helper_Lf::GetPercentageBetweenMinMax(
				Animation.AnimationTime, 0, Animation.AnimationMaxPlayLength);


			// Tick the blend in time (realtime)
			Animation.AnimationWeightTime = FMath::Clamp(Animation.AnimationWeightTime + DeltaTime, 0.0f, Animation.AnimationWeightStartTime);

			//Blend in weight
			const float AdditionWeight = FTurboSequence_Helper_Lf::GetPercentageBetweenMinMax(Animation.AnimationWeightTime, 0,
			                                                                                  Animation.AnimationWeightStartTime);
			//Remove one shot
			const float AnimTimeRemaining = Animation.AnimationMaxPlayLength - Animation.AnimationTime;
			const float RealTimeRemaining = AnimTimeRemaining / Animation.Settings.AnimationSpeed;
			const bool bRemoveOneShot = !Animation.bSetForRemoval && !Animation.bIsLoop && (RealTimeRemaining < Animation.AnimationRemoveStartTime);

			if(bRemoveOneShot)
			{
				Animation.bSetForRemoval = true;
				Animation.AnimationRemoveTime = 0.f;
			}

			float AnimationWeight = AdditionWeight;

			// Removal
			if (Animation.bSetForRemoval)
			{
				//Count up the removal time
				Animation.AnimationRemoveTime = FMath::Clamp(Animation.AnimationRemoveTime + DeltaTime, 0.0f, Animation.AnimationRemoveStartTime);

				//Blend out weight
				const float RemovalWeight = 1.f - FTurboSequence_Helper_Lf::Clamp01(
					FTurboSequence_Helper_Lf::GetPercentageBetweenMinMax(
						Animation.AnimationRemoveTime, 0, Animation.AnimationRemoveStartTime));
				
				AnimationWeight = FMath::Min(AdditionWeight,RemovalWeight);

				if (FMath::IsNearlyZero(AnimationWeight))
				{
					RemoveAnimation(Runtime, Library, AnimIdx); //Reverse loop so ok to remove
					continue; 
				}
			}

			// Weight
			Animation.FinalAnimationWeight = FMath::Min(AnimationWeight, Animation.Settings.AnimationWeight);

			if (Animation.Settings.bNormalizeWeightInGroup)
			{
				float& GroupWeight = AnimationGroupWeight.FindOrAdd(Animation.AnimationGroupLayerHash, 0.0f);
				GroupWeight += Animation.FinalAnimationWeight;
			}
		}
		else
        {
        	Animation.AnimationTime = 0;
        	Animation.AnimationNormalizedTime = 0;
        }
		
		AddAnimationToLibraryChunked(Library, Animation.CPUAnimationIndex_0, Animation.GPUAnimationIndex_0,
		                             Animation.CPUAnimationIndex_1, Animation.GPUAnimationIndex_1, Animation.FrameAlpha, Runtime, Animation);
	}
	
	float BaseLayerWeight = 1;
	for (int32 AnimIdx = Runtime.AnimationMetaData.Num() - 1; AnimIdx >= 0; --AnimIdx)
	{
		FAnimationMetaData_Lf& Animation = Runtime.AnimationMetaData[AnimIdx];
		if (Animation.Settings.bNormalizeWeightInGroup)
		{
			if (AnimIdx)
			{
				const float TotalAnimWeightRuntime = AnimationGroupWeight[Animation.AnimationGroupLayerHash];
				
				Animation.FinalAnimationWeight /= TotalAnimWeightRuntime;
				Animation.FinalAnimationWeight = FMath::Min(Animation.FinalAnimationWeight,
				                                            Animation.Settings.AnimationWeight);
				BaseLayerWeight -= Animation.FinalAnimationWeight;
			}
			else
			{
				Animation.FinalAnimationWeight = FTurboSequence_Helper_Lf::Clamp01(BaseLayerWeight);
			}
		}
	}
}


void FTurboSequence_Utility_Lf::GetAnimNotifies(const FSkinnedMeshRuntime_Lf& Runtime, FTurboSequence_AnimNotifyQueue_Lf& NotifyQueue)
{
	static FAnimNotifyContext NotifyContext;
	
	for (int32 AnimIdx = Runtime.AnimationMetaData.Num() - 1; AnimIdx >= 0; --AnimIdx)
	{
		const FAnimationMetaData_Lf& Animation = Runtime.AnimationMetaData[AnimIdx];
		if (Animation.Animation)
		{
			Animation.Animation->GetAnimNotifiesFromDeltaPositions(Animation.LastAnimationTime, Animation.AnimationTime, NotifyContext);
			NotifyQueue.AddAnimNotifies(NotifyContext.ActiveNotifies, Animation.FinalAnimationWeight);
			NotifyContext.ActiveNotifies.Reset();
		}
	}
}


FTurboSequence_TransposeMatrix_Lf FTurboSequence_Utility_Lf::GetBoneTransformFromLocalPoses(int16 BoneIndex,
	const FAnimationLibraryData_Lf& LibraryData, const FReferenceSkeleton& ReferenceSkeleton,
	const FReferenceSkeleton& AnimationSkeleton, const FAnimPose_Lf& Pose, const TObjectPtr<UAnimSequence> Animation)
{
	FTurboSequence_TransposeMatrix_Lf BoneSpaceTransform = FTurboSequence_TransposeMatrix_Lf();

	if (!BoneIndex && Animation->bEnableRootMotion)
	{
		switch (Animation->RootMotionRootLock)
		{
		case ERootMotionRootLock::RefPose:
			{
				const FMatrix RefPoseMatrix = GetSkeletonRefPose(ReferenceSkeleton)[0].
					ToMatrixWithScale();

				for (uint8 M = 0; M < 3; ++M)
				{
					BoneSpaceTransform.Colum[M].X = RefPoseMatrix.M[0][M];
					BoneSpaceTransform.Colum[M].Y = RefPoseMatrix.M[1][M];
					BoneSpaceTransform.Colum[M].Z = RefPoseMatrix.M[2][M];
					BoneSpaceTransform.Colum[M].W = RefPoseMatrix.M[3][M];
				}
			}
			break;

		case ERootMotionRootLock::AnimFirstFrame:
			BoneSpaceTransform = Pose.LocalSpacePoses[0];
			break;

		case ERootMotionRootLock::Zero:

			{
				const FMatrix ZeroMatrix = FTransform::Identity.ToMatrixWithScale();

				for (uint8 M = 0; M < 3; ++M)
				{
					BoneSpaceTransform.Colum[M].X = ZeroMatrix.M[0][M];
					BoneSpaceTransform.Colum[M].Y = ZeroMatrix.M[1][M];
					BoneSpaceTransform.Colum[M].Z = ZeroMatrix.M[2][M];
					BoneSpaceTransform.Colum[M].W = ZeroMatrix.M[3][M];
				}
			}
			break;

		default:
			BoneSpaceTransform = FTurboSequence_TransposeMatrix_Lf();
			break;
		}
	}
	else
	{
		const FName& BoneName = GetSkeletonBoneName(ReferenceSkeleton, BoneIndex);
		if (const int32 BoneIdx = GetSkeletonBoneIndex(AnimationSkeleton, BoneName); BoneIdx > INDEX_NONE)
		{
			BoneSpaceTransform = Pose.LocalSpacePoses[LibraryData.BoneNameToAnimationBoneIndex[BoneName]];
		}
		else
		{
			const FMatrix& RefPoseMatrix = GetSkeletonRefPose(ReferenceSkeleton)[0].ToMatrixWithScale();

			for (uint8 M = 0; M < 3; ++M)
			{
				BoneSpaceTransform.Colum[M].X = RefPoseMatrix.M[0][M];
				BoneSpaceTransform.Colum[M].Y = RefPoseMatrix.M[1][M];
				BoneSpaceTransform.Colum[M].Z = RefPoseMatrix.M[2][M];
				BoneSpaceTransform.Colum[M].W = RefPoseMatrix.M[3][M];
			}
		}
	}

	return BoneSpaceTransform;
}

void FTurboSequence_Utility_Lf::GetBoneTransformFromAnimationSafe(FMatrix& OutAtom,
                                                                  const FAnimationMetaData_Lf& Animation,
                                                                  const int32 FrameIndex,
                                                                  const uint16 SkeletonBoneIndex,
                                                                  const FSkinnedMeshGlobalLibrary_Lf& Library,
                                                                  const FReferenceSkeleton& ReferenceSkeleton)
{
	if (IsValid(Animation.Animation))
	{
		const FAnimationLibraryData_Lf& LibraryData = Library.AnimationLibraryData[Animation.AnimationLibraryHash];
		
		if (const FCPUAnimationPose_Lf* Pose = LibraryData.KeyframeIndexToPose.Find(FrameIndex))
		{
			const FReferenceSkeleton& AnimationSkeleton = Animation.Animation->GetSkeleton()->GetReferenceSkeleton();
			
			const FTurboSequence_TransposeMatrix_Lf& Matrix = GetBoneTransformFromLocalPoses(
				SkeletonBoneIndex, LibraryData, ReferenceSkeleton,
				AnimationSkeleton,
				Pose->Pose,
				Animation.Animation);
			
			for (uint8 M = 0; M < 3; ++M)
			{
				OutAtom.M[0][M] = Matrix.Colum[M].X;
				OutAtom.M[1][M] = Matrix.Colum[M].Y;
				OutAtom.M[2][M] = Matrix.Colum[M].Z;
				OutAtom.M[3][M] = Matrix.Colum[M].W;
			}

			// OutAtom.M[0][0] = CurrentRow0.X;
			// OutAtom.M[0][1] = CurrentRow1.X;
			// OutAtom.M[0][2] = CurrentRow2.X;
			// OutAtom.M[0][3] = 0;
			//
			// OutAtom.M[1][0] = CurrentRow0.Y;
			// OutAtom.M[1][1] = CurrentRow1.Y;
			// OutAtom.M[1][2] = CurrentRow2.Y;
			// OutAtom.M[1][3] = 0;
			//
			// OutAtom.M[2][0] = CurrentRow0.Z;
			// OutAtom.M[2][1] = CurrentRow1.Z;
			// OutAtom.M[2][2] = CurrentRow2.Z;
			// OutAtom.M[2][3] = 0;
			//
			// OutAtom.M[3][0] = CurrentRow0.W;
			// OutAtom.M[3][1] = CurrentRow1.W;
			// OutAtom.M[3][2] = CurrentRow2.W;
			// OutAtom.M[3][3] = 1;
		}
		else
		{
			OutAtom = FMatrix::Identity;
		}
	}
	else
	{
		//const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(Asset);
		if (GetSkeletonIsValidIndex(ReferenceSkeleton, SkeletonBoneIndex))
		{
			OutAtom = GetSkeletonRefPose(ReferenceSkeleton)[SkeletonBoneIndex].ToMatrixWithScale();
		}
		else
		{
			OutAtom = FMatrix::Identity;
		}
	}
}

void FTurboSequence_Utility_Lf::PrintAnimsToScreen(const FSkinnedMeshRuntime_Lf& Runtime)
{
	if(!GEngine)
	{
		return;
	}
	
	TArray<FString> Lines;
	TArray<FColor> LineColours;
	
	float CumulativeAppliedWeight = 0.f;
	const int32 AnimationMetaDataNum = Runtime.AnimationMetaData.Num();
	for (int32 AnimationMetaDataIndex = 0; AnimationMetaDataIndex < AnimationMetaDataNum; ++AnimationMetaDataIndex)
	{
		const FAnimationMetaData_Lf& Animation = Runtime.AnimationMetaData[AnimationMetaDataIndex];
		FString AnimName = TEXT("Bind pose");

		if(const UAnimSequence* AnimSequence = Animation.Animation)
		{
			AnimSequence->GetName(AnimName);
		}
		
		const float Weight = FMath::Min(Animation.FinalAnimationWeight, 1.f - CumulativeAppliedWeight);

		CumulativeAppliedWeight += Weight;
		
		Lines.Add(FString::Printf(TEXT("%d %s time:%f speed:%f weight( desired:%f applied :%f) Remove:%s"),AnimationMetaDataIndex, *AnimName, Animation.AnimationTime,Animation.Settings.AnimationSpeed, Animation.FinalAnimationWeight, Weight, Animation.bSetForRemoval ? TEXT("TRUE") : TEXT("FALSE") ));
		LineColours.Add( (Weight > 0.f) ? FColor::Yellow : FColor::Red);
	}

	//Reverse the lines, reads better
	for(int32 LineIndex = Lines.Num() -1; LineIndex >= 0 ; --LineIndex)
	{
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, LineColours[LineIndex], *Lines[LineIndex] );
	}
	
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, TEXT("------------------"));

}

FTransform FTurboSequence_Utility_Lf::BendBoneFromAnimations(const int32 BoneIndex, const FSkinnedMeshRuntime_Lf& Runtime,
                                                             const FSkinnedMeshGlobalLibrary_Lf& Library)
{
	const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(Runtime.DataAsset);

	FTransform OutAtom;
	bool bFoundFirstAnimation = false;
	float CumulativeAppliedWeight = 0.f;

	for (const FAnimationMetaData_Lf& Animation : Runtime.AnimationMetaData) 
	{
		float Weight = Animation.FinalAnimationWeight;

		FBoneMaskBuiltProxyHandle BoneMaskBuiltProxyHandle = Animation.Settings.MaskDefinition.GetBuiltProxyHandle(Runtime.DataAsset);

		if (const FAnimationBlendLayerMask_Lf* AnimationBlendLayerMask_Lf = Library.AnimationBlendLayerMasks.Find(BoneMaskBuiltProxyHandle))
		{
			if(AnimationBlendLayerMask_Lf->RawAnimationLayers.IsValidIndex(BoneIndex))
			{
				const uint16 MaskWeightInt = AnimationBlendLayerMask_Lf->RawAnimationLayers[BoneIndex];

				const float MaskWeight = static_cast<float>(MaskWeightInt) / static_cast<float>(0x7FFF);
				
				Weight *= MaskWeight;
			}
		}
		
		Weight = FMath::Min(Weight, 1.f - CumulativeAppliedWeight);

		CumulativeAppliedWeight += Weight;
		
		if (FMath::IsNearlyZero(Weight))
		{
			continue;
		}
		
		FMatrix BoneSpaceAtom0 = FMatrix::Identity;
		FMatrix BoneSpaceAtom1 = FMatrix::Identity;

		// Since we get the data directly from the GPU Collection Data we can likely ignore the root state,
		// the correct root bone data already baked into the collection .
		GetBoneTransformFromAnimationSafe(BoneSpaceAtom0, Animation, Animation.CPUAnimationIndex_0, BoneIndex, Library,
		                                  ReferenceSkeleton);

		GetBoneTransformFromAnimationSafe(BoneSpaceAtom1, Animation, Animation.CPUAnimationIndex_1, BoneIndex, Library,
		                                  ReferenceSkeleton);

		const FTransform& Atom0 = FTransform(BoneSpaceAtom0);
		const FTransform& Atom1 = FTransform(BoneSpaceAtom1);
		
		FVector ScaleInterpolated = FMath::Lerp(Atom0.GetScale3D(), Atom1.GetScale3D(),Animation.FrameAlpha );
		FVector TranslationInterpolated = FMath::Lerp(Atom0.GetTranslation(), Atom1.GetTranslation(), Animation.FrameAlpha);
		FQuat RotationInterpolated = FQuat::Slerp(Atom0.GetRotation(), Atom1.GetRotation(),Animation.FrameAlpha);

		//NB AccumulateWithShortestRotation
		
		if (!bFoundFirstAnimation)
		{
			bFoundFirstAnimation = true;
			OutAtom.SetScale3D(ScaleInterpolated * Weight);
			OutAtom.SetTranslation(TranslationInterpolated * Weight);
			OutAtom.SetRotation(RotationInterpolated * Weight);
		}
		else
		{
			OutAtom.SetScale3D(OutAtom.GetScale3D() + ScaleInterpolated * Weight);
			OutAtom.SetTranslation(OutAtom.GetTranslation() + TranslationInterpolated * Weight);
			OutAtom.SetRotation(FTurboSequence_Helper_Lf::Scale_Quaternion(
				OutAtom.GetRotation(), RotationInterpolated,
				Weight).GetNormalized());
		}
	}

	if(!bFoundFirstAnimation)
	{
		return FTransform::Identity;
	}
	
	OutAtom.NormalizeRotation();
	return OutAtom;
}

void FTurboSequence_Utility_Lf::ExtractRootMotionFromAnimations(FTransform& OutAtom,
                                                                const FSkinnedMeshRuntime_Lf& Runtime,
                                                                float DeltaTime)
{
	FMatrix OutputTransform = FMatrix::Identity;
	for (const FAnimationMetaData_Lf& Animation : Runtime.AnimationMetaData)
	{
		if (ETurboSequence_RootMotionMode_Lf Mode = Animation.Settings.RootMotionMode;
			IsValid(Animation.Animation) && Animation.Animation->HasRootMotion() && (Mode ==
				ETurboSequence_RootMotionMode_Lf::Force || (Mode ==
					ETurboSequence_RootMotionMode_Lf::OnRootBoneAnimated && Animation.bIsRootBoneAnimation)))
		{
			FTransform RootMotion_Transform = Animation.Animation->ExtractRootMotion(
				Animation.AnimationTime, DeltaTime, Animation.bIsLoop);

			float Scalar = Animation.FinalAnimationWeight * Animation.Settings.AnimationSpeed;

			OutputTransform = OutputTransform * (1 - Scalar) + RootMotion_Transform.ToMatrixWithScale() *
				Scalar;
		}
	}
	OutAtom = FTransform(OutputTransform);
}

void FTurboSequence_Utility_Lf::GetTransforms(TArray<FTransform>& OutAtoms, const TArray<int32>& BoneIndices, FSkinnedMeshRuntime_Lf& Runtime,
                                              const FSkinnedMeshGlobalLibrary_Lf& Library,
                                              const EBoneSpaces::Type Space)
{
	const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(Runtime.DataAsset);

	TMap<int32, FTransform> AnimatedBoneCache;

	for (const int32 BoneIndex : BoneIndices)
	{
		FTransform BoneMatrix = FTransform::Identity;

		int32 RuntimeIndex = BoneIndex;
		while (RuntimeIndex != INDEX_NONE)
		{
			if (const FOverrideBoneTransform_Lf* IKBoneData = Runtime.OverrideBoneTransforms.Find(RuntimeIndex))
			{
				BoneMatrix = IKBoneData->OverrideTransform * BoneMatrix;
			}
			else
			{
				if(const FTransform* AnimatedBone = AnimatedBoneCache.Find(RuntimeIndex))
				{
					BoneMatrix = *AnimatedBone * BoneMatrix;
				}
				else
				{
					const FTransform BoneFromAnimations = BendBoneFromAnimations(RuntimeIndex, Runtime, Library);
					
					AnimatedBoneCache.Add(RuntimeIndex, BoneFromAnimations);
				
					BoneMatrix = BoneFromAnimations * BoneMatrix;
				}
			}
		
			RuntimeIndex = GetSkeletonParentIndex(ReferenceSkeleton, RuntimeIndex);
		}

		OutAtoms.Add(BoneMatrix);
	}
	
	
	if (Space == EBoneSpaces::Type::WorldSpace)
	{
		for(int i = 0; i < OutAtoms.Num(); ++i)
		{
			OutAtoms[i] *= Runtime.WorldSpaceTransform;
		}
	}
}

void FTurboSequence_Utility_Lf::DebugDrawSkeleton(const FSkinnedMeshRuntime_Lf& Runtime,
                                                  const FSkinnedMeshGlobalLibrary_Lf& Library,
                                                  const FColor LineColor, const UWorld* World)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	
	const FTransform& WorldSpaceTransform = Runtime.WorldSpaceTransform;
	
	//Evaluate like we do on the gpu in MeshUnit_CS_LF.usf

	FTransform BoneMatrix = FTransform::Identity;

	FTransform ParentCache[FTurboSequence_BoneTransform_CS_Lf::EvaluationParentCacheSize];
	
	int32 BoneIndex = 0; 
	
	for (const auto & CPUBoneToGPUBone : Runtime.DataAsset->CPUBoneToGPUBoneIndicesMap)
	{
		const int CPUBoneIndex = CPUBoneToGPUBone.Key; 
		const int GPUParentCacheRead = Runtime.DataAsset->GPUParentCacheRead[BoneIndex];
		const int GPUParentCacheWrite = Runtime.DataAsset->GPUParentCacheWrite[BoneIndex];

		if( GPUParentCacheRead == -1 )
		{
			BoneMatrix = FTransform::Identity;
		}
		else if (GPUParentCacheRead > 0)
		{
			BoneMatrix = ParentCache[GPUParentCacheRead -1];
		}

		FTransform ParentMatrix = BoneMatrix;

		if (const FOverrideBoneTransform_Lf* IKBoneData = Runtime.OverrideBoneTransforms.Find(CPUBoneIndex))
		{
			BoneMatrix = IKBoneData->OverrideTransform * BoneMatrix;
		}
		else
		{
			BoneMatrix = BendBoneFromAnimations(CPUBoneIndex, Runtime, Library) * BoneMatrix;
		}

		//Draw bone
		DrawDebugLine(World, (ParentMatrix * WorldSpaceTransform).GetTranslation(), (BoneMatrix * WorldSpaceTransform).GetTranslation(), LineColor, false, -1, 1);

		if(GPUParentCacheWrite > 0)
		{
			ParentCache[GPUParentCacheWrite - 1] = BoneMatrix;
		}

		BoneIndex++;
	}
#endif
}



void FTurboSequence_Utility_Lf::GetBoneTransform(FTransform& OutAtom, const int32 BoneIndex, const FSkinnedMeshRuntime_Lf& Runtime,
                                                 const FSkinnedMeshGlobalLibrary_Lf& Library,
                                                 const EBoneSpaces::Type Space)
{
	const FReferenceSkeleton& ReferenceSkeleton = GetReferenceSkeleton(Runtime.DataAsset);
	OutAtom = FTransform::Identity;
	int32 RuntimeIndex = BoneIndex;
	while (RuntimeIndex != INDEX_NONE)
	{
		if (const FOverrideBoneTransform_Lf* IKBoneData = Runtime.OverrideBoneTransforms.Find(RuntimeIndex))
		{
			OutAtom *= IKBoneData->OverrideTransform;
			break;
		}

		OutAtom *= BendBoneFromAnimations(RuntimeIndex, Runtime, Library);

		RuntimeIndex = GetSkeletonParentIndex(ReferenceSkeleton, RuntimeIndex);
	}

	if (Space == EBoneSpaces::Type::WorldSpace)
	{
		OutAtom *= Runtime.WorldSpaceTransform;
	}
}

bool FTurboSequence_Utility_Lf::OverrideBoneTransform(const FTransform& Atom, const int32 BoneIndex,
                                                      FSkinnedMeshRuntime_Lf& Runtime,
                                                      const EBoneSpaces::Type Space)
{
	if (!Runtime.DataAsset->CPUBoneToGPUBoneIndicesMap.Contains(BoneIndex))
	{
		return false;
	}

	FTransform IKTransform = Atom;

	// We only work with local space on the GPU
	// which mean we need to bring the World Space Transform into Local Space
	if (Space == EBoneSpaces::Type::WorldSpace)
	{
		IKTransform *= Runtime.WorldSpaceTransform.Inverse();
	}

	if(FOverrideBoneTransform_Lf* OverrideBoneTransform = Runtime.OverrideBoneTransforms.Find(BoneIndex))
	{
		OverrideBoneTransform->OverrideTransform = IKTransform;
	}
	else
	{
		FOverrideBoneTransform_Lf Data;
		Data.OverrideTransform = IKTransform;
		Runtime.OverrideBoneTransforms.Add(BoneIndex, Data);
	}

	return true;
}

bool FTurboSequence_Utility_Lf::RemoveOverrideBoneTransform(const int32 BoneIndex, FSkinnedMeshRuntime_Lf& Runtime)
{
	return Runtime.OverrideBoneTransforms.Remove(BoneIndex) > 0;
}

FTurboSequence_PoseCurveData_Lf FTurboSequence_Utility_Lf::GetAnimationCurveByAnimation(
	const FAnimationMetaData_Lf& Animation,
	const FSkinnedMeshGlobalLibrary_Lf& Library, const FName& CurveName)
{
	if (IsValid(Animation.Animation))
	{
		const FAnimationLibraryData_Lf& LibraryData = Library.AnimationLibraryData[Animation.AnimationLibraryHash];

		if (LibraryData.KeyframeIndexToPose.Contains(Animation.CPUAnimationIndex_0))
		{
			float PoseCurve0 = FTurboSequence_Helper_Lf::GetAnimationCurveWeight(
				LibraryData.KeyframeIndexToPose[Animation.CPUAnimationIndex_0].Pose, CurveName);

			return FTurboSequence_PoseCurveData_Lf(Animation.Animation, CurveName, PoseCurve0);
		}
	}

	return FTurboSequence_PoseCurveData_Lf();
}

TArray<FTurboSequence_PoseCurveData_Lf> FTurboSequence_Utility_Lf::GetAnimationCurveAtTime(
	const FSkinnedMeshRuntime_Lf& Runtime, const FName& CurveName, const FSkinnedMeshGlobalLibrary_Lf& Library,
	FCriticalSection& CriticalSection)
{
	CriticalSection.Lock();
	TArray<FTurboSequence_PoseCurveData_Lf> Array;
	CriticalSection.Unlock();
	for (const FAnimationMetaData_Lf& Animation : Runtime.AnimationMetaData)
	{
		const FTurboSequence_PoseCurveData_Lf& Curve = GetAnimationCurveByAnimation(
			Animation, Library, CurveName);
		if (IsValid(Curve.CurveAnimation))
		{
			CriticalSection.Lock();
			Array.Add(Curve);
			CriticalSection.Unlock();
		}
	}
	return Array;
}

bool FTurboSequence_Utility_Lf::GetSocketTransform(FTransform& OutTransform, const FName& SocketName,
                                                   FSkinnedMeshRuntime_Lf& Runtime,
                                                   FSkinnedMeshGlobalLibrary_Lf& Library,
                                                   const EBoneSpaces::Type Space)
{
	if(const USkeletalMeshSocket* Socket = Runtime.DataAsset->ReferenceMeshNative->FindSocket(SocketName))
	{
		const int32 BoneIndex = GetSkeletonBoneIndex(GetReferenceSkeleton(Runtime.DataAsset), Socket->BoneName);

		if(BoneIndex != INDEX_NONE)
		{
			GetBoneTransform(OutTransform, BoneIndex, Runtime, Library, Space);

			OutTransform = Socket->GetSocketLocalTransform() * OutTransform;

			return true;
		}
	}

	return false;
	
}

void FTurboSequence_Utility_Lf::GetBoneTransforms(TArray<FTransform>& OutTransforms, TArray<FName> BoneNames,
                                                  FSkinnedMeshRuntime_Lf& Runtime,
                                                  const FSkinnedMeshGlobalLibrary_Lf& Library,
                                                  const EBoneSpaces::Type Space)
{
	TArray<int32> BoneIndices;

	const FReferenceSkeleton &ReferenceSkeleton = GetReferenceSkeleton(Runtime.DataAsset);
	
	for (FName &BoneName : BoneNames)
	{
		BoneIndices.Add(GetSkeletonBoneIndex(ReferenceSkeleton, BoneName));
	}
	
	GetTransforms(OutTransforms, BoneIndices, Runtime, Library, Space);
}


