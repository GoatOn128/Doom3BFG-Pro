#pragma once

class idImage;

enum renderClass_t {
	TC_DEPTH = 0,
	TC_RGBA = 1,
	TC_RGBA_DEPTH = 2,
	TC_FLOAT_RGBA = 3,
	TC_FLOAT_RGBA_DEPTH = 4,
	TC_FLOAT16_RGBA = 5,
	TC_FLOAT16_RGBA_DEPTH = 6,
	TC_NUM_RENDER_CLASS = 7
};

enum resolveTarget_t {
	RESOLVE_TARGET_COLOR0 = 0,
	RESOLVE_TARGET_COLOR1 = 1,
	RESOLVE_TARGET_COLOR2 = 2,
	RESOLVE_TARGET_COLOR3 = 3,
	RESOLVE_TARGET_DEPTH = 4
};

class idRenderDestination {
public:
					idRenderDestination();
					idRenderDestination( const char * name, int width, int height,
						renderClass_t textureClass );
					~idRenderDestination();

	void			SetDefault() { isDefault = true; }
	bool			IsDefault() const { return isDefault; }
	bool			IsValid() const { return isDefault || FBOhandle != 0; }
	int				GetTargetWidth() const { return targetWidth; }
	int				GetTargetHeight() const { return targetHeight; }
	idImage *		GetColorImage() const { return targetImage[0]; }
	idImage *		GetTargetImage( int index ) const { return index >= 0 && index < 4 ? targetImage[index] : NULL; }
	idImage *		GetDepthImage() const { return depthImage; }

	void			Create( const char * name, int width, int height, renderClass_t textureClass );
	void			CreateFromImages( idImage * color, idImage * depth, idImage * stencil );
	void			AddTarget( idImage * target );
	void			Resize( int width, int height );

	bool			isDefault;
	int				targetWidth;
	int				targetHeight;
	idImage *		targetImage[4];
	idImage *		depthImage;
	idImage *		stencilImage;
	GLuint			FBOhandle;
	int				boundSide;
	int				boundLevel;
};

extern idRenderDestination * renderDestScaledLinear;
extern idRenderDestination * renderDestScaledColorWithFeedback;
extern idRenderDestination * renderDestScaledFeedback;
extern idRenderDestination * renderDestScaledDepth;
extern idRenderDestination * renderDestScaledColor2;
extern idRenderDestination * renderDestScaledColor;
extern idRenderDestination * renderDestNative;

