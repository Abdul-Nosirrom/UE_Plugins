// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "CanvasTypes.h"
#include "UObject/Object.h"
#include "ComponentVisualizer.h"
#include "ScriptComponentVisualizer.generated.h"

UCLASS(Blueprintable)
class ANGELSCRIPTEDITORUTILITIES_API UScriptComponentVisualizer : public UObject
{
	friend class UScriptComponentVisualizerRegistry;
	friend class FScriptComponentVisualizer;
	
	GENERATED_BODY()

protected:
	void SetPDI(FPrimitiveDrawInterface* InPDI) { PDI = InPDI; }
	void SetCanvas(FCanvas* InCanvas, const FViewport* InViewport, const FSceneView* InView) { Canvas = InCanvas; Viewport = const_cast<FViewport*>(InViewport), View = const_cast<FSceneView*>(InView); }
	
	UFUNCTION(BlueprintImplementableEvent)
	void VisualizeComponent(const UActorComponent* Component);
	UFUNCTION(BlueprintImplementableEvent)
	void VisualizeHUD(const UActorComponent* Component);

	virtual UWorld* GetWorld() const override
	{
		return GEditor->GetWorld();
	}

	// CUSTOM DRAW UTILITY
	UFUNCTION(ScriptCallable)
	FVector2D GetViewportSize() const
	{
		if (Viewport)
		{
			auto Size = Viewport->GetSizeXY();
			return FVector2D(Size.X, Size.Y);
		}
		return FVector2D::ZeroVector;
	}

	UFUNCTION(ScriptCallable)
	FVector2D WorldToScreen(const FVector& WorldPos) const
	{
		FVector2D OutScreenPos = FVector2D::ZeroVector;
		if (View)
		{
			View->WorldToPixel(WorldPos, OutScreenPos);
		}
		return OutScreenPos;
	}
	
	UFUNCTION(ScriptCallable)
	void DrawTrajectory(const FVector& Position, const FVector& Velocity, const FVector& Acceleration = FVector::ZeroVector, const FLinearColor& Color = FLinearColor::White, float Time = 5.f, float DT = 0.5f, float Thickness = 0.f, bool bDashed = false, float DashSize = 50.f)
	{
		if (DT <= 0 || Time <= 0.f)
		{
			return;
		}
		
		FVector trajPos = Position;
		FVector currentVelocity = Velocity;

		float currentTime = 0.f;
		while (currentTime <= Time)
		{
			const FVector posDelta = currentVelocity * DT;
			
			// Draw lines
			if (PDI)
			{
				if (bDashed)
				{
					DrawDashedLine(trajPos, trajPos + posDelta, Color, DashSize);
				}
				else
				{
					DrawLine(trajPos, trajPos + posDelta, Color, SDPG_Foreground, Thickness);	
				}
			}
			// Draw TimeStep Text
			if (Canvas)
			{
				FVector2D ScreenPos = WorldToScreen(trajPos);
				const FString Text = FString::Printf(TEXT("t = %.2f\n v = %s"), currentTime, *Velocity.ToString());
				int32 textSize = DrawShadowedString(Text, ScreenPos.X, ScreenPos.Y, Color);
				//DrawTile(ScreenPos.X, ScreenPos.Y, textSize, textSize)
			}

			trajPos += posDelta;
			currentVelocity += Acceleration * DT;
			currentTime += DT;
		}
	}

	// CANVAS DRAW UTILITY
	UFUNCTION(ScriptCallable)
	void DrawTile(double X, double Y, double SizeX, double SizeY, float U, float V, float SizeU, float SizeV, const FLinearColor& Color, const UTexture2D* Texture = nullptr, bool bAlphaBlend = false) const
	{
		if (Canvas)
			Canvas->DrawTile(X, Y, SizeX, SizeY, U, V, SizeU, SizeV, Color, Texture == nullptr ? nullptr : Texture->GetResource(), bAlphaBlend);
	}
	
	UFUNCTION(ScriptCallable)
	int32 DrawShadowedString(const FString& Text, double StartX, double StartY, const FLinearColor& Color = FLinearColor::White, const FLinearColor& ShadowColor = FLinearColor::Black) const
	{
		if (Canvas)
			return Canvas->DrawShadowedString(StartX, StartY, *Text, GEngine->GetSmallFont(), Color, ShadowColor);
		return -1;
	}

	UFUNCTION(ScriptCallable)
	void DrawNGon(const FVector2D& Center, const FColor& Color, int32 NumSides, float Radius) const
	{
		if (Canvas)
			Canvas->DrawNGon(Center, Color, NumSides, Radius);
	}

