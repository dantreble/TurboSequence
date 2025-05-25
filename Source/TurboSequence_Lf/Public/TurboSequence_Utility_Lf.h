// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TurboSequence_Data_Lf.h"

class UTurboSequence_MeshAsset_Lf;
class UNiagaraSystem;
class UTurboSequence_GlobalData_Lf;

class TURBOSEQUENCE_LF_API FTurboSequence_Utility_Lf
{
public:
	FTurboSequence_Utility_Lf() = delete;
	~FTurboSequence_Utility_Lf() = delete;
	

	// Licence Start
	// Copyright Epic Games, Inc. All Rights Reserved.
	/**
 * Utility function to determine the two key indices to interpolate given a relative position in the animation
 *
 * @param	RelativePos		The relative position to solve in the range [0,1] inclusive.
 * @param	NumKeys			The number of keys present in the track being solved.
 * @param	Interpolation
 * @param	PosIndex0Out	Output value for the closest key index before the RelativePos specified.
 * @param	PosIndex1Out	Output value for the closest key index after the RelativePos specified.
 * @return	The rate at which to interpolate the two keys returned to obtain the final result.
 */
	static float AnimationCodecTimeToIndex(
		//float SequenceLength,
		float RelativePos,
		int32 NumKeys,
		EAnimInterpolationType Interpolation,
		int32& PosIndex0Out,
		int32& PosIndex1Out);

	// Licence End

	/**
	 * Updates the cameras in the given array with the views from the world.
	 *
	 * @param OutView The array to store the updated camera views.
	 * @param InWorld The world object to retrieve the camera views from.
	 */
	static void UpdateCameras(TArray<FCameraView_Lf>& OutView, const UWorld* InWorld);

	/**
 * Updates the cameras in the scene.
 *
 * @param OutViews The array to store the updated camera views.
 * @param LastFrameCameraTransforms The map of the last frame's camera transforms.
 * @param InWorld The world in which the cameras are located.
 * @param DeltaTime The time elapsed since the last frame.
 *
 * @throws None
 */
	static void UpdateCameras_1(TArray<FCameraView_Lf>& OutViews,
	                            const TMap<uint8, FTransform>& LastFrameCameraTransforms,
	                            const UWorld* InWorld, float DeltaTime);

	/**
 * Updates the last frame camera transforms based on the provided camera views.
 *
 * @param OutLastFrameCameraTransforms The map to store the last frame camera transforms.
 * @param CameraViews The array of camera views to update the transforms from.
 *
 * @throws None
 */
	static void UpdateCameras_2(TMap<uint8, FTransform>& OutLastFrameCameraTransforms,
	                            const TArray<FCameraView_Lf>& CameraViews);

	/**
 * Determines if a skinned mesh is visible to any of the player cameras.
 *
 * @param Runtime The runtime data of the skinned mesh.
 * @param PlayerViews An array of camera views representing the player's perspective.
 *
 * @return void
 *
 * @throws None
 */
	static bool IsMeshVisible(const FSkinnedMeshRuntime_Lf& Runtime,
	                          const TArray<FCameraView_Lf>& PlayerViews);
	/**
	 * Refreshes asynchronous chunked mesh data based on the provided global data and libraries.
	 *
	 * @param Library The global library containing the mesh data to refresh.
	 * @param Library_RenderThread The render thread library containing the mesh data to refresh.
	 *
	 * @throws None
	 */
	static void RefreshAsyncChunkedMeshData(
		FSkinnedMeshGlobalLibrary_Lf& Library,
		FSkinnedMeshGlobalLibrary_RenderThread_Lf& Library_RenderThread);
	/**
	 * Creates maximum level of details and CPU bones for the provided global library.
	 *
	 * @param Library The global library to create max LOD and CPU bones for.
	 *
	 * @return None
	 *
	 * @throws None
	 */
	static void UpdateMaxBones(FSkinnedMeshGlobalLibrary_Lf& Library);
	/**
	 * Creates bone maps for the given skinned mesh global library and render thread library.
	 *
	 * @param Library The skinned mesh global library to create bone maps for.
	 * @param Library_RenderThread The skinned mesh render thread library to create bone maps for.
	 *
	 * @throws None
	 */
	static void CreateBoneMaps(FSkinnedMeshGlobalLibrary_Lf& Library,
	                           FSkinnedMeshGlobalLibrary_RenderThread_Lf& Library_RenderThread);

