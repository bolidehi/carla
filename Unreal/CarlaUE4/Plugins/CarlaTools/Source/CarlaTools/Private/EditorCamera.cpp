#include "EditorCamera.h"
#include "UnrealClient.h"
#include "Editor/EditorEngine.h"
#include "EditorViewportClient.h"

void AEditorCameraUtils::Get()
{
	auto ViewportClient = dynamic_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	CameraTransform = FTransform();
	CameraTransform.SetLocation(ViewportClient->GetViewLocation());
	CameraTransform.SetRotation(FQuat(ViewportClient->GetViewRotation()));
}

void AEditorCameraUtils::Set()
{
	auto ViewportClient = dynamic_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	ViewportClient->SetViewLocation(CameraTransform.GetLocation());
	ViewportClient->SetViewRotation(FRotator(CameraTransform.GetRotation()));
}
