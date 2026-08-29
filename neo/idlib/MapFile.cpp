/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").  

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "precompiled.h"
#pragma hdrstop


/*
===============
FloatCRC
===============
*/
ID_INLINE unsigned int FloatCRC( float f ) {
	return *(unsigned int *)&f;
}

/*
===============
StringCRC
===============
*/
ID_INLINE unsigned int StringCRC( const char *str ) {
	unsigned int i, crc;
	const unsigned char *ptr;

	crc = 0;
	ptr = reinterpret_cast<const unsigned char*>(str);
	for ( i = 0; str[i]; i++ ) {
		crc ^= str[i] << (i & 3);
	}
	return crc;
}

static bool MapStringListContains( const idList<idStr> &list, const char *value ) {
	for ( int i = 0; i < list.Num(); ++i ) {
		if ( idStr::Icmp( list[i], value ) == 0 ) {
			return true;
		}
	}
	return false;
}

static bool ParseMapStringBlock( idLexer &src, idList<idStr> &values ) {
	if ( !src.ExpectTokenString( "{" ) ) {
		return false;
	}
	idToken token;
	while ( src.ReadToken( &token ) ) {
		if ( token == "}" ) {
			return true;
		}
		if ( token.type == TT_STRING && !MapStringListContains( values, token ) ) {
			values.Append( token );
		}
	}
	return false;
}

static bool ParseMapStateBlock( idLexer &src, idList<idMapEditorState> &states, bool colors ) {
	if ( !src.ExpectTokenString( "{" ) ) {
		return false;
	}
	idToken token;
	while ( src.ReadToken( &token ) ) {
		if ( token == "}" ) {
			return true;
		}
		if ( token.type != TT_STRING ) {
			return false;
		}
		idMapEditorState state;
		state.name = token;
		if ( colors ) {
			unsigned int packed = 0;
			for ( int channel = 0; channel < 4; ++channel ) {
				int value = static_cast<int>( src.ParseFloat() * 255.0f + 0.5f );
				value = value < 0 ? 0 : ( value > 255 ? 255 : value );
				packed |= static_cast<unsigned int>( value ) << ( channel * 8 );
			}
			state.state = packed;
		} else {
			state.state = static_cast<unsigned int>( src.ParseInt() );
		}
		states.Append( state );
	}
	return false;
}

void idMapPrimitive::AddGroup( const char *name ) {
	if ( name != NULL && name[0] != '\0' && !MapStringListContains( groups, name ) ) {
		groups.Append( name );
	}
}

void idMapPrimitive::WriteGroups( idFile *fp, const char *indent ) const {
	if ( fp == NULL || groups.Num() == 0 ) {
		return;
	}
	for ( int i = 0; i < groups.Num(); ++i ) {
		if ( i == 0 ) {
			fp->WriteFloatString( "%sgroups {\n", indent );
		}
		fp->WriteFloatString( "%s \"%s\"\n", indent, groups[i].c_str() );
	}
	fp->WriteFloatString( "%s}\n", indent );
}

idMapReference *idMapReference::Parse( idLexer &src ) {
	if ( !src.ExpectTokenString( "{" ) ) {
		return NULL;
	}
	idToken token;
	if ( !src.ExpectTokenType( TT_STRING, 0, &token ) ) {
		return NULL;
	}
	idMapReference *reference = new (TAG_IDLIB) idMapReference;
	reference->SetMapName( token );
	if ( !src.ExpectTokenString( "}" ) || !src.ExpectTokenString( "}" ) ) {
		delete reference;
		return NULL;
	}
	return reference;
}

bool idMapReference::Write( idFile *fp, int primitiveNum ) const {
	if ( fp == NULL || mapName.IsEmpty() ) {
		return false;
	}
	fp->WriteFloatString( "// reference %d\n{\n", primitiveNum );
	WriteGroups( fp, " " );
	fp->WriteFloatString( " reference {\n  \"%s\"\n }\n}\n", mapName.c_str() );
	return true;
}

unsigned int idMapReference::GetGeometryCRC() const {
	return StringCRC( mapName );
}

/*
=================
ComputeAxisBase

WARNING : special case behaviour of atan2(y,x) <-> atan(y/x) might not be the same everywhere when x == 0
rotation by (0,RotY,RotZ) assigns X to normal
=================
*/
static void ComputeAxisBase( const idVec3 &normal, idVec3 &texS, idVec3 &texT ) {
	float RotY, RotZ;
	idVec3 n;

	// do some cleaning
	n[0] = ( idMath::Fabs( normal[0] ) < 1e-6f ) ? 0.0f : normal[0];
	n[1] = ( idMath::Fabs( normal[1] ) < 1e-6f ) ? 0.0f : normal[1];
	n[2] = ( idMath::Fabs( normal[2] ) < 1e-6f ) ? 0.0f : normal[2];

	RotY = -atan2( n[2], idMath::Sqrt( n[1] * n[1] + n[0] * n[0]) );
	RotZ = atan2( n[1], n[0] );
	// rotate (0,1,0) and (0,0,1) to compute texS and texT
	texS[0] = -sin(RotZ);
	texS[1] = cos(RotZ);
	texS[2] = 0;
	// the texT vector is along -Z ( T texture coorinates axis )
	texT[0] = -sin(RotY) * cos(RotZ);
	texT[1] = -sin(RotY) * sin(RotZ);
	texT[2] = -cos(RotY);
}

