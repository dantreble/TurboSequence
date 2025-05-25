// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.


#include "TurboSequence_MinimalData_Lf.h"
#include "TurboSequence_Helper_Lf.h"

FBoneMaskSourceHandle FTurboSequence_MaskDefinition::GetHandle() const
{
	FBoneMaskSourceHandle Handle;
	for (const auto& BoneLayer : BoneLayerMasks)
	{
		FTurboSequence_Helper_Lf::HashDataTypeToHash(Handle.Hash, BoneLayer.BoneLayerName);
		FTurboSequence_Helper_Lf::HashDataTypeToHash(Handle.Hash, BoneLayer.BoneDepth);
	}
	return Handle;
}

FBoneMaskBuiltProxyHandle FTurboSequence_MaskDefinition::GetBuiltProxyHandle(UTurboSequence_MeshAsset_Lf *DataAsset) const
{
	FBoneMaskBuiltProxyHandle Handle;

	Handle.Hash = HashCombine(GetHandle().Hash, GetTypeHash(DataAsset));

	return Handle;
}

bool FTurboSequence_AnimNotifyQueue_Lf::PassesChanceOfTriggering(const FAnimNotifyEvent* Event) const
{
	return Event->NotifyStateClass ? true : RandomStream.FRandRange(0.f, 1.f) < Event->NotifyTriggerChance;
}

void FTurboSequence_AnimNotifyQueue_Lf::AddAnimNotifiesToDest(bool bSrcIsLeader, const TArray<FAnimNotifyEventReference>& NewNotifies, TArray<FAnimNotifyEventReference>& DestArray, const float InstanceWeight)
{
	// for now there is no filter whatsoever, it just adds everything requested
	for (const FAnimNotifyEventReference& NotifyRef : NewNotifies)
	{
		const FAnimNotifyEvent* Notify = NotifyRef.GetNotify();
		if( Notify && (bSrcIsLeader || Notify->bTriggerOnFollower))
		{
			// only add if it is over TriggerWeightThreshold
			const bool bPassesDedicatedServerCheck = Notify->bTriggerOnDedicatedServer || !IsRunningDedicatedServer();
			if (bPassesDedicatedServerCheck && Notify->TriggerWeightThreshold <= InstanceWeight && PassesChanceOfTriggering(Notify))
			{
				// Only add unique AnimNotifyState instances just once. We can get multiple triggers if looping over an animation.
				// It is the same state, so just report it once.
				Notify->NotifyStateClass ? DestArray.AddUnique(NotifyRef) : DestArray.Add(NotifyRef);
			}
		}
	}
}

void FTurboSequence_AnimNotifyQueue_Lf::AddAnimNotifiesToDestNoFiltering(const TArray<FAnimNotifyEventReference>& NewNotifies, TArray<FAnimNotifyEventReference>& DestArray) const
{
	for (const FAnimNotifyEventReference& NotifyRef : NewNotifies)
	{
		if (const FAnimNotifyEvent* Notify = NotifyRef.GetNotify())
		{
			Notify->NotifyStateClass ? DestArray.AddUnique(NotifyRef) : DestArray.Add(NotifyRef);
		}
	}
}

void FTurboSequence_AnimNotifyQueue_Lf::AddAnimNotify(const FAnimNotifyEvent* Notify, const UObject* NotifySource)
{
	AnimNotifies.Add(FAnimNotifyEventReference(Notify, NotifySource));
}

void FTurboSequence_AnimNotifyQueue_Lf::AddAnimNotifies(bool bSrcIsLeader, const TArray<FAnimNotifyEventReference>& NewNotifies, const float InstanceWeight)
{
	AddAnimNotifiesToDest(bSrcIsLeader, NewNotifies, AnimNotifies, InstanceWeight);
}

void FTurboSequence_AnimNotifyQueue_Lf::Reset(USkeletalMeshComponent* Component)
{
	AnimNotifies.Reset();
}