	/**
	 * Creates the inverse reference pose for a given skinned mesh global library and render thread library.
	 *
	 * @param Library The skinned mesh global library to create the inverse reference pose for.
	 * @param Library_RenderThread The skinned mesh render thread library to create the inverse reference pose for.
	 *
	 * @throws None
	 */
	static void CreateInverseReferencePose(FSkinnedMeshGlobalLibrary_Lf& Library,
	                                       FSkinnedMeshGlobalLibrary_RenderThread_Lf& Library_RenderThread);

	/**
	 * Adds a pose to the chunked library with multi-threading support.
	 *
	 * @param CPUIndex The cpu index of the pose to add.
	 * @param Library The skinned mesh global library to add the animation.
	 * @param Animation The metadata of the animation to add.
	 * @param LibraryAnimData 
	 * @param ReferenceSkeleton The ReferenceSkeleton of the animation to add.
	 * @param AnimationSkeleton The AnimationSkeleton of the animation to add.
	 *
	 * @return Success.
	 *
	 * @throws None
	 */
	
	static int32 AddAnimationPoseToLibraryChunked(const int32 CPUIndex,
	                                              FSkinnedMeshGlobalLibrary_Lf& Library,
	                                              const FAnimationMetaData_Lf& Animation,
	                                              FAnimationLibraryData_Lf& LibraryAnimData,
	                                              const FReferenceSkeleton& ReferenceSkeleton,
	                                              const FReferenceSkeleton& AnimationSkeleton);

	/**
	 * Adds an animation to the chunked library with multi-threading support.
	 *
	 * @param Library The skinned mesh global library to add the animation.
	 * @param CPUIndex0 The index for the CPU Frame before.
	 * @param GPUIndex0 The index for the GPU Frame before.
	 * @param CPUIndex1 The index for the CPU Frame after.
	 * @param GPUIndex1 The index for the GPU Frame after.
	 * @param FrameAlpha The interpolation percentage between the frme before and after 
	 * @param Runtime The skinned mesh runtime data for the animation.
	 * @param Animation The metadata of the animation to add.
	 *
	 * @return Success.
	 *
	 * @throws None
	 */
	static bool AddAnimationToLibraryChunked(FSkinnedMeshGlobalLibrary_Lf& Library,
	                                         int32& CPUIndex0,
	                                         int32& GPUIndex0,
	                                         int32& CPUIndex1, int32& GPUIndex1, float& FrameAlpha,
	                                         const FSkinnedMeshRuntime_Lf& Runtime,
	                                         const FAnimationMetaData_Lf& Animation);


	/**
	 * Checks if any animation layer from array A is present in array B.
	 *
	 * @param A The first array of FTurboSequence_BoneLayer_Lf objects.
	 * @param B The second array of FTurboSequence_BoneLayer_Lf objects.
	 *
	 * @return True if any animation layer from array A is present in array B, false otherwise.
	 */
	static bool AnyMatchingSourceBoneNames(const TArray<FTurboSequence_BoneLayer_Lf>& A,
	                                      const TArray<FTurboSequence_BoneLayer_Lf>& B);
	/**
	 * Checks if the root bone name is present in the given bone layer collection.
	 *
	 * @param Collection The array of bone layers to search for the root bone name.
	 * @param Runtime The runtime data of the skinned mesh.
	 *
	 * @return True if the root bone name is present, false otherwise.
	 *
	 * @throws None
	 */
	static bool ContainsRootBoneName(const TArray<FTurboSequence_BoneLayer_Lf>& Collection,
	                                 const FSkinnedMeshRuntime_Lf& Runtime);
	/**
	 * Generates an animation layer mask based on the given animation metadata.
	 *
	 * @param MaskDefinition
	 * @param OutLayers The array to store the generated layer mask.
	 * @param MeshAsset
	 *
	 * @return The handle of the generated animation layer mask.
	 *
	 * @throws None
	 */
	static void GenerateAnimationLayerMask(
		const FTurboSequence_MaskDefinition& MaskDefinition,
		TArray<uint16>& OutLayers,
		const TObjectPtr<UTurboSequence_MeshAsset_Lf>& MeshAsset);