/*
=================
idMapBrushSide::GetTextureVectors
=================
*/
void idMapBrushSide::GetTextureVectors( idVec4 v[2] ) const {
	int i;
	idVec3 texX, texY;

	ComputeAxisBase( plane.Normal(), texX, texY );
	for ( i = 0; i < 2; i++ ) {
		v[i][0] = texX[0] * texMat[i][0] + texY[0] * texMat[i][1];
		v[i][1] = texX[1] * texMat[i][0] + texY[1] * texMat[i][1];
		v[i][2] = texX[2] * texMat[i][0] + texY[2] * texMat[i][1];
		v[i][3] = texMat[i][2] + ( origin * v[i].ToVec3() );
	}
}

/*
=================
idMapPatch::Parse
=================
*/
idMapPatch *idMapPatch::Parse( idLexer &src, const idVec3 &origin, bool patchDef3, float version ) {
	float		info[7];
	idDrawVert *vert;
	idToken		token;
	int			i, j;

	if ( !src.ExpectTokenString( "{" ) ) {
		return NULL;
	}

	// read the material (we had an implicit 'textures/' in the old format...)
	if ( !src.ReadToken( &token ) ) {
		src.Error( "idMapPatch::Parse: unexpected EOF" );
		return NULL;
	}

	// Parse it
	if (patchDef3) {
		if ( !src.Parse1DMatrix( 7, info ) ) {
			src.Error( "idMapPatch::Parse: unable to Parse patchDef3 info" );
			return NULL;
		}
	} else {
		if ( !src.Parse1DMatrix( 5, info ) ) {
			src.Error( "idMapPatch::Parse: unable to parse patchDef2 info" );
			return NULL;
		}
	}

	idMapPatch *patch = new (TAG_IDLIB) idMapPatch( info[0], info[1] );

	patch->SetSize( info[0], info[1] );
	if ( version < 2.0f ) {
		patch->SetMaterial( "textures/" + token );
	} else {
		patch->SetMaterial( token );
	}

	if ( patchDef3 ) {
		patch->SetHorzSubdivisions( info[2] );
		patch->SetVertSubdivisions( info[3] );
		patch->SetExplicitlySubdivided( true );
	}

	if ( patch->GetWidth() < 0 || patch->GetHeight() < 0 ) {
		src.Error( "idMapPatch::Parse: bad size" );
		delete patch;
		return NULL;
	}

	// these were written out in the wrong order, IMHO
	if ( !src.ExpectTokenString( "(" ) ) {
		src.Error( "idMapPatch::Parse: bad patch vertex data" );
		delete patch;
		return NULL;
	}


	for ( j = 0; j < patch->GetWidth(); j++ ) {
		if ( !src.ExpectTokenString( "(" ) ) {
			src.Error( "idMapPatch::Parse: bad vertex row data" );
			delete patch;
			return NULL;
		}
		for ( i = 0; i < patch->GetHeight(); i++ ) {
			float v[5];

			if ( !src.Parse1DMatrix( 5, v ) ) {
				src.Error( "idMapPatch::Parse: bad vertex column data" );
				delete patch;
				return NULL;
			}

			vert = &((*patch)[i * patch->GetWidth() + j]);
			vert->xyz[0] = v[0] - origin[0];
			vert->xyz[1] = v[1] - origin[1];
			vert->xyz[2] = v[2] - origin[2];
			vert->SetTexCoord( v[3], v[4] );
		}
		if ( !src.ExpectTokenString( ")" ) ) {
			delete patch;
			src.Error( "idMapPatch::Parse: unable to parse patch control points" );
			return NULL;
		}
	}

	if ( !src.ExpectTokenString( ")" ) ) {
		src.Error( "idMapPatch::Parse: unable to parse patch control points, no closure" );
		delete patch;
		return NULL;
	}

	// read any key/value pairs
	while( src.ReadToken( &token ) ) {
		if ( token == "}" ) {
			src.ExpectTokenString( "}" );
			break;
		}
		if ( token.type == TT_STRING ) {
			idStr key = token;
			src.ExpectTokenType( TT_STRING, 0, &token );
			patch->epairs.Set( key, token );
		}
	}

	return patch;
}

