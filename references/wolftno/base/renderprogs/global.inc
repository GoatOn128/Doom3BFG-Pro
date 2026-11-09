// Reconstructed from the shared pre-uniform blocks in *.vglsl and *.pglsl.
// Do not add a #version directive: shaders provide it before including this file.

float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }

vec2 saturate( vec2 v ) { return clamp( v, 0.0, 1.0 ); }

vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }

vec4 saturate( vec4 v ) { return clamp( v, 0.0, 1.0 ); }

vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }

float dot4 ( vec4 a, vec4 b ) { return dot( a, b ); }

float dot4 ( vec2 a, vec4 b ) { return dot( vec4( a, 0.0, 1.0 ), b ); }

float dot3 ( vec3 a, vec3 b ) { return dot( a, b ); }

float dot3 ( vec3 a, vec4 b ) { return dot( a, b.xyz ); }

float dot3 ( vec4 a, vec3 b ) { return dot( a.xyz, b ); }

float dot3 ( vec4 a, vec4 b ) { return dot( a.xyz, b.xyz ); }

#ifdef ID_VERTEX_SHADER

vec4 swizzleColor ( vec4 color ) { return color; }

#endif

#ifdef ID_PIXEL_SHADER

void clip( float v ) { if ( v < 0.0 ) { discard; } }

void clip( vec2 v ) { if ( any( lessThan( v, vec2( 0.0 ) ) ) ) { discard; } }

void clip( vec3 v ) { if ( any( lessThan( v, vec3( 0.0 ) ) ) ) { discard; } }

void clip( vec4 v ) { if ( any( lessThan( v, vec4( 0.0 ) ) ) ) { discard; } }

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

vec4 tex3Dlod( sampler3D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }

vec4 texCUBElod( samplerCube sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }

vec4 h4tex2D ( sampler2D image, vec2 texcoord ) { return tex2D( image, texcoord ); }

vec2 screenPosToTexcoord ( vec2 pos, vec4 bias_scale ) { return ( pos * bias_scale.zw + bias_scale.xy ); }

float dot2 ( vec2 a, vec2 b ) { return dot( a, b ); }

vec3 pow3 ( vec3 a, float b ) { return vec3( pow( a.x, b ), pow( a.y, b ), pow( a.z, b ) ); }

vec4 h4texPhys ( sampler2D image, vec2 texcoord ) { return tex2D( image, texcoord ); }

vec4 h4texPhys ( sampler2D image, vec2 texcoord, vec2 dx, vec2 dy ) { return tex2D( image, texcoord, dx, dy ); }

const vec4 matrixCoCg1YtoRGB1X = vec4( 1.0, -1.0, 0.0, 1.0 );

const vec4 matrixCoCg1YtoRGB1Y = vec4( 0.0, 1.0, -0.50196078, 1.0 );

const vec4 matrixCoCg1YtoRGB1Z = vec4( -1.0, -1.0, 1.00392156, 1.0 );

float tex2Ddepth ( sampler2D image, vec2 texcoord ) { return tex2D( image, texcoord ).x; }

float ComputeSoftParticleScale ( sampler2D image, vec2 tc, float interpolatedZOverW, float softness ) {
	float depth = tex2Ddepth( image, tc );
	float r1 = 1.0 / ( 1.0 - depth );
	float r2 = 1.0 / ( 1.0 - interpolatedZOverW );
	return saturate( ( r1 - r2 ) * softness );
}

vec4 tex2DMSFetch ( sampler2DMS image, ivec2 texcoord, int sampleIndex ) { return texelFetch( image, texcoord, sampleIndex ); }

float shadow2Ddepth ( sampler2DShadow image, vec3 texcoord ) { return tex2D( image, texcoord ).x; }

vec3 SafeNormalizeHalf ( vec3 a ) {
	return ( dot( a, a ) < 0.00001 ? vec3( 0.0 ) : normalize( a ) );
}

vec2 SafeNormalizeHalf ( vec2 a ) {
	return ( dot( a, a ) < 0.00001 ? vec2( 0.0 ) : normalize( a ) );
}

float tex2Ddepth_fast ( sampler2D image, vec2 texcoord ) { return tex2D( image, texcoord ).x; }

vec4 tex2DGrad ( sampler2D image, vec2 texcoord, vec2 dx, vec2 dy ) { return textureGrad( image, texcoord, dx, dy ); }

bool ReadFacing ( bool frag_facing ) { return frag_facing; }

vec2 ComputeTransAtlasCoord ( vec2 tc, vec4 scaleBias ) {
	vec2 halfScale = scaleBias.xy * vec2( 0.5 );
	vec2 result = max( min( fract( tc ), vec2( 1.0 ) - halfScale ), halfScale );
	return result * scaleBias.xy + scaleBias.zw;
}

vec4 ComputeDistortion ( vec2 distortVec, float blur ) {
	vec4 final;
	final.z = 0.125 + sqrt( dot( distortVec, distortVec ) );
	final.xy = distortVec / final.z;
	final.w = blur * 0.5;
	return final + ( 127.0 / 255.0 );
}

vec3 SafeNormalize ( vec3 a ) {
	return ( dot( a, a ) < 0.00001 ? vec3( 0.0 ) : normalize( a ) );
}

vec2 SafeNormalize ( vec2 a ) {
	return ( dot( a, a ) < 0.00001 ? vec2( 0.0 ) : normalize( a ) );
}

vec3 ScaleSpecular ( vec3 unscaled, float factor ) {
	return ( unscaled * 8.0 ) / ( factor * 255.0 + 8.0 );
}

vec4 texPhys ( sampler2D image, vec2 texcoord ) { return tex2D( image, texcoord ); }

vec4 texPhys ( sampler2D image, vec2 texcoord, vec2 dx, vec2 dy ) { return tex2D( image, texcoord, dx, dy ); }

vec3 VirtualToPhysical2 ( sampler2D virtToPhysPage, sampler2D physPageToXform0, vec2 virtCoords ) {
	vec2 physPage = tex2D( virtToPhysPage, virtCoords ).xy;
	vec4 xform = tex2D( physPageToXform0, physPage );
	vec3 physCoord;
	physCoord.xy = virtCoords * xform.x + xform.zw;
	physCoord.z = xform.y;
	return physCoord;
}

vec4 pow4 ( vec4 a, float b ) { return vec4( pow( a.x, b ), pow( a.y, b ), pow( a.z, b ), pow( a.w, b ) ); }

vec4 ComputeAnimCrossFadeColor ( sampler2D image, vec2 tc0, vec2 tc1, float frameFrac ) {
	vec4 result0 = tex2D( image, tc0 );
	vec4 result1 = tex2D( image, tc1 );
	return mix( result0, result1, frameFrac );
}

#endif