	/**
	 * Adds an animation to the given skinned mesh runtime.
	 *
	 * @param Runtime The skinned mesh runtime to add the animation to.
	 * @param Animation The metadata of the animation to add.
	 * @param Library The global library of skinned meshes.
	 * @param bForceFront
	 *
	 * @throws None
	 */
	static void AddAnimation(FSkinnedMeshRuntime_Lf& Runtime,
	                         FAnimationMetaData_Lf& Animation,
	                         FSkinnedMeshGlobalLibrary_Lf& Library, const bool bForceFront);

	/**
	 * Removes an animation from the given skinned mesh runtime.
	 *
	 * @param Runtime The skinned mesh runtime from which to remove the animation.
	 * @param Library The global library of skinned meshes.
	 * @param Index The index of the animation to remove.
	 *
	 * @throws None
	 */
	static void RemoveAnimation(FSkinnedMeshRuntime_Lf& Runtime,
	                            FSkinnedMeshGlobalLibrary_Lf& Library,
	                            const int32 Index);
	/**
	 * Clears animations from the given skinned mesh runtime based on a condition.
	 *
	 * @tparam Function The type of the condition function.
	 * @param Runtime The skinned mesh runtime to clear animations from.
	 * @param Library The global library of skinned meshes.
	 * @param ForceMode The force mode for removing animations.
	 * @param CurrentLayers The current bone layers.
	 * @param Condition The condition function to determine which animations to clear.
	 *
	 * @throws None
	 */
	template <typename Function>
	static void ClearAnimations(
		FSkinnedMeshRuntime_Lf& Runtime,
		FSkinnedMeshGlobalLibrary_Lf& Library,
		ETurboSequence_AnimationForceMode_Lf ForceMode,
		const TArray<FTurboSequence_BoneLayer_Lf>& CurrentLayers,
		const Function& Condition)
	{
		for (int32 i = Runtime.AnimationMetaData.Num() - 1; i >= 1; --i) // First Anim -> Rest Pose, so we keep it
		{
			const FAnimationMetaData_Lf &AnimationMetaData_Lf = Runtime.AnimationMetaData[i];
			if (::Invoke(Condition, AnimationMetaData_Lf))
			{
				// Remove only animations in the same layer
				if (ForceMode == ETurboSequence_AnimationForceMode_Lf::PerLayer)
				{
					if (AnyMatchingSourceBoneNames(AnimationMetaData_Lf.Settings.MaskDefinition.BoneLayerMasks, CurrentLayers))
					{
						RemoveAnimation(Runtime, Library, i);
					}
				}
				// Might Implement an Ultra Force Animation mode where we remove all even they are
				// not in the same layer ...
				else
				{
					RemoveAnimation(Runtime, Library, i);
				}
			}
		}
	}