/*
============
idMapPatch::Write
============
*/
bool idMapPatch::Write( idFile *fp, int primitiveNum, const idVec3 &origin ) const {
	int i, j;
	const idDrawVert *v;

	fp->WriteFloatString( "// primitive %d\n{\n", primitiveNum );
	WriteGroups( fp, " " );
	if ( GetExplicitlySubdivided() ) {
		fp->WriteFloatString( " patchDef3\n {\n" );
		fp->WriteFloatString( "  \"%s\"\n  ( %d %d %d %d 0 0 0 )\n", GetMaterial(), GetWidth(), GetHeight(), GetHorzSubdivisions(), GetVertSubdivisions());
	} else {
		fp->WriteFloatString( " patchDef2\n {\n" );
		fp->WriteFloatString( "  \"%s\"\n  ( %d %d 0 0 0 )\n", GetMaterial(), GetWidth(), GetHeight());
	}

	fp->WriteFloatString( "  (\n" );
	idVec2 st;
	for ( i = 0; i < GetWidth(); i++ ) {
		fp->WriteFloatString( "   ( " );
		for ( j = 0; j < GetHeight(); j++ ) {
			v = &verts[ j * GetWidth() + i ];
			st = v->GetTexCoord();
			fp->WriteFloatString( " ( %f %f %f %f %f )", v->xyz[0] + origin[0],
								v->xyz[1] + origin[1], v->xyz[2] + origin[2], st[0], st[1] );
		}
		fp->WriteFloatString( " )\n" );
	}
	fp->WriteFloatString( "  )\n }\n}\n" );

	return true;
}

/*
===============
idMapPatch::GetGeometryCRC
===============
*/
unsigned int idMapPatch::GetGeometryCRC() const {
	int i, j;
	unsigned int crc;

	crc = GetHorzSubdivisions() ^ GetVertSubdivisions();
	for ( i = 0; i < GetWidth(); i++ ) {
		for ( j = 0; j < GetHeight(); j++ ) {
			crc ^= FloatCRC( verts[j * GetWidth() + i].xyz.x );
			crc ^= FloatCRC( verts[j * GetWidth() + i].xyz.y );
			crc ^= FloatCRC( verts[j * GetWidth() + i].xyz.z );
		}
	}

	crc ^= StringCRC( GetMaterial() );

	return crc;
}

/*
=================
idMapBrush::Parse
=================
*/
idMapBrush *idMapBrush::Parse( idLexer &src, const idVec3 &origin, bool newFormat, float version ) {
	int i;
	idVec3 planepts[3];
	idToken token;
	idList<idMapBrushSide*> sides;
	idMapBrushSide	*side;
	idDict epairs;

	if ( !src.ExpectTokenString( "{" ) ) {
		return NULL;
	}

	do {
		if ( !src.ReadToken( &token ) ) {
			src.Error( "idMapBrush::Parse: unexpected EOF" );
			sides.DeleteContents( true );
			return NULL;
		}
		if ( token == "}" ) {
			break;
		}

		// here we may have to jump over brush epairs ( only used in editor )
		do {
			// if token is a brace
			if ( token == "(" ) {
				break;
			}
			// the token should be a key string for a key/value pair
			if ( token.type != TT_STRING ) {
				src.Error( "idMapBrush::Parse: unexpected %s, expected ( or epair key string", token.c_str() );
				sides.DeleteContents( true );
				return NULL;
			}

			idStr key = token;

			if ( !src.ReadTokenOnLine( &token ) || token.type != TT_STRING ) {
				src.Error( "idMapBrush::Parse: expected epair value string not found" );
				sides.DeleteContents( true );
				return NULL;
			}

			epairs.Set( key, token );

			// try to read the next key
			if ( !src.ReadToken( &token ) ) {
				src.Error( "idMapBrush::Parse: unexpected EOF" );
				sides.DeleteContents( true );
				return NULL;
			}
		} while (1);

		src.UnreadToken( &token );

		side = new (TAG_IDLIB) idMapBrushSide();
		sides.Append(side);

		if ( newFormat ) {
			if ( !src.Parse1DMatrix( 4, side->plane.ToFloatPtr() ) ) {
				src.Error( "idMapBrush::Parse: unable to read brush side plane definition" );
				sides.DeleteContents( true );
				return NULL;
			}
		} else {
			// read the three point plane definition
			if (!src.Parse1DMatrix( 3, planepts[0].ToFloatPtr() ) ||
				!src.Parse1DMatrix( 3, planepts[1].ToFloatPtr() ) ||
				!src.Parse1DMatrix( 3, planepts[2].ToFloatPtr() ) ) {
				src.Error( "idMapBrush::Parse: unable to read brush side plane definition" );
				sides.DeleteContents( true );
				return NULL;
			}

			planepts[0] -= origin;
			planepts[1] -= origin;
			planepts[2] -= origin;

			side->plane.FromPoints( planepts[0], planepts[1], planepts[2] );
		}

		// read the texture matrix
		// this is odd, because the texmat is 2D relative to default planar texture axis
		if ( !src.Parse2DMatrix( 2, 3, side->texMat[0].ToFloatPtr() ) ) {
			src.Error( "idMapBrush::Parse: unable to read brush side texture matrix" );
			sides.DeleteContents( true );
			return NULL;
		}
		side->origin = origin;
		
		// read the material
		if ( !src.ReadTokenOnLine( &token ) ) {
			src.Error( "idMapBrush::Parse: unable to read brush side material" );
			sides.DeleteContents( true );
			return NULL;
		}

		// we had an implicit 'textures/' in the old format...
		if ( version < 2.0f ) {
			side->material = "textures/" + token;
		} else {
			side->material = token;
		}

		// Q2 allowed override of default flags and values, but we don't any more
		if ( src.ReadTokenOnLine( &token ) ) {
			if ( src.ReadTokenOnLine( &token ) ) {
				if ( src.ReadTokenOnLine( &token ) ) {
				}
			}
		}
	} while( 1 );

	if ( !src.ExpectTokenString( "}" ) ) {
		sides.DeleteContents( true );
		return NULL;
	}

	idMapBrush *brush = new (TAG_IDLIB) idMapBrush();
	for ( i = 0; i < sides.Num(); i++ ) {
		brush->AddSide( sides[i] );
	}

	brush->epairs = epairs;

	return brush;
}