	// PDI DRAW UTILITY
	UFUNCTION(ScriptCallable)
	void DrawSprite(
		const FVector& Position,
		float SizeX,
		float SizeY,
		const UTexture2D* Sprite,
		const FLinearColor& Color,
		ESceneDepthPriorityGroup DepthPriorityGroup,
		float U,
		float UL,
		float V,
		float VL
		) const
	{
		if (PDI)
			PDI->DrawSprite(Position, SizeX, SizeY, Sprite->GetResource(), Color, DepthPriorityGroup, U, UL, V, VL);
	}

	UFUNCTION(ScriptCallable)
	void DrawLine(
		const FVector& Start,
		const FVector& End,
		const FLinearColor& Color,
		ESceneDepthPriorityGroup DepthPriorityGroup = SDPG_Foreground,
		float Thickness = 0.0f,
		float DepthBias = 0.0f,
		bool bScreenSpace = false
		) const
	{
		if (PDI)
			PDI->DrawLine(Start, End, Color, DepthPriorityGroup, Thickness, DepthBias, bScreenSpace);
	}

	UFUNCTION(ScriptCallable)
	void DrawPoint(
		const FVector& Position,
		const FLinearColor& Color,
		float PointSize,
		ESceneDepthPriorityGroup DepthPriorityGroup = SDPG_Foreground
		) const
	{
		if (PDI)
			PDI->DrawPoint(Position, Color, PointSize, DepthPriorityGroup);
	}
	