	/**
	 * Refreshes the state of the BlendSpace by ticking the BlendSpace and updating the Data.
	 *
	 * @param BlendSpace The BlendSpace to refresh.
	 * @param Data The animation blend space data to update.
	 * @param DeltaTime The time elapsed since the last update.
	 *
	 * @return True if the BlendSpace was successfully refreshed, false otherwise.
	 *
	 * @throws None
	 */
	static bool RefreshBlendSpaceState(const TObjectPtr<UBlendSpace> BlendSpace, FAnimationBlendSpaceData_Lf& Data,
	                                   float DeltaTime);
	/**
	 * Plays a BlendSpace animation for a given skinned mesh.
	 *
	 * @param Library The global library of skinned meshes.
	 * @param Runtime The runtime data of the skinned mesh.
	 * @param BlendSpace The BlendSpace animation to play.
	 * @param ThreadContext The thread context for multi-threading.
	 * @param AnimSettings The settings for the animation playback.
	 * @param OverrideWeight The optional override weight for the animation.
	 * @param OverrideStartTime The optional override start time for the animation.
	 * @param OverrideEndTime The optional override end time for the animation.
	 *
	 * @return The minimal blend space animation data.
	 *
	 * @throws None
	 */
	static FTurboSequence_AnimMinimalBlendSpace_Lf PlayBlendSpace(
		FSkinnedMeshGlobalLibrary_Lf& Library,
		FSkinnedMeshRuntime_Lf& Runtime,
		const TObjectPtr<UBlendSpace> BlendSpace,
		const TObjectPtr<UTurboSequence_ThreadContext_Lf>& ThreadContext, const FTurboSequence_AnimPlaySettings_Lf&
		AnimSettings,
		float OverrideWeight = INDEX_NONE,
		float OverrideStartTime = INDEX_NONE, float OverrideEndTime = INDEX_NONE);

	/**
	 * Plays an animation for a skinned mesh with specified settings.
	 *
	 * @param Library The global library of skinned meshes.
	 * @param Runtime The runtime data of the skinned mesh.
	 * @param Animation The animation to play.
	 * @param AnimSettings The settings for the animation playback.
	 * @param bIsLoop Flag indicating if the animation should loop.
	 * @param OverrideWeight The optional override weight for the animation.
	 * @param OverrideStartTime The optional override start time for the animation.
	 * @param OverrideEndTime The optional override end time for the animation.
	 * @param bForceFront
	 *
	 * @return The ID of the played animation.
	 *
	 * @throws None
	 */
	static FAnimationMetaDataHandle PlayAnimation(
		FSkinnedMeshGlobalLibrary_Lf& Library,
		FSkinnedMeshRuntime_Lf& Runtime,
		UAnimSequence* Animation,
		const FTurboSequence_AnimPlaySettings_Lf& AnimSettings,
		const bool bIsLoop,
		float OverrideWeight = INDEX_NONE,
		float OverrideStartTime = INDEX_NONE,
		float OverrideEndTime = INDEX_NONE, const bool bForceFront = false);
	/**
	 * Returns the animation sequence with the highest priority from the given runtime.
	 *
	 * @param Runtime The runtime data of the skinned mesh.
	 *
	 * @return A pointer to the UAnimSequence object with the highest priority, or nullptr if no animations are found.
	 *
	 * @throws None
	 */
	static TObjectPtr<UAnimSequence> GetHighestPriorityAnimation(
		const FSkinnedMeshRuntime_Lf& Runtime);
	/**
	 * Updates the blend spaces of a skinned mesh runtime.
	 *
	 * @param Runtime The runtime data of the skinned mesh.
	 * @param DeltaTime The time elapsed since the last update.
	 * @param Library The global library for the skinned mesh.
	 *
	 * @throws None
	 */
	static void UpdateBlendSpaces(FSkinnedMeshRuntime_Lf& Runtime,
	                              float DeltaTime,
	                              FSkinnedMeshGlobalLibrary_Lf& Library);
	/**
	 * Tweak the blend space of the skinned mesh runtime to the wanted position.
	 *
	 * @param Runtime The runtime data of the skinned mesh.
	 * @param BlendSpace The minimal blend space to tweak.
	 * @param WantedPosition The desired position to set in the blend space.
	 *
	 * @return True if the blend space was successfully tweaked, false otherwise.
	 *
	 * @throws None
	 */
	static bool TweakBlendSpace(FSkinnedMeshRuntime_Lf& Runtime,
	                            const FTurboSequence_AnimMinimalBlendSpace_Lf& BlendSpace,
	                            const FVector3f& WantedPosition);
	/**
	 * Tweak the animation of the skinned mesh runtime.
	 *
	 * @param Runtime The runtime data of the skinned mesh.
	 * @param Library The global library of skinned meshes.
	 * @param Settings The animation play settings.
	 * @param AnimationID The ID of the animation to tweak.
	 *
	 * @return True if the animation was successfully tweaked, false otherwise.
	 *
	 * @throws None
	 */
	static bool TweakAnimation(FSkinnedMeshRuntime_Lf& Runtime,
	                           FSkinnedMeshGlobalLibrary_Lf& Library,
	                           const FTurboSequence_AnimPlaySettings_Lf& Settings,
	                           const FAnimationMetaDataHandle& AnimationID);
	/**
	 * Solves animations for a given skinned mesh runtime.
	 *
	 * @param Runtime The runtime data of the skinned mesh.
	 * @param Library The global library of skinned meshes.
	 * @param DeltaTime The time elapsed since the last update.
	 * @param CurrentFrameCount The current frame count.
	 *
	 * @throws None
	 */
	static void SolveAnimations(FSkinnedMeshRuntime_Lf& Runtime, FSkinnedMeshGlobalLibrary_Lf& Library,
	                            const float DeltaTime,
	                            const int64 CurrentFrameCount);