/*
=================
idMapBrush::ParseQ3
=================
*/
idMapBrush *idMapBrush::ParseQ3( idLexer &src, const idVec3 &origin ) {
	int i, shift[2], rotate;
	float scale[2];
	idVec3 planepts[3];
	idToken token;
	idList<idMapBrushSide*> sides;
	idMapBrushSide	*side;
	idDict epairs;

	do {
		if ( src.CheckTokenString( "}" ) ) {
			break;
		}

		side = new (TAG_IDLIB) idMapBrushSide();
		sides.Append( side );

		// read the three point plane definition
		if (!src.Parse1DMatrix( 3, planepts[0].ToFloatPtr() ) ||
			!src.Parse1DMatrix( 3, planepts[1].ToFloatPtr() ) ||
			!src.Parse1DMatrix( 3, planepts[2].ToFloatPtr() ) ) {
			src.Error( "idMapBrush::ParseQ3: unable to read brush side plane definition" );
			sides.DeleteContents( true );
			return NULL;
		}

		planepts[0] -= origin;
		planepts[1] -= origin;
		planepts[2] -= origin;

		side->plane.FromPoints( planepts[0], planepts[1], planepts[2] );

		// read the material
		if ( !src.ReadTokenOnLine( &token ) ) {
			src.Error( "idMapBrush::ParseQ3: unable to read brush side material" );
			sides.DeleteContents( true );
			return NULL;
		}

		// we have an implicit 'textures/' in the old format
		side->material = "textures/" + token;

		// read the texture shift, rotate and scale
		shift[0] = src.ParseInt();
		shift[1] = src.ParseInt();
		rotate = src.ParseInt();
		scale[0] = src.ParseFloat();
		scale[1] = src.ParseFloat();
		side->texMat[0] = idVec3( 0.03125f, 0.0f, 0.0f );
		side->texMat[1] = idVec3( 0.0f, 0.03125f, 0.0f );
		side->origin = origin;
		
		// Q2 allowed override of default flags and values, but we don't any more
		if ( src.ReadTokenOnLine( &token ) ) {
			if ( src.ReadTokenOnLine( &token ) ) {
				if ( src.ReadTokenOnLine( &token ) ) {
				}
			}
		}
	} while( 1 );

	idMapBrush *brush = new (TAG_IDLIB) idMapBrush();
	for ( i = 0; i < sides.Num(); i++ ) {
		brush->AddSide( sides[i] );
	}

	brush->epairs = epairs;

	return brush;
}

/*
============
idMapBrush::Write
============
*/
bool idMapBrush::Write( idFile *fp, int primitiveNum, const idVec3 &origin ) const {
	int i;
	idMapBrushSide *side;

	fp->WriteFloatString( "// primitive %d\n{\n", primitiveNum );
	WriteGroups( fp, " " );
	fp->WriteFloatString( " brushDef3\n {\n" );

	// write brush epairs
	for ( i = 0; i < epairs.GetNumKeyVals(); i++) {
		fp->WriteFloatString( "  \"%s\" \"%s\"\n", epairs.GetKeyVal(i)->GetKey().c_str(), epairs.GetKeyVal(i)->GetValue().c_str());
	}

	// write brush sides
	for ( i = 0; i < GetNumSides(); i++ ) {
		side = GetSide( i );
		fp->WriteFloatString( "  ( %f %f %f %f ) ", side->plane[0], side->plane[1], side->plane[2], side->plane[3] );
		fp->WriteFloatString( "( ( %f %f %f ) ( %f %f %f ) ) \"%s\" 0 0 0\n",
							side->texMat[0][0], side->texMat[0][1], side->texMat[0][2],
								side->texMat[1][0], side->texMat[1][1], side->texMat[1][2],
									side->material.c_str() );
	}

	fp->WriteFloatString( " }\n}\n" );

	return true;
}

