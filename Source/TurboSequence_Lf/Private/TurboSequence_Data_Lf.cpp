// Copyright Lukas Fratzl, 2022-2024. All Rights Reserved.


#include "TurboSequence_Data_Lf.h"

#include "NiagaraComponent.h"

void UTurboSequenceRenderAttachmentData::PrintRenderData() const
{
	Super::PrintRenderData();
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::Printf(TEXT("Attachment Position %d, Rotation %d, Scale %d"),bChangedParticleAttachmentPosition, bChangedParticleAttachmentRotation, bChangedParticleAttachmentScale ));
}



int32 FSkinnedMeshRuntime_Lf::GetAnimIndex(FAnimationMetaDataHandle AnimationMetaDataHandle) const
{
	return AnimationMetaData.IndexOfByPredicate([&](const FAnimationMetaData_Lf& MetaData) {
		return MetaData.AnimationID == AnimationMetaDataHandle;
	});
}

const FAnimationMetaData_Lf* FSkinnedMeshRuntime_Lf::GetAnimMetaData(
	FAnimationMetaDataHandle AnimationMetaDataHandle) const
{
	if(const int32 AnimIndex = GetAnimIndex(AnimationMetaDataHandle); AnimIndex != INDEX_NONE)
	{
		return &AnimationMetaData[AnimIndex];
	}

	return nullptr;
}

FAnimationMetaData_Lf* FSkinnedMeshRuntime_Lf::GetAnimMetaData(
	FAnimationMetaDataHandle AnimationMetaDataHandle)
{
	if(const int32 AnimIndex = GetAnimIndex(AnimationMetaDataHandle); AnimIndex != INDEX_NONE)
	{
		return &AnimationMetaData[AnimIndex];
	}

	return nullptr;
}


FAttachmentMeshHandle FSkinnedMeshRuntime_Lf::AddAttachment(FTurboSequenceRenderHandle TurboSequenceRenderHandle)
{
	FSkinnedMeshAttachmentRuntime NewAttachment;

	const FAttachmentMeshHandle AttachmentMeshHandle(MeshID, LastAnimationID++);

	NewAttachment.AttachmentHandle = AttachmentMeshHandle;
	NewAttachment.RenderHandle = TurboSequenceRenderHandle;

	Attachments.Add(NewAttachment);
	
	return AttachmentMeshHandle;
}

void FSkinnedMeshRuntime_Lf::AddAttachedParticle(TObjectPtr<UNiagaraComponent> NiagaraComponent)
{
	AttachedParticles.Add(NiagaraComponent);
}

void FSkinnedMeshRuntime_Lf::RemoveAllAttachedParticles(const bool bDestroy)
{
	if(bDestroy)
	{
		for (const TObjectPtr<UNiagaraComponent>& NiagaraComponent : AttachedParticles)
		{
			NiagaraComponent->DestroyComponent();
		}
	}

	AttachedParticles.Empty();
}

bool FSkinnedMeshRuntime_Lf::RemoveAttachedParticle(UNiagaraComponent* NiagaraComponent, const bool bDestroy)
{
	const bool bRemoved = AttachedParticles.RemoveSwap(NiagaraComponent) > 0;

	if(bRemoved && bDestroy && IsValid(NiagaraComponent))
	{
		NiagaraComponent->DestroyComponent();
	}
	
	return bRemoved;
}