	// GLOBAL DRAW UTILITY
	UFUNCTION(ScriptCallable)
	void DrawWireBox(const FBox& Box, const FLinearColor& Color, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.f, float DepthBias = 0.f, bool bScreenSpace = false) const
	{
		if (PDI)
			::DrawWireBox(PDI, Box, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}

	UFUNCTION(ScriptCallable)
	void DrawCircle(const FVector& Base, const FVector& X, const FVector& Y, const FLinearColor& Color, double Radius, int32 NumSides = 16,  ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false) const
	{
		if (PDI)
			::DrawCircle(PDI, Base, X, Y, Color, Radius, NumSides, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}

	UFUNCTION(ScriptCallable)
	void DrawArc(FVector Base, const FVector X, const FVector Y, const float MinAngle, const float MaxAngle, const double Radius, const int32 Sections, const FLinearColor& Color, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground) const
	{
		if (PDI)
			::DrawArc(PDI, Base, X, Y, MinAngle, MaxAngle, Radius, Sections, Color, DepthPriority);
	}

	UFUNCTION(ScriptCallable)
	void DrawRectangle(const FVector& Center, const FVector& XAxis, const FVector& YAxis, FColor Color, float Width, float Height, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false) const
	{
		if (PDI)
			::DrawRectangle(PDI, Center, XAxis, YAxis, Color, Width, Height, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}

	UFUNCTION(ScriptCallable)
	void DrawWireSphere(const FVector& Base, const FLinearColor& Color, double Radius, int32 NumSides = 16,  ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false) const
	{
		if (PDI)
			::DrawWireSphere(PDI, Base, Color, Radius, NumSides, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	
	UFUNCTION(ScriptCallable)
	void DrawWireCylinder(const FVector& Base, const FVector& X, const FVector& Y, const FVector& Z, const FLinearColor& Color, double Radius, double HalfHeight, int32 NumSides = 16, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false) const
	{
		if (PDI)
			::DrawWireCylinder(PDI, Base, X, Y, Z, Color, Radius, HalfHeight, NumSides, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}
	
	UFUNCTION(ScriptCallable)
	void DrawWireCapsule(const FVector& Base, const FVector& X, const FVector& Y, const FVector& Z, const FLinearColor& Color, double Radius, double HalfHeight, int32 NumSides = 16, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false) const
	{
		if (PDI)
			::DrawWireCapsule(PDI, Base, X, Y, Z, Color, Radius, HalfHeight, NumSides, DepthPriority, Thickness, DepthBias, bScreenSpace);
	}

	UFUNCTION(ScriptCallable)
	void DrawWireChoppedCone(const FVector& Base,const FVector& X,const FVector& Y,const FVector& Z,const FLinearColor& Color,double Radius,double TopRadius,double HalfHeight,int32 NumSides = 16, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground) const
	{
		if (PDI)
			::DrawWireChoppedCone(PDI, Base, X, Y, Z, Color, Radius, TopRadius, HalfHeight, NumSides, DepthPriority);		
	}

	UFUNCTION(ScriptCallable)
	void DrawOrientedWireBox(const FVector& Base, const FVector& X, const FVector& Y, const FVector& Z, FVector Extent, const FLinearColor& Color, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.0f, float DepthBias = 0.0f, bool bScreenSpace = false) const
	{
		if (PDI)
			::DrawOrientedWireBox(PDI, Base, X, Y, Z, Extent, Color, DepthPriority, Thickness, DepthBias, bScreenSpace);		
	}

	UFUNCTION(ScriptCallable)
	void DrawDirectionalArrow(const FMatrix& ArrowToWorld, const FLinearColor& InColor, float Length, float ArrowSize, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.0f) const
	{
		if (PDI)
			::DrawDirectionalArrow(PDI, ArrowToWorld, InColor, Length, ArrowSize, DepthPriority, Thickness);
	}

	UFUNCTION(ScriptCallable)
	void DrawConnectedArrow(const FMatrix& ArrowToWorld, const FLinearColor& Color, float ArrowHeight, float ArrowWidth, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.5f, int32 NumSpokes = 6) const
	{
		if (PDI)
			::DrawConnectedArrow(PDI, ArrowToWorld, Color, ArrowHeight, ArrowWidth, DepthPriority, Thickness, NumSpokes);
	}

	UFUNCTION(ScriptCallable)
	void DrawWireStar(const FVector& Position, float Size, const FLinearColor& Color, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground) const
	{
		if (PDI)
			::DrawWireStar(PDI, Position, Size, Color, DepthPriority);
	}

	UFUNCTION(ScriptCallable)
	void DrawDashedLine(const FVector& Start, const FVector& End, const FLinearColor& Color, double DashSize = 50.f, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float DepthBias = 0.0f) const
	{
		if (PDI)
			::DrawDashedLine(PDI, Start, End, Color, DashSize, DepthPriority, DepthBias);
	}

	UFUNCTION(ScriptCallable)
	void DrawCoordinateSystem(FVector const& AxisLoc, FRotator const& AxisRot, float Scale, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground, float Thickness = 0.0f) const
	{
		if (PDI)
			::DrawCoordinateSystem(PDI, AxisLoc, AxisRot, Scale, DepthPriority, Thickness);
	}

	UFUNCTION(ScriptCallable)
	void DrawFrustumWireframe(const FMatrix& WorldToFrustum, FColor Color, ESceneDepthPriorityGroup DepthPriority = SDPG_Foreground) const
	{
		if (PDI)
			::DrawFrustumWireframe(PDI, WorldToFrustum, Color, DepthPriority);
	}
	
protected:
	FPrimitiveDrawInterface* PDI;
	FCanvas* Canvas;
	FViewport* Viewport;
	FSceneView* View;
	UPROPERTY(EditAnywhere)
	TSubclassOf<UActorComponent> VisualizedClass;
};

class FScriptComponentVisualizer : public FComponentVisualizer
{
	friend class UScriptComponentVisualizerRegistry;

public:
	FScriptComponentVisualizer(UScriptComponentVisualizer* InVisualizer)
	{
		Visualizer = InVisualizer;
	}

	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override
	{
		Visualizer->SetPDI(PDI);
		Visualizer->VisualizeComponent(Component);
		Visualizer->SetPDI(nullptr);
	}

	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override
	{
		Visualizer->SetCanvas(Canvas, Viewport, View);
		Visualizer->VisualizeHUD(Component);
		Visualizer->SetCanvas(nullptr, nullptr, nullptr);
	}

	/*
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override
	{
		return false;
	}

	virtual void EndEditing() override
	{
		
	}

	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override
	{
		return false;
	}

	virtual bool GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const override
	{
		return false;
	}

	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override
	{
		return false;
	}

	virtual bool HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event) override
	{
		return false;
	}

	virtual bool HandleModifiedClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click) override
	{
		return false;
	}

	virtual bool HandleBoxSelect(const FBox& InBox, FEditorViewportClient* InViewportClient, FViewport* InViewport) override
	{
		return false;
	}

	virtual bool HandleFrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, FViewport* InViewport) override
	{
		return false;
	}

	virtual bool HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox) override
	{
		return false;
	}

	virtual bool HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination) override
	{
		return false;
	}

	virtual void TrackingStarted(FEditorViewportClient* InViewportClient) override
	{
		
	}

	virtual void TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove) override
	{
		
	}

	virtual UActorComponent* GetEditedComponent() const override { return nullptr; }
	*/
	
private:
	TWeakObjectPtr<UScriptComponentVisualizer> Visualizer; // Set by ComponentVisualizerRegistry upon creation
};