/*
===============
idMapBrush::GetGeometryCRC
===============
*/
unsigned int idMapBrush::GetGeometryCRC() const {
	int i, j;
	idMapBrushSide *mapSide;
	unsigned int crc;

	crc = 0;
	for ( i = 0; i < GetNumSides(); i++ ) {
		mapSide = GetSide(i);
		for ( j = 0; j < 4; j++ ) {
			crc ^= FloatCRC( mapSide->GetPlane()[j] );
		}
		crc ^= StringCRC( mapSide->GetMaterial() );
	}

	return crc;
}

/*
================
idMapEntity::Parse
================
*/
idMapEntity *idMapEntity::Parse( idLexer &src, bool worldSpawn, float version ) {
	idToken	token;
	idMapEntity *mapEnt;
	idMapPatch *mapPatch;
	idMapBrush *mapBrush;
	idMapReference *mapReference;
	bool worldent;
	idVec3 origin;
	double v1, v2, v3;

	if ( !src.ReadToken(&token) ) {
		return NULL;
	}

	if ( token != "{" ) {
		src.Error( "idMapEntity::Parse: { not found, found %s", token.c_str() );
		return NULL;
	}

	mapEnt = new (TAG_IDLIB) idMapEntity();

	if ( worldSpawn ) {
		mapEnt->primitives.Resize( 1024, 256 );
	}

	origin.Zero();
	worldent = false;
	do {
		if ( !src.ReadToken(&token) ) {
			src.Error( "idMapEntity::Parse: EOF without closing brace" );
			return NULL;
		}
		if ( token == "}" ) {
			break;
		}

		if ( token == "{" ) {
			// parse a brush or patch
			if ( !src.ReadToken( &token ) ) {
				src.Error( "idMapEntity::Parse: unexpected EOF" );
				return NULL;
			}

			idList<idStr> primitiveGroups;
			if ( version >= IDSTUDIO_MAP_VERSION && token == "groups" ) {
				if ( !ParseMapStringBlock( src, primitiveGroups ) || !src.ReadToken( &token ) ) {
					delete mapEnt;
					return NULL;
				}
			}

			if ( worldent ) {
				origin.Zero();
			}

			// if is it a brush: brush, brushDef, brushDef2, brushDef3
			if ( token.Icmpn( "brush", 5 ) == 0 ) {
				mapBrush = idMapBrush::Parse( src, origin, ( !token.Icmp( "brushDef2" ) || !token.Icmp( "brushDef3" ) ), version );
				if ( !mapBrush ) {
					return NULL;
				}
				for ( int groupIndex = 0; groupIndex < primitiveGroups.Num(); ++groupIndex ) {
					mapBrush->AddGroup( primitiveGroups[groupIndex] );
				}
				mapEnt->AddPrimitive( mapBrush );
			}
			// if is it a patch: patchDef2, patchDef3
			else if ( token.Icmpn( "patch", 5 ) == 0 ) {
				mapPatch = idMapPatch::Parse( src, origin, !token.Icmp( "patchDef3" ), version );
				if ( !mapPatch ) {
					return NULL;
				}
				for ( int groupIndex = 0; groupIndex < primitiveGroups.Num(); ++groupIndex ) {
					mapPatch->AddGroup( primitiveGroups[groupIndex] );
				}
				mapEnt->AddPrimitive( mapPatch );
			}
			else if ( version >= IDSTUDIO_MAP_VERSION && !token.Icmp( "reference" ) ) {
				mapReference = idMapReference::Parse( src );
				if ( !mapReference ) {
					delete mapEnt;
					return NULL;
				}
				for ( int groupIndex = 0; groupIndex < primitiveGroups.Num(); ++groupIndex ) {
					mapReference->AddGroup( primitiveGroups[groupIndex] );
				}
				mapEnt->AddPrimitive( mapReference );
			}
			// assume it's a brush in Q3 or older style
			else {
				src.UnreadToken( &token );
				mapBrush = idMapBrush::ParseQ3( src, origin );
				if ( !mapBrush ) {
					return NULL;
				}
				mapEnt->AddPrimitive( mapBrush );
			}
		} else if ( version >= IDSTUDIO_MAP_VERSION && token == "layers" ) {
			if ( !ParseMapStringBlock( src, mapEnt->layers ) ) {
				delete mapEnt;
				return NULL;
			}
		} else if ( version >= IDSTUDIO_MAP_VERSION && token == "groups" ) {
			if ( !ParseMapStringBlock( src, mapEnt->groups ) ) {
				delete mapEnt;
				return NULL;
			}
		} else {
			idStr key, value;

			// parse a key / value pair
			key = token;
			src.ReadTokenOnLine( &token );
			value = token;

			// strip trailing spaces that sometimes get accidentally
			// added in the editor
			value.StripTrailingWhitespace();
			key.StripTrailingWhitespace();

			mapEnt->epairs.Set( key, value );

			if ( !idStr::Icmp( key, "origin" ) ) {
				// scanf into doubles, then assign, so it is idVec size independent
				v1 = v2 = v3 = 0;
				sscanf( value, "%lf %lf %lf", &v1, &v2, &v3 );
				origin.x = v1;
				origin.y = v2;
				origin.z = v3;
			}
			else if ( !idStr::Icmp( key, "classname" ) && !idStr::Icmp( value, "worldspawn" ) ) {
				worldent = true;
			}
		}
	} while( 1 );

	return mapEnt;
}