	static void GetAnimNotifies(const FSkinnedMeshRuntime_Lf& Runtime, FTurboSequence_AnimNotifyQueue_Lf& NotifyQueue);
	
	/**
	 * Returns a FUintVector containing the hashed values of the given USkeleton, UTurboSequence_MeshAsset_Lf,
	 * and UAnimSequence pointers. If the UAnimSequence pointer is not valid, the third value of the FUintVector
	 * will be 0.
	 *
	 * @param Skeleton A pointer to the USkeleton object.
	 * @param Asset A pointer to the UTurboSequence_MeshAsset_Lf object.
	 * @param Animation A pointer to the UAnimSequence object.
	 *
	 * @return A FUintVector containing the hashed values of the given pointers.
	 *
	 * @throws None
	 */
	static FORCEINLINE FUintVector GetAnimationLibraryKey(const TObjectPtr<USkeleton> Skeleton,
	                                                      const TObjectPtr<UTurboSequence_MeshAsset_Lf>
	                                                      Asset, const UAnimSequence* Animation)
	{
		if (!IsValid(Animation))
		{
			return FUintVector(GetTypeHash(Skeleton), GetTypeHash(Asset), 0);
		}
		return FUintVector(GetTypeHash(Skeleton), GetTypeHash(Asset), GetTypeHash(Animation));
	}

	static FORCEINLINE const FReferenceSkeleton& GetReferenceSkeleton_Raw(
		const TObjectPtr<USkinnedAsset>& Mesh)
	{
		return Mesh->GetRefSkeleton();
	}

	static FORCEINLINE const FReferenceSkeleton& GetReferenceSkeleton(
		const TObjectPtr<UTurboSequence_MeshAsset_Lf>& Asset)
	{
		return Asset->ReferenceMeshNative->GetRefSkeleton();
	}

	static FORCEINLINE const TArray<FTransform>& GetSkeletonRefPose(
		const FReferenceSkeleton& ReferenceSkeleton)
	{
		return ReferenceSkeleton.GetRefBonePose();
	}

	static FORCEINLINE int32 GetSkeletonParentIndex(const FReferenceSkeleton& ReferenceSkeleton,
	                                                int32 Index)
	{
		return ReferenceSkeleton.GetParentIndex(Index);
	}

	static FORCEINLINE int32 GetSkeletonNumBones(const FReferenceSkeleton& ReferenceSkeleton)
	{
		return ReferenceSkeleton.GetNum();
	}

	static FORCEINLINE int32 GetSkeletonBoneIndex(const FReferenceSkeleton& ReferenceSkeleton,
	                                              const FName& BoneName)
	{
		return ReferenceSkeleton.FindBoneIndex(BoneName);
	}

