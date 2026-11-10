#version 150

void clip( float v ) { if ( v < 0.0 ) { discard; } }
void clip( vec2 v ) { if ( any( lessThan( v, vec2( 0.0 ) ) ) ) { discard; } }
void clip( vec3 v ) { if ( any( lessThan( v, vec3( 0.0 ) ) ) ) { discard; } }
void clip( vec4 v ) { if ( any( lessThan( v, vec4( 0.0 ) ) ) ) { discard; } }

float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }
vec2 saturate( vec2 v ) { return clamp( v, vec2( 0.0 ), vec2( 1.0 ) ); }
vec3 saturate( vec3 v ) { return clamp( v, vec3( 0.0 ), vec3( 1.0 ) ); }
vec4 saturate( vec4 v ) { return clamp( v, vec4( 0.0 ), vec4( 1.0 ) ); }

vec4 tex2D( sampler2D sampler, vec2 texcoord ) { return texture( sampler, texcoord.xy ); }
vec4 tex2D( sampler2DShadow sampler, vec3 texcoord ) { return vec4( texture( sampler, texcoord.xyz ) ); }

vec4 tex2D( sampler2D sampler, vec2 texcoord, vec2 dx, vec2 dy ) { return textureGrad( sampler, texcoord.xy, dx, dy ); }
vec4 tex2D( sampler2DShadow sampler, vec3 texcoord, vec2 dx, vec2 dy ) { return vec4( textureGrad( sampler, texcoord.xyz, dx, dy ) ); }

vec4 texCUBE( samplerCube sampler, vec3 texcoord ) { return texture( sampler, texcoord.xyz ); }
vec4 texCUBE( samplerCubeShadow sampler, vec4 texcoord ) { return vec4( texture( sampler, texcoord.xyzw ) ); }

vec4 tex1Dproj( sampler1D sampler, vec2 texcoord ) { return textureProj( sampler, texcoord ); }
vec4 tex2Dproj( sampler2D sampler, vec3 texcoord ) { return textureProj( sampler, texcoord ); }
vec4 tex3Dproj( sampler3D sampler, vec4 texcoord ) { return textureProj( sampler, texcoord ); }

vec4 tex1Dbias( sampler1D sampler, vec4 texcoord ) { return texture( sampler, texcoord.x, texcoord.w ); }
vec4 tex2Dbias( sampler2D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xy, texcoord.w ); }
vec4 tex3Dbias( sampler3D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }
vec4 texCUBEbias( samplerCube sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }

vec4 tex1Dlod( sampler1D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.x, texcoord.w ); }
vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }
vec4 tex3Dlod( sampler3D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }
vec4 texCUBElod( samplerCube sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }


uniform vec4 rpScreenCorrectionFactor;
uniform vec4 rpWindowCoord;
uniform vec4 rpDiffuseModifier;
uniform vec4 rpSpecularModifier;
uniform vec4 rpLocalLightOrigin;
uniform vec4 rpLocalViewOrigin;
uniform vec4 rpLightProjectionS;
uniform vec4 rpLightProjectionT;
uniform vec4 rpLightProjectionQ;
uniform vec4 rpLightFalloffS;
uniform vec4 rpBumpMatrixS;
uniform vec4 rpBumpMatrixT;
uniform vec4 rpDiffuseMatrixS;
uniform vec4 rpDiffuseMatrixT;
uniform vec4 rpSpecularMatrixS;
uniform vec4 rpSpecularMatrixT;
uniform vec4 rpVertexColorModulate;
uniform vec4 rpVertexColorAdd;
uniform vec4 rpColor;
uniform vec4 rpViewOrigin;
uniform vec4 rpGlobalEyePos;
uniform vec4 rpMVPmatrixX;
uniform vec4 rpMVPmatrixY;
uniform vec4 rpMVPmatrixZ;
uniform vec4 rpMVPmatrixW;
uniform vec4 rpModelMatrixX;
uniform vec4 rpModelMatrixY;
uniform vec4 rpModelMatrixZ;
uniform vec4 rpModelMatrixW;
uniform vec4 rpProjectionMatrixX;
uniform vec4 rpProjectionMatrixY;
uniform vec4 rpProjectionMatrixZ;
uniform vec4 rpProjectionMatrixW;
uniform vec4 rpModelViewMatrixX;
uniform vec4 rpModelViewMatrixY;
uniform vec4 rpModelViewMatrixZ;
uniform vec4 rpModelViewMatrixW;
uniform vec4 rpTextureMatrixS;
uniform vec4 rpTextureMatrixT;
uniform vec4 rpTexGen0S;
uniform vec4 rpTexGen0T;
uniform vec4 rpTexGen0Q;
uniform vec4 rpTexGen0Enabled;
uniform vec4 rpTexGen1S;
uniform vec4 rpTexGen1T;
uniform vec4 rpTexGen1Q;
uniform vec4 rpTexGen1Enabled;
uniform vec4 rpWobbleSkyX;
uniform vec4 rpWobbleSkyY;
uniform vec4 rpWobbleSkyZ;
uniform vec4 rpOverbright;
uniform vec4 rpEnableSkinning;
uniform vec4 rpAlphaTest;
float dot2( vec2 a, vec2 b ) { return dot( a, b ); }
float dot3( vec3 a, vec3 b ) { return dot( a, b ); }
float dot3( vec3 a, vec4 b ) { return dot( a, b.xyz ); }
float dot3( vec4 a, vec3 b ) { return dot( a.xyz, b ); }
float dot3( vec4 a, vec4 b ) { return dot( a.xyz, b.xyz ); }
float dot4( vec4 a, vec4 b ) { return dot( a, b ); }
float dot4( vec2 a, vec4 b ) { return dot( vec4( a, 0, 1 ), b ); }
const vec4 matrixRGB1toCoCg1YX = vec4( 0.50, 0.0, -0.50, 0.50196078 );
const vec4 matrixRGB1toCoCg1YY = vec4( -0.25, 0.5, -0.25, 0.50196078 );
const vec4 matrixRGB1toCoCg1YZ = vec4( 0.0, 0.0, 0.0, 1.0 );
const vec4 matrixRGB1toCoCg1YW = vec4( 0.25, 0.5, 0.25, 0.0 );
const vec4 matrixCoCg1YtoRGB1X = vec4( 1.0, -1.0, 0.0, 1.0 );
const vec4 matrixCoCg1YtoRGB1Y = vec4( 0.0, 1.0, -0.50196078, 1.0 );
const vec4 matrixCoCg1YtoRGB1Z = vec4( -1.0, -1.0, 1.00392156, 1.0 );
vec3 ConvertYCoCgToRGB( vec4 YCoCg ) {
vec3 rgbColor;
YCoCg.z = ( YCoCg.z * 31.875 ) + 1.0;
YCoCg.z = 1.0 / YCoCg.z;
YCoCg.xy *= YCoCg.z;
rgbColor.x = dot4( YCoCg, matrixCoCg1YtoRGB1X );
rgbColor.y = dot4( YCoCg, matrixCoCg1YtoRGB1Y );
rgbColor.z = dot4( YCoCg, matrixCoCg1YtoRGB1Z );
return rgbColor;
}
vec2 CenterScale( vec2 inTC, vec2 centerScale ) {
float scaleX = centerScale.x;
float scaleY = centerScale.y;
vec4 tc0 = vec4( scaleX, 0, 0, 0.5 - ( 0.5 * scaleX ) );
vec4 tc1 = vec4( 0, scaleY, 0, 0.5 - ( 0.5 * scaleY ) );
vec2 finalTC;
finalTC.x = dot4( inTC, tc0 );
finalTC.y = dot4( inTC, tc1 );
return finalTC;
}
vec2 Rotate2D( vec2 inTC, vec2 cs ) {
float sinValue = cs.y;
float cosValue = cs.x;
vec4 tc0 = vec4( cosValue, -sinValue, 0, ( -0.5 * cosValue ) + ( 0.5 * sinValue ) + 0.5 );
vec4 tc1 = vec4( sinValue, cosValue, 0, ( -0.5 * sinValue ) + ( -0.5 * cosValue ) + 0.5 );
vec2 finalTC;
finalTC.x = dot4( inTC, tc0 );
finalTC.y = dot4( inTC, tc1 );
return finalTC;
}
float rand( vec2 co ) {
return fract( sin( dot( co.xy, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
}
#define _half2( x ) vec2( x )
#define _half3( x ) vec3( x )
#define _half4( x ) vec4( x )
#define _float2( x ) vec2( x )
#define _float3( x ) vec3( x )
#define _float4( x ) vec4( x )
#define VPOS WPOS
vec4 idtex2Dproj( sampler2D samp, vec4 texCoords ) { return tex2Dproj( samp, texCoords.xyw ); }
vec4 swizzleColor( vec4 c ) { return c; }
vec2 vposToScreenPosTexCoord( vec2 vpos ) { return vpos.xy * rpWindowCoord.xy; }
#define BRANCH
#define IFANY
uniform sampler2D samp0;
uniform sampler2D samp1;
uniform sampler2D samp2;

in vec2 vofi_TexCoord0;

out vec4 out_FragColor0;

void main() {
{
vec3 crc = vec3( 1.595794678, -0.813476563, 0 );
vec3 crb = vec3( 0, -0.391448975, 2.017822266 );
vec3 adj = vec3( -0.87065506, 0.529705048, -1.081668854 );
vec3 YScalar = vec3( 1.164123535, 1.164123535, 1.164123535 );
float Y = tex2D( samp0, vofi_TexCoord0.xy ).x;
float Cr = tex2D( samp1, vofi_TexCoord0.xy ).x;
float Cb = tex2D( samp2, vofi_TexCoord0.xy ).x;
vec3 p = ( YScalar * Y );
p += ( crc * Cr ) + ( crb * Cb ) + adj;
out_FragColor0.xyz = p;
out_FragColor0.w = 1.0;
out_FragColor0 *= rpColor;
}
}