/*
============
idMapEntity::Write
============
*/
bool idMapEntity::Write( idFile *fp, int entityNum ) const {
	int i;
	idMapPrimitive *mapPrim;
	idVec3 origin;

	fp->WriteFloatString( "// entity %d\n{\n", entityNum );
	if ( groups.Num() > 0 ) {
		fp->WriteFloatString( "groups {\n" );
		for ( i = 0; i < groups.Num(); ++i ) {
			fp->WriteFloatString( " \"%s\"\n", groups[i].c_str() );
		}
		fp->WriteFloatString( "}\n" );
	}
	if ( layers.Num() > 0 ) {
		fp->WriteFloatString( "layers {\n" );
		for ( i = 0; i < layers.Num(); ++i ) {
			fp->WriteFloatString( " \"%s\"\n", layers[i].c_str() );
		}
		fp->WriteFloatString( "}\n" );
	}

	// write entity epairs
	for ( i = 0; i < epairs.GetNumKeyVals(); i++) {
		fp->WriteFloatString( "\"%s\" \"%s\"\n", epairs.GetKeyVal(i)->GetKey().c_str(), epairs.GetKeyVal(i)->GetValue().c_str());
	}

	epairs.GetVector( "origin", "0 0 0", origin );

	// write pritimives
	for ( i = 0; i < GetNumPrimitives(); i++ ) {
		mapPrim = GetPrimitive( i );

		switch( mapPrim->GetType() ) {
			case idMapPrimitive::TYPE_BRUSH:
				static_cast<idMapBrush*>(mapPrim)->Write( fp, i, origin );
				break;
			case idMapPrimitive::TYPE_PATCH:
				static_cast<idMapPatch*>(mapPrim)->Write( fp, i, origin );
				break;
			case idMapPrimitive::TYPE_REFERENCE:
				static_cast<idMapReference*>(mapPrim)->Write( fp, i );
				break;
		}
	}

	fp->WriteFloatString( "}\n" );

	return true;
}

/*
===============
idMapEntity::RemovePrimitiveData
===============
*/
void idMapEntity::RemovePrimitiveData() {
	primitives.DeleteContents(true);
}

/*
===============
idMapEntity::GetGeometryCRC
===============
*/
unsigned int idMapEntity::GetGeometryCRC() const {
	int i;
	unsigned int crc;
	idMapPrimitive	*mapPrim;

	crc = 0;
	for ( i = 0; i < GetNumPrimitives(); i++ ) {
		mapPrim = GetPrimitive( i );

		switch( mapPrim->GetType() ) {
			case idMapPrimitive::TYPE_BRUSH:
				crc ^= static_cast<idMapBrush*>(mapPrim)->GetGeometryCRC();
				break;
			case idMapPrimitive::TYPE_PATCH:
				crc ^= static_cast<idMapPatch*>(mapPrim)->GetGeometryCRC();
				break;
			case idMapPrimitive::TYPE_REFERENCE:
				crc ^= static_cast<idMapReference*>(mapPrim)->GetGeometryCRC();
				break;
		}
	}

	return crc;
}