	static FORCEINLINE FName GetSkeletonBoneName(const FReferenceSkeleton& ReferenceSkeleton,
	                                             int32 BoneIndex)
	{
		return ReferenceSkeleton.GetBoneName(BoneIndex);
	}

	static FORCEINLINE bool GetSkeletonIsValidIndex(const FReferenceSkeleton& ReferenceSkeleton,
	                                                int32 BoneIndex)
	{
		return ReferenceSkeleton.IsValidIndex(BoneIndex);
	}

	/**
* Retrieves the bone transform from local poses for a given bone index.
*
* @param BoneIndex The index of the bone.
* @param LibraryData The animation library data.
* @param ReferenceSkeleton The reference skeleton.
* @param AnimationSkeleton The animation skeleton.
* @param Pose The animation pose.
* @param Animation The animation sequence.
*
* @return The transpose matrix representing the bone transform.
*
* @throws None
*/
	static FTurboSequence_TransposeMatrix_Lf GetBoneTransformFromLocalPoses(int16 BoneIndex,
	                                                                        const FAnimationLibraryData_Lf& LibraryData,
	                                                                        const FReferenceSkeleton& ReferenceSkeleton,
	                                                                        const FReferenceSkeleton& AnimationSkeleton,
	                                                                        const FAnimPose_Lf& Pose,
	                                                                        const TObjectPtr<UAnimSequence> Animation);
	/**
* Retrieves the bone transform from the animation safely.
*
* @param OutAtom The output matrix to store the bone transform.
* @param Animation The animation metadata.
* @param SkeletonBoneIndex The index of the skeleton bone.
* @param Library The skinned mesh global library.
* @param ReferenceSkeleton The reference skeleton.
*
* @throws None
*/
	static void GetBoneTransformFromAnimationSafe(
		FMatrix& OutAtom, const FAnimationMetaData_Lf& Animation, int32 FrameIndex,
		uint16 SkeletonBoneIndex,
		const FSkinnedMeshGlobalLibrary_Lf& Library, const FReferenceSkeleton& ReferenceSkeleton);

	static void PrintAnimsToScreen(const FSkinnedMeshRuntime_Lf& Runtime);

	/**
* Calculates the bone transformation for a given bone index by blending the animations in the runtime.
*
* @param BoneIndex The index of the bone to calculate the transformation for.
* @param Runtime The runtime data of the skinned mesh.
* @param Library The global library of skinned meshes.
*
* @return The transformation of the bone calculated from the blended animations.
*
* @throws None
*/
	static FTransform BendBoneFromAnimations(const int32 BoneIndex,
	                                         const FSkinnedMeshRuntime_Lf& Runtime,
	                                         const FSkinnedMeshGlobalLibrary_Lf& Library);
	/**
* Extracts root motion from animations for the given skinned mesh runtime.
*
* @param OutAtom The output atom to store the extracted root motion.
* @param Runtime The runtime data of the skinned mesh.
* @param DeltaTime The time elapsed since the last update.
*
* @return None
*
* @throws None
*/
	static void ExtractRootMotionFromAnimations(FTransform& OutAtom,
	                                            const FSkinnedMeshRuntime_Lf& Runtime,
	                                            float DeltaTime);


	/**
	 * 
	 * @param OutAtoms 
	 * @param BoneIndices 
	 * @param Runtime The skinned mesh runtime data.
 	 * @param Library The global library of skinned meshes.
	 * @param Space The bone space type for the transformation.
	 */
	static void GetTransforms(TArray<FTransform>& OutAtoms, const TArray<int32>& BoneIndices, FSkinnedMeshRuntime_Lf& Runtime,
	                          const FSkinnedMeshGlobalLibrary_Lf& Library,
	                          const EBoneSpaces::Type Space);