/*
===============
idMapFile::Parse
===============
*/
bool idMapFile::Parse( const char *filename, bool ignoreRegion, bool osPath ) {
	// no string concatenation for epairs and allow path names for materials
	idLexer src( LEXFL_NOSTRINGCONCAT | LEXFL_NOSTRINGESCAPECHARS | LEXFL_ALLOWPATHNAMES );
	idToken token;
	idStr fullName;
	idStr sourceExtension;
	idMapEntity *mapEnt;
	int i, j, k;

	name = filename;
	name.ExtractFileExtension( sourceExtension );
	name.StripFileExtension();
	fullName = name;
	hasPrimitiveData = false;

	if ( !sourceExtension.Icmp( "refmap" ) ) {
		fullName = filename;
		src.LoadFile( fullName, osPath );
	} else if ( !ignoreRegion ) {
		// try loading a .reg file first
		fullName.SetFileExtension( "reg" );
		src.LoadFile( fullName, osPath );
	}

	if ( !src.IsLoaded() ) {
		// now try a .map file
		fullName.SetFileExtension( "map" );
		src.LoadFile( fullName, osPath );
		if ( !src.IsLoaded() ) {
			// didn't get anything at all
			return false;
		}
	}

	version = OLD_MAP_VERSION;
	fileTime = src.GetFileTime();
	entities.DeleteContents( true );
	groupStates.Clear();
	layerStates.Clear();
	groupColors.Clear();

	if ( src.CheckTokenString( "Version" ) ) {
		src.ReadTokenOnLine( &token );
		version = token.GetFloatValue();
	}

	while( 1 ) {
		if ( version >= IDSTUDIO_MAP_VERSION ) {
			idList<idMapEditorState> *states = NULL;
			bool colors = false;
			if ( src.CheckTokenString( "groupstates" ) ) {
				states = &groupStates;
			} else if ( src.CheckTokenString( "layerstates" ) ) {
				states = &layerStates;
			} else if ( src.CheckTokenString( "groupcolors" ) ) {
				states = &groupColors;
				colors = true;
			}
			if ( states != NULL ) {
				if ( !ParseMapStateBlock( src, *states, colors ) ) {
					return false;
				}
				continue;
			}
		}
		if ( version >= IDSTUDIO_MAP_VERSION && src.CheckTokenString( "entity" ) ) {
			if ( !src.ReadToken( &token ) || token.type != TT_NUMBER ) {
				src.Error( "idMapFile::Parse: expected entity number" );
				return false;
			}
		}
		mapEnt = idMapEntity::Parse( src, ( entities.Num() == 0 ), version );
		if ( !mapEnt ) {
			break;
		}
		entities.Append( mapEnt );
	}

	SetGeometryCRC();

	// if the map has a worldspawn
	if ( entities.Num() ) {

		// "removeEntities" "classname" can be set in the worldspawn to remove all entities with the given classname
		const idKeyValue *removeEntities = entities[0]->epairs.MatchPrefix( "removeEntities", NULL );
		while ( removeEntities ) {
			RemoveEntities( removeEntities->GetValue() );
			removeEntities = entities[0]->epairs.MatchPrefix( "removeEntities", removeEntities );
		}

		// "overrideMaterial" "material" can be set in the worldspawn to reset all materials
		idStr material;
		if ( entities[0]->epairs.GetString( "overrideMaterial", "", material ) ) {
			for ( i = 0; i < entities.Num(); i++ ) {
				mapEnt = entities[i];
				for ( j = 0; j < mapEnt->GetNumPrimitives(); j++ ) {
					idMapPrimitive *mapPrimitive = mapEnt->GetPrimitive( j );
					switch( mapPrimitive->GetType() ) {
						case idMapPrimitive::TYPE_BRUSH: {
							idMapBrush *mapBrush = static_cast<idMapBrush *>(mapPrimitive);
							for ( k = 0; k < mapBrush->GetNumSides(); k++ ) {
								mapBrush->GetSide( k )->SetMaterial( material );
							}
							break;
						}
						case idMapPrimitive::TYPE_PATCH: {
							static_cast<idMapPatch *>(mapPrimitive)->SetMaterial( material );
							break;
						}
						case idMapPrimitive::TYPE_REFERENCE:
							break;
					}
				}
			}
		}

		// force all entities to have a name key/value pair
		if ( entities[0]->epairs.GetBool( "forceEntityNames" ) ) {
			for ( i = 1; i < entities.Num(); i++ ) {
				mapEnt = entities[i];
				if ( !mapEnt->epairs.FindKey( "name" ) ) {
					mapEnt->epairs.Set( "name", va( "%s%d", mapEnt->epairs.GetString( "classname", "forcedName" ), i ) );
				}
			}
		}

		// move the primitives of any func_group entities to the worldspawn
		if ( entities[0]->epairs.GetBool( "moveFuncGroups" ) ) {
			for ( i = 1; i < entities.Num(); i++ ) {
				mapEnt = entities[i];
				if ( idStr::Icmp( mapEnt->epairs.GetString( "classname" ), "func_group" ) == 0 ) {
					entities[0]->primitives.Append( mapEnt->primitives );
					mapEnt->primitives.Clear();
				}
			}
		}
	}

	hasPrimitiveData = true;
	return true;
}

/*
============
idMapFile::Write
============
*/
bool idMapFile::Write( const char *fileName, const char *ext, bool fromBasePath ) {
	int i;
	idStr qpath;
	idFile *fp;

	qpath = fileName;
	qpath.SetFileExtension( ext );

	idLib::common->Printf( "writing %s...\n", qpath.c_str() );

	if ( fromBasePath ) {
		fp = idLib::fileSystem->OpenFileWrite( qpath, "fs_basepath" );
	}
	else {
		fp = idLib::fileSystem->OpenExplicitFileWrite( qpath );
	}

	if ( !fp ) {
		idLib::common->Warning( "Couldn't open %s\n", qpath.c_str() );
		return false;
	}

	const bool idStudioFormat = UsesIdStudioFormat();
	fp->WriteFloatString( "Version %f\n", idStudioFormat ? (float) IDSTUDIO_MAP_VERSION : (float) CURRENT_MAP_VERSION );

	for ( i = 0; i < entities.Num(); i++ ) {
		entities[i]->Write( fp, i );
	}
	if ( idStudioFormat ) {
		WriteStateBlock( fp, "groupstates", groupStates, false );
		WriteStateBlock( fp, "layerstates", layerStates, false );
		WriteStateBlock( fp, "groupcolors", groupColors, true );
	}

	idLib::fileSystem->CloseFile( fp );

	return true;
}