	static void DebugDrawSkeleton(const FSkinnedMeshRuntime_Lf& Runtime,
	                              const FSkinnedMeshGlobalLibrary_Lf& Library,
	                              FColor LineColor, const UWorld* World);
	/**
* Calculates the IK transform for a specific bone index based on the skinned mesh runtime.
*
* @param OutAtom The output transform for the bone.
* @param BoneIndex The index of the bone to calculate the IK transform for.
* @param Runtime The skinned mesh runtime data.
* @param Library The global library of skinned meshes.
* @param Space The bone space type for the transformation.
*
* @throws None
*/
	static void GetBoneTransform(FTransform& OutAtom, int32 BoneIndex,
	                             const FSkinnedMeshRuntime_Lf& Runtime,
	                             const FSkinnedMeshGlobalLibrary_Lf& Library,
	                             const EBoneSpaces::Type Space);
	/**
* Sets the IK transform for a bone in the skinned mesh runtime.
*
* @param Atom The transform to set.
* @param BoneIndex The index of the bone.
* @param Runtime The skinned mesh runtime.
* @param Space The bone space type for the transformation. Defaults to WorldSpace.
*
* @return true if the IK transform was successfully set, false otherwise.
*
* @throws None
*/
	static bool OverrideBoneTransform(const FTransform& Atom, const int32 BoneIndex,
	                                  FSkinnedMeshRuntime_Lf& Runtime,
	                                  const EBoneSpaces::Type Space = EBoneSpaces::WorldSpace);

	static bool RemoveOverrideBoneTransform(int32 BoneIndex, FSkinnedMeshRuntime_Lf& Runtime);

	/**
* Retrieves the animation curve data for a given animation.
*
* @param Animation The animation metadata.
* @param Library The global animation library.
* @param CurveName The name of the curve to retrieve.
*
* @return The pose curve data associated with the animation curve.
*
* @throws None
*/
	static FTurboSequence_PoseCurveData_Lf GetAnimationCurveByAnimation(
		const FAnimationMetaData_Lf& Animation,
		const FSkinnedMeshGlobalLibrary_Lf& Library, const FName& CurveName);
	/**
* Retrieves the animation curve data at a specific time for a given curve name.
*
* @param Runtime The runtime data of the skinned mesh.
* @param CurveName The name of the curve.
* @param Library The global library data of the skinned mesh.
* @param CriticalSection The critical section for thread safety.
*
* @return An array of FTurboSequence_PoseCurveData_Lf objects representing the animation curve data.
*
* @throws None
*/
	static TArray<FTurboSequence_PoseCurveData_Lf> GetAnimationCurveAtTime(
		const FSkinnedMeshRuntime_Lf& Runtime, const FName& CurveName, const FSkinnedMeshGlobalLibrary_Lf& Library,
		FCriticalSection& CriticalSection);
	/**
* Retrieves the transform of a socket in the given skinned mesh runtime.
*
* @param OutTransform The output transform of the socket.
* @param SocketName The name of the socket.
* @param Runtime The skinned mesh runtime.
* @param Library The global library of skinned meshes.
* @param Space The bone space type for the transformation.
*
* @return true if the socket was found 
* @throws None
*/
	static bool GetSocketTransform(FTransform& OutTransform, const FName& SocketName,
	                        FSkinnedMeshRuntime_Lf& Runtime,
	                        FSkinnedMeshGlobalLibrary_Lf& Library,
	                        const EBoneSpaces::Type Space);


/**
* Retrieves the transform of a an array of bones in the given skinned mesh runtime.
*
* @param OutTransforms 
* @param BoneNames 
* @param Runtime The skinned mesh runtime.
* @param Library The global library of skinned meshes.
* @param Space The bone space type for the transformation.
*
* @throws None
*/
static void GetBoneTransforms(TArray<FTransform>& OutTransforms, TArray<FName> BoneNames,
                              FSkinnedMeshRuntime_Lf& Runtime,
                              const FSkinnedMeshGlobalLibrary_Lf& Library,
                              const EBoneSpaces::Type Space);
};