bool idMapFile::UsesIdStudioFormat() const {
	if ( groupStates.Num() > 0 || layerStates.Num() > 0 || groupColors.Num() > 0 ) {
		return true;
	}
	for ( int entityIndex = 0; entityIndex < entities.Num(); ++entityIndex ) {
		const idMapEntity *entity = entities[entityIndex];
		if ( entity->groups.Num() > 0 || entity->layers.Num() > 0 ) {
			return true;
		}
		for ( int primitiveIndex = 0; primitiveIndex < entity->GetNumPrimitives(); ++primitiveIndex ) {
			const idMapPrimitive *primitive = entity->GetPrimitive( primitiveIndex );
			if ( primitive->GetType() == idMapPrimitive::TYPE_REFERENCE || primitive->groups.Num() > 0 ) {
				return true;
			}
		}
	}
	return false;
}

void idMapFile::WriteStateBlock( idFile *fp, const char *name, const idList<idMapEditorState> &states, bool colors ) {
	if ( fp == NULL || states.Num() == 0 ) {
		return;
	}
	fp->WriteFloatString( "%s {\n", name );
	for ( int i = 0; i < states.Num(); ++i ) {
		if ( colors ) {
			const unsigned int value = states[i].state;
			fp->WriteFloatString( " \"%s\" %f %f %f %f\n", states[i].name.c_str(),
				( ( value >> 0 ) & 255 ) / 255.0f, ( ( value >> 8 ) & 255 ) / 255.0f,
				( ( value >> 16 ) & 255 ) / 255.0f, ( ( value >> 24 ) & 255 ) / 255.0f );
		} else {
			fp->WriteFloatString( " \"%s\" %u\n", states[i].name.c_str(), states[i].state );
		}
	}
	fp->WriteFloatString( "}\n" );
}

/*
===============
idMapFile::SetGeometryCRC
===============
*/
void idMapFile::SetGeometryCRC() {
	int i;

	geometryCRC = 0;
	for ( i = 0; i < entities.Num(); i++ ) {
		geometryCRC ^= entities[i]->GetGeometryCRC();
	}
}

/*
===============
idMapFile::AddEntity
===============
*/
int idMapFile::AddEntity( idMapEntity *mapEnt ) {
	int ret = entities.Append( mapEnt );
	return ret;
}

/*
===============
idMapFile::FindEntity
===============
*/
idMapEntity *idMapFile::FindEntity( const char *name ) {
	for ( int i = 0; i < entities.Num(); i++ ) {
		idMapEntity *ent = entities[i];
		if ( idStr::Icmp( ent->epairs.GetString( "name" ), name ) == 0 ) {
			return ent;
		}
	}
	return NULL;
}

/*
===============
idMapFile::RemoveEntity
===============
*/
void idMapFile::RemoveEntity( idMapEntity *mapEnt ) {
	entities.Remove( mapEnt );
	delete mapEnt;
}

/*
===============
idMapFile::RemoveEntity
===============
*/
void idMapFile::RemoveEntities( const char *classname ) {
	for ( int i = 0; i < entities.Num(); i++ ) {
		idMapEntity *ent = entities[i];
		if ( idStr::Icmp( ent->epairs.GetString( "classname" ), classname ) == 0 ) {
			delete entities[i];
			entities.RemoveIndex( i );
			i--;
		}
	}
}

/*
===============
idMapFile::RemoveAllEntities
===============
*/
void idMapFile::RemoveAllEntities() {
	entities.DeleteContents( true );
	hasPrimitiveData = false;
}

/*
===============
idMapFile::RemovePrimitiveData
===============
*/
void idMapFile::RemovePrimitiveData() {
	for ( int i = 0; i < entities.Num(); i++ ) {
		idMapEntity *ent = entities[i];
		ent->RemovePrimitiveData();
	}
	hasPrimitiveData = false;
}

/*
===============
idMapFile::NeedsReload
===============
*/
bool idMapFile::NeedsReload() {
	if ( name.Length() ) {
		ID_TIME_T time = FILE_NOT_FOUND_TIMESTAMP;
		if ( idLib::fileSystem->ReadFile( name, NULL, &time ) > 0 ) {
			return ( time > fileTime );
		}
	}
	return true;
}
