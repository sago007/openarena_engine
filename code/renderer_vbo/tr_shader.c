/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include TR_CONFIG_H
#include TR_LOCAL_H

// tr_shader.c -- this file deals with the parsing and definition of shaders

static char *s_shaderText;

// the shader is parsed into these global variables, then copied into
// dynamically allocated memory if it is valid.
static	shaderStage_t	stages[MAX_SHADER_STAGES];		
static	shader_t		shader;
static	texModInfo_t	texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];

#define FILE_HASH_SIZE		1024
static	shader_t*		hashTable[FILE_HASH_SIZE];

#define MAX_SHADERTEXT_HASH		2048
static char **shaderTextHashTable[MAX_SHADERTEXT_HASH];

/*
================
return a hash value for the filename
================
*/
#ifdef __GNUCC__
  #warning TODO: check if long is ok here 
#endif
static long generateHashValue( const char *fname, const int size ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		if (letter == PATH_SEP) letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (size-1);
	return hash;
}

void R_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset) {
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh, *sh2;
	qhandle_t	h;

	sh = R_FindShaderByName( shaderName );
	if (sh == NULL || sh == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(shaderName, 0);
		sh = R_GetShaderByHandle(h);
	}
	if (sh == NULL || sh == tr.defaultShader) {
		ri.Printf( PRINT_WARNING, "WARNING: R_RemapShader: shader %s not found\n", shaderName );
		return;
	}

	sh2 = R_FindShaderByName( newShaderName );
	if (sh2 == NULL || sh2 == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(newShaderName, 0);
		sh2 = R_GetShaderByHandle(h);
	}

	if (sh2 == NULL || sh2 == tr.defaultShader) {
		ri.Printf( PRINT_WARNING, "WARNING: R_RemapShader: new shader %s not found\n", newShaderName );
		return;
	}

	// remap all the shaders with the given name
	// even tho they might have different lightmaps
	COM_StripExtension(shaderName, strippedName, sizeof(strippedName));
	hash = generateHashValue(strippedName, FILE_HASH_SIZE);
	for (sh = hashTable[hash]; sh; sh = sh->next) {
		if (Q_stricmp(sh->name, strippedName) == 0) {
			if (sh != sh2) {
				sh->remappedShader = sh2;
			} else {
				sh->remappedShader = NULL;
			}
		}
	}
	if (timeOffset) {
		sh2->timeOffset = atof(timeOffset);
	}
}

/*
===============
ParseVector
===============
*/
static qboolean ParseVector( char **text, int count, float *v ) {
	char	*token;
	int		i;

	// FIXME: spaces are currently required after parens, should change parseext...
	token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, "(" ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return qfalse;
	}

	for ( i = 0 ; i < count ; i++ ) {
		token = COM_ParseExt( text, qfalse );
		if ( !token[0] ) {
			ri.Printf( PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name );
			return qfalse;
		}
		v[i] = atof( token );
	}

	token = COM_ParseExt( text, qfalse );
	if ( strcmp( token, ")" ) ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return qfalse;
	}

	return qtrue;
}


/*
===============
NameToAFunc
===============
*/
static unsigned NameToAFunc( const char *funcname )
{	
	if ( !Q_stricmp( funcname, "GT0" ) )
	{
		return GLS_ATEST_GT_0;
	}
	else if ( !Q_stricmp( funcname, "LT128" ) )
	{
		return GLS_ATEST_LT_80;
	}
	else if ( !Q_stricmp( funcname, "GE128" ) )
	{
		return GLS_ATEST_GE_80;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader.name );
	return 0;
}


/*
===============
NameToSrcBlendMode
===============
*/
static int NameToSrcBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_SRCBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static int NameToDstBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return GLS_DSTBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return GLS_DSTBLEND_ONE;
}

/*
===============
NameToGenFunc
===============
*/
static genFunc_t NameToGenFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "sin" ) )
	{
		return GF_SIN;
	}
	else if ( !Q_stricmp( funcname, "square" ) )
	{
		return GF_SQUARE;
	}
	else if ( !Q_stricmp( funcname, "triangle" ) )
	{
		return GF_TRIANGLE;
	}
	else if ( !Q_stricmp( funcname, "sawtooth" ) )
	{
		return GF_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "inversesawtooth" ) )
	{
		return GF_INVERSE_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "noise" ) )
	{
		return GF_NOISE;
	}

	ri.Printf( PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader.name );
	return GF_SIN;
}


/*
===================
ParseWaveForm
===================
*/
static void ParseWaveForm( char **text, waveForm_t *wave )
{
	char *token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->func = NameToGenFunc( token );

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->base = atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->amplitude = atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->phase = atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->frequency = atof( token );
}


/*
===================
ParseTexMod
===================
*/
static void ParseTexMod( char *_text, shaderStage_t *stage )
{
	const char *token;
	char **text = &_text;
	texModInfo_t *tmi;

	if ( stage->bundle[0].numTexMods == TR_MAX_TEXMODS ) {
		ri.Error( ERR_DROP, "ERROR: too many tcMod stages in shader '%s'", shader.name );
		return;
	}

	tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
	stage->bundle[0].numTexMods++;

	token = COM_ParseExt( text, qfalse );

	//
	// turb
	//
	if ( !Q_stricmp( token, "turb" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = atof( token );

		tmi->type = TMOD_TURBULENT;
	}
	//
	// scale
	//
	else if ( !Q_stricmp( token, "scale" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scale[1] = atof( token );
		tmi->type = TMOD_SCALE;
	}
	//
	// scroll
	//
	else if ( !Q_stricmp( token, "scroll" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[0] = atof( token );
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->scroll[1] = atof( token );
		tmi->type = TMOD_SCROLL;
	}
	//
	// stretch
	//
	else if ( !Q_stricmp( token, "stretch" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.func = NameToGenFunc( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = atof( token );
		
		tmi->type = TMOD_STRETCH;
	}
	//
	// transform
	//
	else if ( !Q_stricmp( token, "transform" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][1] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0] = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[1] = atof( token );

		tmi->type = TMOD_TRANSFORM;
	}
	//
	// rotate
	//
	else if ( !Q_stricmp( token, "rotate" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->rotateSpeed = atof( token );
		tmi->type = TMOD_ROTATE;
	}
	//
	// entityTranslate
	//
	else if ( !Q_stricmp( token, "entityTranslate" ) )
	{
		tmi->type = TMOD_ENTITY_TRANSLATE;
	}
	else
	{
		ri.Printf( PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader.name );
	}
}


/*
===================
ParseStage
===================
*/
static qboolean ParseStage( shaderStage_t *stage, char **text )
{
	char *token;
	int depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
	qboolean depthMaskExplicit = qfalse;

	stage->active = qtrue;

	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no matching '}' found\n" );
			return qfalse;
		}

		if ( token[0] == '}' )
		{
			break;
		}
		//
		// map <name>
		//
		else if ( !Q_stricmp( token, "map" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if ( !Q_stricmp( token, "$whiteimage" ) )
			{
				stage->bundle[0].image[0] = tr.whiteImage;
				continue;
			}
			else if ( !Q_stricmp( token, "$lightmap" ) )
			{
				stage->bundle[0].isLightmap = qtrue;
				if ( shader.lightmapIndex < 0 ) {
					stage->bundle[0].image[0] = tr.whiteImage;
				} else {
					stage->bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
				}
				continue;
			}
			else
			{
				stage->bundle[0].image[0] = R_FindImageFile( token, !shader.noMipMaps, !shader.noPicMip, GL_REPEAT );
				if ( !stage->bundle[0].image[0] )
				{
					ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
					return qfalse;
				}
			}
		}
		//
		// clampmap <name>
		//
		else if ( !Q_stricmp( token, "clampmap" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			stage->bundle[0].image[0] = R_FindImageFile( token, !shader.noMipMaps, !shader.noPicMip, GL_CLAMP_TO_EDGE );
			if ( !stage->bundle[0].image[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
				return qfalse;
			}
		}
		//
		// animMap <frequency> <image1> .... <imageN>
		//
		else if ( !Q_stricmp( token, "animMap" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'animMmap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			stage->bundle[0].imageAnimationSpeed = atof( token );

			// parse up to MAX_IMAGE_ANIMATIONS animations
			while ( 1 ) {
				int		num;

				token = COM_ParseExt( text, qfalse );
				if ( !token[0] ) {
					break;
				}
				num = stage->bundle[0].numImageAnimations;
				if ( num < MAX_IMAGE_ANIMATIONS ) {
					stage->bundle[0].image[num] = R_FindImageFile( token, !shader.noMipMaps, !shader.noPicMip, GL_REPEAT );
					if ( !stage->bundle[0].image[num] )
					{
						ri.Printf( PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token, shader.name );
						return qfalse;
					}
					stage->bundle[0].numImageAnimations++;
				}
			}
			if( stage->bundle[0].numImageAnimations > 1 ) {
				stage->bundle[0].combinedImage = R_CombineImages(stage->bundle[0].numImageAnimations,
										 stage->bundle[0].image);
			}
		}
		else if ( !Q_stricmp( token, "videoMap" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'videoMmap' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}
			stage->bundle[0].videoMapHandle = ri.CIN_PlayCinematic( token, 0, 0, 256, 256, (CIN_loop | CIN_silent | CIN_shader));
			if (stage->bundle[0].videoMapHandle != -1) {
				stage->bundle[0].isVideoMap = qtrue;
				stage->bundle[0].image[0] = tr.scratchImage[stage->bundle[0].videoMapHandle];
			}
		}
		//
		// alphafunc <func>
		//
		else if ( !Q_stricmp( token, "alphaFunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			atestBits = NameToAFunc( token );
		}
		//
		// depthFunc <func>
		//
		else if ( !Q_stricmp( token, "depthfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );

			if ( !token[0] )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name );
				return qfalse;
			}

			if ( !Q_stricmp( token, "lequal" ) )
			{
				depthFuncBits = 0;
			}
			else if ( !Q_stricmp( token, "equal" ) )
			{
				depthFuncBits = GLS_DEPTHFUNC_EQUAL;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// detail
		//
		else if ( !Q_stricmp( token, "detail" ) )
		{
			stage->isDetail = qtrue;
		}
		//
		// blendfunc <srcFactor> <dstFactor>
		// or blendfunc <add|filter|blend>
		//
		else if ( !Q_stricmp( token, "blendfunc" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
				continue;
			}
			// check for "simple" blends first
			if ( !Q_stricmp( token, "add" ) ) {
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			} else if ( !Q_stricmp( token, "filter" ) ) {
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			} else if ( !Q_stricmp( token, "blend" ) ) {
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			} else {
				// complex double blends
				blendSrcBits = NameToSrcBlendMode( token );

				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					ri.Printf( PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
					continue;
				}
				blendDstBits = NameToDstBlendMode( token );
			}

			// clear depth mask for blended surfaces
			if ( !depthMaskExplicit )
			{
				depthMaskBits = 0;
			}
		}
		//
		// rgbGen
		//
		else if ( !Q_stricmp( token, "rgbGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->rgbWave );
				stage->rgbGen = CGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				vec3_t	color;

				ParseVector( text, 3, color );
				stage->constantColor[0] = 255 * color[0];
				stage->constantColor[1] = 255 * color[1];
				stage->constantColor[2] = 255 * color[2];

				stage->rgbGen = CGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->rgbGen = CGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "identityLighting" ) )
			{
				if ( r_overBrightBits->integer == 0 )
					stage->rgbGen = CGEN_IDENTITY;
				else
					stage->rgbGen = CGEN_IDENTITY_LIGHTING;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->rgbGen = CGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->rgbGen = CGEN_VERTEX;
				if ( stage->alphaGen == 0 ) {
					stage->alphaGen = AGEN_VERTEX;
				}
			}
			else if ( !Q_stricmp( token, "exactVertex" ) )
			{
				stage->rgbGen = CGEN_EXACT_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingDiffuse" ) )
			{
				stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// alphaGen 
		//
		else if ( !Q_stricmp( token, "alphaGen" ) )
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->alphaWave );
				stage->alphaGen = AGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				token = COM_ParseExt( text, qfalse );
				stage->constantColor[3] = 255 * atof( token );
				stage->alphaGen = AGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->alphaGen = AGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->alphaGen = AGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->alphaGen = AGEN_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingSpecular" ) )
			{
				stage->alphaGen = AGEN_LIGHTING_SPECULAR;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
			}
			else if ( !Q_stricmp( token, "portal" ) )
			{
				stage->alphaGen = AGEN_PORTAL;
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
				{
					shader.portalRange = 256;
					ri.Printf( PRINT_WARNING, "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader.name );
				}
				else
				{
					shader.portalRange = atof( token );
				}
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// tcGen <function>
		//
		else if ( !Q_stricmp(token, "texgen") || !Q_stricmp( token, "tcGen" ) ) 
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "environment" ) )
			{
				stage->bundle[0].tcGen = TCGEN_ENVIRONMENT_MAPPED;
			}
			else if ( !Q_stricmp( token, "lightmap" ) )
			{
				stage->bundle[0].tcGen = TCGEN_LIGHTMAP;
			}
			else if ( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ) )
			{
				stage->bundle[0].tcGen = TCGEN_TEXTURE;
			}
			else if ( !Q_stricmp( token, "vector" ) )
			{
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[0] );
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[1] );

				stage->bundle[0].tcGen = TCGEN_VECTOR;
			}
			else 
			{
				ri.Printf( PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", shader.name );
			}
		}
		//
		// tcMod <type> <...>
		//
		else if ( !Q_stricmp( token, "tcMod" ) )
		{
			char buffer[1024] = "";

			while ( 1 )
			{
				token = COM_ParseExt( text, qfalse );
				if ( token[0] == 0 )
					break;
				strcat( buffer, token );
				strcat( buffer, " " );
			}

			ParseTexMod( buffer, stage );

			continue;
		}
		//
		// depthmask
		//
		else if ( !Q_stricmp( token, "depthwrite" ) )
		{
			depthMaskBits = GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = qtrue;

			continue;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	// I assume DST_ALPHA is always 1, so I just replace it with GL_ONE
	if ( blendSrcBits == GLS_SRCBLEND_DST_ALPHA )
		blendSrcBits = GLS_SRCBLEND_ONE;
	else if ( blendSrcBits == GLS_SRCBLEND_ONE_MINUS_DST_ALPHA )
		blendSrcBits = GLS_SRCBLEND_ZERO;

	if ( blendDstBits == GLS_DSTBLEND_DST_ALPHA )
		blendDstBits = GLS_DSTBLEND_ONE;
	else if ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_DST_ALPHA )
		blendDstBits = GLS_DSTBLEND_ZERO;

	// If the image has no (real) alpha channel, I can do the same
	// for SRC_ALPHA
	if ( !stage->bundle[0].image[0]->hasAlpha &&
	     stage->alphaGen == AGEN_IDENTITY) {
		if ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA )
			blendSrcBits = GLS_SRCBLEND_ONE;
		else if ( blendSrcBits == GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA )
			blendSrcBits = GLS_SRCBLEND_ZERO;
		
		if ( blendDstBits == GLS_DSTBLEND_SRC_ALPHA )
			blendDstBits = GLS_DSTBLEND_ONE;
		else if ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
			blendDstBits = GLS_DSTBLEND_ZERO;

		// also alphaFunc makes no sense without alpha
		atestBits = 0;
	} else {
		// image has alpha, if it uses alpha blending I can optimise
		// alphafunc NONE to alphafunc GT0
		if ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA &&
		     blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA &&
		     atestBits == 0 ) {
			atestBits = GLS_ATEST_GT_0;
		}
	}

	//
	// if cgen isn't explicitly specified, use either identity or identitylighting
	//
	if ( stage->rgbGen == CGEN_BAD ) {
		if ( blendSrcBits == 0 ||
			blendSrcBits == GLS_SRCBLEND_ONE || 
			blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) {
			stage->rgbGen = CGEN_IDENTITY_LIGHTING;
		} else {
			stage->rgbGen = CGEN_IDENTITY;
		}
	}


	//
	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	//
	if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && 
		 ( blendDstBits == GLS_DSTBLEND_ZERO ) )
	{
		blendDstBits = blendSrcBits = 0;
		depthMaskBits = GLS_DEPTHMASK_TRUE;
	}

	// decide which agens we can skip
	if ( stage->alphaGen == AGEN_IDENTITY ) {
		if ( stage->rgbGen == CGEN_IDENTITY
			|| stage->rgbGen == CGEN_LIGHTING_DIFFUSE ) {
			stage->alphaGen = AGEN_SKIP;
		}
	}

	//
	// compute state bits
	//
	stage->stateBits = depthMaskBits | 
		               blendSrcBits | blendDstBits | 
					   atestBits | 
					   depthFuncBits;

	return qtrue;
}

/*
===============
ParseDeform

deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes projectionShadow
deformVertexes autoSprite
deformVertexes autoSprite2
deformVertexes text[0-7]
===============
*/
static void ParseDeform( char **text ) {
	char	*token;
	deformStage_t	*ds;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 )
	{
		ri.Printf( PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", shader.name );
		return;
	}

	if ( shader.numDeforms == MAX_SHADER_DEFORMS ) {
		ri.Printf( PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name );
		return;
	}

	ds = &shader.deforms[ shader.numDeforms ];
	shader.numDeforms++;

	if ( !Q_stricmp( token, "projectionShadow" ) ) {
		ds->deformation = DEFORM_PROJECTION_SHADOW;
		return;
	}

	if ( !Q_stricmp( token, "autosprite" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE;
		return;
	}

	if ( !Q_stricmp( token, "autosprite2" ) ) {
		ds->deformation = DEFORM_AUTOSPRITE2;
		return;
	}

	if ( !Q_stricmpn( token, "text", 4 ) ) {
		int		n;
		
		n = token[4] - '0';
		if ( n < 0 || n > 7 ) {
			n = 0;
		}
		ds->deformation = DEFORM_TEXT0 + n;
		return;
	}

	if ( !Q_stricmp( token, "bulge" ) )	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeWidth = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeHeight = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeSpeed = atof( token );

		ds->deformation = DEFORM_BULGE;
		return;
	}

	if ( !Q_stricmp( token, "wave" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}

		if ( atof( token ) != 0 )
		{
			ds->deformationSpread = 1.0f / atof( token );
		}
		else
		{
			ds->deformationSpread = 100.0f;
			ri.Printf( PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_WAVE;
		return;
	}

	if ( !Q_stricmp( token, "normal" ) )
	{
		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.amplitude = atof( token );

		token = COM_ParseExt( text, qfalse );
		if ( token[0] == 0 )
		{
			ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.frequency = atof( token );

		ds->deformation = DEFORM_NORMALS;
		return;
	}

	if ( !Q_stricmp( token, "move" ) ) {
		int		i;

		for ( i = 0 ; i < 3 ; i++ ) {
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 ) {
				ri.Printf( PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
				return;
			}
			ds->moveVector[i] = atof( token );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = DEFORM_MOVE;
		return;
	}

	ri.Printf( PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name );
}


/*
===============
ParseSkyParms

skyParms <outerbox> <cloudheight> <innerbox>
===============
*/
static void ParseSkyParms( char **text ) {
	char		*token;
	static char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
	char		pathname[MAX_QPATH];
	int			i;

	// outerbox
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i=0 ; i<6 ; i++) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga"
				, token, suf[i] );
			shader.sky.outerbox[i] = R_FindImageFile( ( char * ) pathname, qtrue, qtrue, GL_CLAMP_TO_EDGE );

			if ( !shader.sky.outerbox[i] ) {
				shader.sky.outerbox[i] = tr.defaultImage;
			}
		}
	}

	// cloudheight
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	shader.sky.cloudHeight = atof( token );
	if ( !shader.sky.cloudHeight ) {
		shader.sky.cloudHeight = 512;
	}
	R_InitSkyTexCoords( shader.sky.cloudHeight );


	// innerbox
	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i=0 ; i<6 ; i++) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s.tga"
				, token, suf[i] );
			shader.sky.innerbox[i] = R_FindImageFile( ( char * ) pathname, qtrue, qtrue, GL_REPEAT );
			if ( !shader.sky.innerbox[i] ) {
				shader.sky.innerbox[i] = tr.defaultImage;
			}
		}
	}

	shader.isSky = qtrue;
}


/*
=================
ParseSort
=================
*/
void ParseSort( char **text ) {
	char	*token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) {
		ri.Printf( PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", shader.name );
		return;
	}

	if ( !Q_stricmp( token, "portal" ) ) {
		shader.sort = SS_PORTAL;
	} else if ( !Q_stricmp( token, "sky" ) ) {
		shader.sort = SS_ENVIRONMENT;
	} else if ( !Q_stricmp( token, "opaque" ) ) {
		shader.sort = SS_OPAQUE;
	}else if ( !Q_stricmp( token, "decal" ) ) {
		shader.sort = SS_DECAL;
	} else if ( !Q_stricmp( token, "seeThrough" ) ) {
		shader.sort = SS_SEE_THROUGH;
	} else if ( !Q_stricmp( token, "banner" ) ) {
		shader.sort = SS_BANNER;
	} else if ( !Q_stricmp( token, "additive" ) ) {
		shader.sort = SS_BLEND1;
	} else if ( !Q_stricmp( token, "nearest" ) ) {
		shader.sort = SS_NEAREST;
	} else if ( !Q_stricmp( token, "underwater" ) ) {
		shader.sort = SS_UNDERWATER;
	} else {
		shader.sort = atof( token );
	}
}



// this table is also present in q3map

typedef struct {
	char	*name;
	int		clearSolid, surfaceFlags, contents;
} infoParm_t;

infoParm_t	infoParms[] = {
	// server relevant contents
	{"water",		1,	0,	CONTENTS_WATER },
	{"slime",		1,	0,	CONTENTS_SLIME },		// mildly damaging
	{"lava",		1,	0,	CONTENTS_LAVA },		// very damaging
	{"playerclip",	1,	0,	CONTENTS_PLAYERCLIP },
	{"monsterclip",	1,	0,	CONTENTS_MONSTERCLIP },
	{"nodrop",		1,	0,	CONTENTS_NODROP },		// don't drop items or leave bodies (death fog, lava, etc)
	{"nonsolid",	1,	SURF_NONSOLID,	0},						// clears the solid flag

	// utility relevant attributes
	{"origin",		1,	0,	CONTENTS_ORIGIN },		// center of rotating brushes
	{"trans",		0,	0,	CONTENTS_TRANSLUCENT },	// don't eat contained surfaces
	{"detail",		0,	0,	CONTENTS_DETAIL },		// don't include in structural bsp
	{"structural",	0,	0,	CONTENTS_STRUCTURAL },	// force into structural bsp even if trnas
	{"areaportal",	1,	0,	CONTENTS_AREAPORTAL },	// divides areas
	{"clusterportal", 1,0,  CONTENTS_CLUSTERPORTAL },	// for bots
	{"donotenter",  1,  0,  CONTENTS_DONOTENTER },		// for bots

	{"fog",			1,	0,	CONTENTS_FOG},			// carves surfaces entering
	{"sky",			0,	SURF_SKY,		0 },		// emit light from an environment map
	{"lightfilter",	0,	SURF_LIGHTFILTER, 0 },		// filter light going through it
	{"alphashadow",	0,	SURF_ALPHASHADOW, 0 },		// test light on a per-pixel basis
	{"hint",		0,	SURF_HINT,		0 },		// use as a primary splitter

	// server attributes
	{"slick",		0,	SURF_SLICK,		0 },
	{"noimpact",	0,	SURF_NOIMPACT,	0 },		// don't make impact explosions or marks
	{"nomarks",		0,	SURF_NOMARKS,	0 },		// don't make impact marks, but still explode
	{"ladder",		0,	SURF_LADDER,	0 },
	{"nodamage",	0,	SURF_NODAMAGE,	0 },
	{"metalsteps",	0,	SURF_METALSTEPS,0 },
	{"flesh",		0,	SURF_FLESH,		0 },
	{"nosteps",		0,	SURF_NOSTEPS,	0 },

	// drawsurf attributes
	{"nodraw",		0,	SURF_NODRAW,	0 },	// don't generate a drawsurface (or a lightmap)
	{"pointlight",	0,	SURF_POINTLIGHT, 0 },	// sample lighting at vertexes
	{"nolightmap",	0,	SURF_NOLIGHTMAP,0 },	// don't generate a lightmap
	{"nodlight",	0,	SURF_NODLIGHT, 0 },		// don't ever add dynamic lights
	{"dust",		0,	SURF_DUST, 0}			// leave a dust trail when walking on this surface
};


/*
===============
ParseSurfaceParm

surfaceparm <name>
===============
*/
static void ParseSurfaceParm( char **text ) {
	char	*token;
	int		numInfoParms = ARRAY_LEN( infoParms );
	int		i;

	token = COM_ParseExt( text, qfalse );
	for ( i = 0 ; i < numInfoParms ; i++ ) {
		if ( !Q_stricmp( token, infoParms[i].name ) ) {
			shader.surfaceFlags |= infoParms[i].surfaceFlags;
			shader.contentFlags |= infoParms[i].contents;
#if 0
			if ( infoParms[i].clearSolid ) {
				si->contents &= ~CONTENTS_SOLID;
			}
#endif
			break;
		}
	}
}

/*
=================
ParseShader

The current text pointer is at the explicit text definition of the
shader.  Parse it into the global shader variable.  Later functions
will optimize it.
=================
*/
static qboolean ParseShader( char **text )
{
	char *token;
	int s;
	qboolean	polygonOffset = qfalse;

	s = 0;

	token = COM_ParseExt( text, qtrue );
	if ( token[0] != '{' )
	{
		ri.Printf( PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader.name );
		return qfalse;
	}

	while ( 1 )
	{
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] )
		{
			ri.Printf( PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", shader.name );
			return qfalse;
		}

		// end of shader definition
		if ( token[0] == '}' )
		{
			break;
		}
		// stage definition
		else if ( token[0] == '{' )
		{
			if ( s >= MAX_SHADER_STAGES ) {
				ri.Printf( PRINT_WARNING, "WARNING: too many stages in shader %s\n", shader.name );
				return qfalse;
			}

			if ( !ParseStage( &stages[s], text ) )
			{
				return qfalse;
			}
			stages[s].active = qtrue;
			s++;

			continue;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if ( !Q_stricmpn( token, "qer", 3 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		// sun parms
		else if ( !Q_stricmp( token, "q3map_sun" ) ) {
			float	a, b;

			token = COM_ParseExt( text, qfalse );
			tr.sunLight[0] = atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[1] = atof( token );
			token = COM_ParseExt( text, qfalse );
			tr.sunLight[2] = atof( token );
			
			VectorNormalize( tr.sunLight );

			token = COM_ParseExt( text, qfalse );
			a = atof( token );
			VectorScale( tr.sunLight, a, tr.sunLight);

			token = COM_ParseExt( text, qfalse );
			a = atof( token );
			a = a / 180 * M_PI;

			token = COM_ParseExt( text, qfalse );
			b = atof( token );
			b = b / 180 * M_PI;

			tr.sunDirection[0] = cos( a ) * cos( b );
			tr.sunDirection[1] = sin( a ) * cos( b );
			tr.sunDirection[2] = sin( b );
		}
		else if ( !Q_stricmp( token, "deformVertexes" ) ) {
			ParseDeform( text );
			continue;
		}
		else if ( !Q_stricmp( token, "tesssize" ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "clampTime" ) ) {
			token = COM_ParseExt( text, qfalse );
			if (token[0]) {
			  shader.clampTime = atof(token);
			}
		}
		// skip stuff that only the q3map needs
		else if ( !Q_stricmpn( token, "q3map", 5 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		// skip stuff that only q3map or the server needs
		else if ( !Q_stricmp( token, "surfaceParm" ) ) {
			ParseSurfaceParm( text );
			continue;
		}
		// no mip maps
		else if ( !Q_stricmp( token, "nomipmaps" ) )
		{
			shader.noMipMaps = qtrue;
			shader.noPicMip = qtrue;
			continue;
		}
		// no picmip adjustment
		else if ( !Q_stricmp( token, "nopicmip" ) )
		{
			shader.noPicMip = qtrue;
			continue;
		}
		// polygonOffset
		else if ( !Q_stricmp( token, "polygonOffset" ) )
		{
			polygonOffset = qtrue;
			continue;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if ( !Q_stricmp( token, "entityMergable" ) )
		{
			shader.entityMergable = qtrue;
			continue;
		}
		// fogParms
		else if ( !Q_stricmp( token, "fogParms" ) ) 
		{
			if ( !ParseVector( text, 3, shader.fogParms.color ) ) {
				return qfalse;
			}

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) 
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name );
				continue;
			}
			shader.fogParms.depthForOpaque = atof( token );

			// skip any old gradient directions
			SkipRestOfLine( text );
			continue;
		}
		// portal
		else if ( !Q_stricmp(token, "portal") )
		{
			shader.sort = SS_PORTAL;
			continue;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if ( !Q_stricmp( token, "skyparms" ) )
		{
			ParseSkyParms( text );
			continue;
		}
		// light <value> determines flaring in q3map, not needed here
		else if ( !Q_stricmp(token, "light") ) 
		{
			token = COM_ParseExt( text, qfalse );
			continue;
		}
		// cull <face>
		else if ( !Q_stricmp( token, "cull") ) 
		{
			token = COM_ParseExt( text, qfalse );
			if ( token[0] == 0 )
			{
				ri.Printf( PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) || !Q_stricmp( token, "disable" ) )
			{
				shader.cullType = CT_TWO_SIDED;
			}
			else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) )
			{
				shader.cullType = CT_BACK_SIDED;
			}
			else
			{
				ri.Printf( PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token, shader.name );
			}
			continue;
		}
		// sort
		else if ( !Q_stricmp( token, "sort" ) )
		{
			ParseSort( text );
			continue;
		}
		else
		{
			ri.Printf( PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader.name );
			return qfalse;
		}
	}

	//
	// ignore shaders that don't have any stages, unless it is a sky or fog
	//
	if ( s == 0 && !shader.isSky && !(shader.contentFlags & CONTENTS_FOG ) ) {
		return qfalse;
	}

	if ( polygonOffset ) {
		int i;
		for( i = 0; i < s; i++ ) {
			stages[i].stateBits |= GLS_POLYGON_OFFSET;
		}
	}

	shader.explicitlyDefined = qtrue;

	return qtrue;
}

/*
========================================================================================

SHADER OPTIMIZATION AND FOGGING

========================================================================================
*/

/*
===================
ComputeStageIteratorFunc

See if we can use on of the simple fastpath stage functions,
otherwise set to the generic stage function
===================
*/
static void ComputeStageIteratorFunc( void )
{
	int stage;
	int units = glConfig.numTextureUnits;

	if (!units) units = 1;
	
	shader.optimalStageIteratorFunc = RB_StageIteratorGeneric;
	shader.anyGLAttr = 0;
	shader.allGLAttr = GLA_COLOR_mask | GLA_TC1_mask | GLA_TC2_mask;

	//
	// see if this should go into the sky path
	//
	if ( shader.isSky )
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorSky;
		shader.anyGLAttr = GLA_FULL_dynamic;
		goto done;
	}

	// check all deformation stages
	for ( stage = 0; stage < shader.numDeforms; stage ++ ) {
		switch ( shader.deforms[stage].deformation ) {
		case DEFORM_NONE:
			// vertex data for vertex and normal
			break;
		case DEFORM_WAVE:
		case DEFORM_BULGE:
		case DEFORM_MOVE:
			// dynamic vertex, normal from vertex data
			shader.anyGLAttr |= GLA_VERTEX_dynamic;
			break;
		case DEFORM_NORMALS:
			// dynamic normal, vertex from vertex data
			shader.anyGLAttr |= GLA_NORMAL_dynamic;
			break;
		case DEFORM_PROJECTION_SHADOW:
		case DEFORM_AUTOSPRITE:
		case DEFORM_AUTOSPRITE2:
		case DEFORM_TEXT0:
		case DEFORM_TEXT1:
		case DEFORM_TEXT2:
		case DEFORM_TEXT3:
		case DEFORM_TEXT4:
		case DEFORM_TEXT5:
		case DEFORM_TEXT6:
		case DEFORM_TEXT7:
			// dynamic vertex and normal
			shader.anyGLAttr |= GLA_VERTEX_dynamic;
			shader.anyGLAttr |= GLA_NORMAL_dynamic;
			break;
		}
	}
	
	// check all shader stages
	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = &stages[stage];
		int           bundle;
		short         colorType, tcType;
		
		if ( !pStage->active )
		{
			break;
		}
		
		switch ( pStage->rgbGen ) {
		case CGEN_IDENTITY_LIGHTING:
		case CGEN_IDENTITY:
		case CGEN_ENTITY:
		case CGEN_ONE_MINUS_ENTITY:
		case CGEN_CONST:
		case CGEN_WAVEFORM:
			// uniform color
			colorType = GLA_COLOR_uniform;
			break;
		case CGEN_EXACT_VERTEX:
		case CGEN_VERTEX:
			// vertex colors
			colorType = GLA_COLOR_vtxcolor;
			break;
		case CGEN_LIGHTING_DIFFUSE:
		case CGEN_ONE_MINUS_VERTEX:
		case CGEN_BAD:
		case CGEN_FOG:
		default:
			// dynamic colors
			colorType = GLA_COLOR_dynamic;
			break;
		}

		switch ( pStage->alphaGen ) {
		case AGEN_SKIP:
			// doesn't matter
			break;
		case AGEN_IDENTITY:
		case AGEN_ENTITY:
		case AGEN_ONE_MINUS_ENTITY:
		case AGEN_CONST:
		case AGEN_WAVEFORM:
			// uniform alpha
			if ( colorType != GLA_COLOR_uniform )
				colorType = GLA_COLOR_dynamic;
			break;
		case AGEN_VERTEX:
			// vertex alpha
			if( colorType != GLA_COLOR_vtxcolor )
				colorType = GLA_COLOR_dynamic;
			break;
		case AGEN_ONE_MINUS_VERTEX:
		case AGEN_LIGHTING_SPECULAR:
		case AGEN_PORTAL:
		default:
			colorType = GLA_COLOR_dynamic;
			break;
		}
		if( r_greyscale->integer && colorType == GLA_COLOR_vtxcolor ) {
			// grayscale conversion is dynamic
			colorType = GLA_COLOR_dynamic;
		}

		shader.anyGLAttr |= colorType;
		shader.allGLAttr &= ~GLA_COLOR_mask | colorType;
		
		for ( bundle = 0; bundle < units; bundle++ ) {
			int shift = bundle * GLA_TC_shift;

			if ( bundle > 0 && !pStage->bundle[bundle].multitextureEnv )
				break;
			
			switch ( pStage->bundle[bundle].tcGen ) {
			case TCGEN_BAD:
			case TCGEN_IDENTITY:
			default:
				// uniform texcoord
				tcType = GLA_TC1_uniform;
				break;
			case TCGEN_TEXTURE:
				// vertex texcoord
				tcType = GLA_TC1_texcoord;
				break;
			case TCGEN_LIGHTMAP:
				// vertex texcoord (lightmap)
				tcType = GLA_TC1_lmcoord;
				break;
			case TCGEN_VECTOR:
			case TCGEN_FOG:
			case TCGEN_ENVIRONMENT_MAPPED:
				// dynamic texcoord
				tcType = GLA_TC1_dynamic;
				break;
			}
			// texmods are always dynamic
			if ( pStage->bundle[bundle].numTexMods > 0 )
				tcType = GLA_TC1_dynamic;

			shader.anyGLAttr |= tcType << shift;
			shader.allGLAttr &= ~(GLA_TC1_mask << shift)
				| tcType << shift;
		}
	}

	// mirror and portal shaders
	if ( shader.sort <= SS_PORTAL )
		shader.anyGLAttr |= GLA_FULL_dynamic;

	if ( r_ignoreFastPath->integer )
	{
		return;
	}

	//
	// see if this can go into the vertex lit fast path
	//
	if ( shader.numUnfoggedPasses == 1 )
	{
		if ( stages[0].rgbGen == CGEN_LIGHTING_DIFFUSE )
		{
			if ( stages[0].alphaGen == AGEN_IDENTITY )
			{
				if ( stages[0].bundle[0].tcGen == TCGEN_TEXTURE )
				{
					if ( !stages[0].bundle[1].multitextureEnv )
					{
						if ( !shader.numDeforms )
						{
							shader.optimalStageIteratorFunc = RB_StageIteratorVertexLitTexture;
							goto done;
						}
					}
				}
			}
		}
	}

	//
	// see if this can go into an optimized LM, multitextured path
	//
	if ( shader.numUnfoggedPasses == 1 )
	{
		if ( ( stages[0].rgbGen == CGEN_IDENTITY ) && ( stages[0].alphaGen == AGEN_IDENTITY ) )
		{
			if ( stages[0].bundle[0].tcGen == TCGEN_TEXTURE && 
				stages[0].bundle[1].tcGen == TCGEN_LIGHTMAP )
			{
				if ( !shader.numDeforms )
				{
					if ( stages[0].bundle[1].multitextureEnv )
					{
						shader.optimalStageIteratorFunc = RB_StageIteratorLightmappedMultitexture;
						goto done;
					}
				}
			}
		}
	}

done:
	return;
}

typedef struct {
	int		blendA;
	int		blendB;

	int		multitextureEnv;
	int		multitextureBlend;
} collapse_t;

static collapse_t	collapse[] = {
	{ 0, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,	
		GL_MODULATE, 0 },

	{ 0, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
		GL_MODULATE, 0 },

	{ GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
		GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
		GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
		GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
		GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR },

	{ 0, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
		GL_ADD, 0 },

	{ GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
		GL_ADD, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE },
#if 0
	{ 0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA,
		GL_DECAL, 0 },
#endif
	{ -1 }
};

/*
================
CollapseMultitexture

Attempt to combine several stages into a single multitexture stage
FIXME: I think modulated add + modulated add collapses incorrectly
=================
*/
static int CollapseMultitexture( void ) {
	int stage, bundle;
	int abits, bbits;
	int i;
	textureBundle_t tmpBundle;

	stage = 0;
	bundle = 0;

	while( stage < MAX_SHADER_STAGES && stages[stage].active ) {
		if ( bundle + 1 >= glConfig.numTextureUnits ) {
			// can't add next stage, no more texture units
			stage++;
			bundle = 0;
			continue;
		}
		
		// make sure both stages are active
		if ( !stages[stage + 1].active ) {
			// can't add next stage, it doesn't exist
			stage++;
			bundle = 0;
			continue;
		}

		// on voodoo2, don't combine different tmus
		if ( glConfig.driverType == GLDRV_VOODOO ) {
			if ( stages[stage].bundle[0].image[0]->TMU ==
			     stages[stage + 1].bundle[0].image[0]->TMU ) {
				stage++;
				bundle = 0;
				continue;
			}
		}

		abits = stages[stage].stateBits;
		bbits = stages[stage + 1].stateBits;
		/*
		// can't combine if the second stage has an alpha test
		if ( bbits & GLS_ATEST_BITS ) {
			stage++;
			bundle = 0;
			continue;
		}

		// can combine alphafunc only if depthwrite is enabled and
		// the second stage has depthfunc equal
		if ( abits & GLS_ATEST_BITS ) {
			if (!((abits & GLS_DEPTHMASK_TRUE) &&
			      (bbits & GLS_DEPTHFUNC_EQUAL)) ) {
				stage++;
				bundle = 0;
				continue;
			}
		} else {
			if ( (abits & GLS_DEPTHFUNC_EQUAL) !=
			     (bbits & GLS_DEPTHFUNC_EQUAL) ) {
				stage++;
				bundle = 0;
				continue;
			}
		}
		*/
		// make sure that both stages have identical state other than blend modes
		if ( ( abits & ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS | GLS_DEPTHMASK_TRUE ) ) !=
		     ( bbits & ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS | GLS_DEPTHMASK_TRUE ) ) ) {
			stage++;
			bundle = 0;
			continue;
		}
		
		abits &= ( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
		bbits &= ( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
		
		// search for a valid multitexture blend function
		for ( i = 0; collapse[i].blendA != -1 ; i++ ) {
			if ( abits == collapse[i].blendA
			     && bbits == collapse[i].blendB ) {
				break;
			}
		}
		
		// nothing found
		if ( collapse[i].blendA == -1 ) {
			stage++;
			bundle = 0;
			continue;
		}
		
		// GL_ADD is a separate extension
		if ( collapse[i].multitextureEnv == GL_ADD && !glConfig.textureEnvAddAvailable ) {
			stage++;
			bundle = 0;
			continue;
		}
		
		// make sure waveforms have identical parameters
		if ( ( stages[stage].rgbGen != stages[stage + 1].rgbGen ) ||
		     ( stages[stage].alphaGen != stages[stage + 1].alphaGen ) )  {
			stage++;
			bundle = 0;
			continue;
		}
		
		// an add collapse can only have identity colors
		if ( collapse[i].multitextureEnv == GL_ADD && stages[stage].rgbGen != CGEN_IDENTITY ) {
			stage++;
			bundle = 0;
			continue;
		}
		
		if ( stages[stage].rgbGen == CGEN_WAVEFORM )
		{
			if ( memcmp( &stages[stage].rgbWave,
				     &stages[stage + 1].rgbWave,
				     sizeof( stages[stage].rgbWave ) ) )
			{
				stage++;
				bundle = 0;
				continue;
			}
		}
		if ( stages[stage].alphaGen == AGEN_WAVEFORM )
		{
			if ( memcmp( &stages[stage].alphaWave,
				     &stages[stage + 1].alphaWave,
				     sizeof( stages[stage].alphaWave ) ) )
			{
				stage++;
				bundle = 0;
				continue;
			}
		}
		
		
		// make sure that lightmaps are in bundle 1 for 3dfx
		if ( bundle == 0 && stages[stage].bundle[0].isLightmap )
		{
			tmpBundle = stages[stage].bundle[0];
			stages[stage].bundle[0] = stages[stage + 1].bundle[0];
			stages[stage].bundle[1] = tmpBundle;
		}
		else
		{
			stages[stage].bundle[bundle + 1] = stages[stage + 1].bundle[0];
		}
		
		// set the new blend state bits
		stages[stage].bundle[bundle + 1].multitextureEnv = collapse[i].multitextureEnv;
		stages[stage].stateBits &= ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
		stages[stage].stateBits |= collapse[i].multitextureBlend;

		bundle++;

		//
		// move down subsequent shaders
		//
		memmove( &stages[stage + 1], &stages[stage + 2], sizeof( stages[0] ) * ( MAX_SHADER_STAGES - stage - 2 ) );
		Com_Memset( &stages[MAX_SHADER_STAGES-1], 0, sizeof( stages[0] ) );
	}

	return stage;
}

/*
==============
R_SortShaders

Positions all shaders in the tr.sortedShaders[]
array so that the shader->sort key is sorted relative
to the other shaders. If shaders use the same GLSL
program, sort them all to the max occluder to avoid
expensive program switches later.

As sorting by anything but shader->sort is only for
better performance, and the shader->sort order may be only
changed by creating a new shader, I don't need to fully
sort the array - I just make a single round of back-to-front
bubblesort.

Sets shader->sortedIndex
==============
*/
static ID_INLINE int cmpShader( shader_t *l, shader_t *r )
{
	int		diff;
	int		depthL = 0, depthR = 0;
	GLuint		resultL, resultR;
	
	diff = l->sort - r->sort;
	if( !diff ) {
		if( l->stages[0] )
			depthL = l->stages[0]->stateBits & GLS_DEPTHMASK_TRUE;
		if( r->stages[0] )
			depthR = r->stages[0]->stateBits & GLS_DEPTHMASK_TRUE;

		diff = depthR - depthL;
	}
	if( !diff ) {
		if( l->GLSLprogram )
			resultL = l->GLSLprogram->QuerySum;
		else
			resultL = QUERY_RESULT(&l->QueryResult);

		if( r->GLSLprogram )
			resultR = r->GLSLprogram->QuerySum;
		else
			resultR = QUERY_RESULT(&r->QueryResult);

		if( depthL )
			diff = resultR - resultL;
		else
			diff = resultL - resultR;
	}
	if( !diff ) {
		diff = l->GLSLprogram - r->GLSLprogram;
	}
	if( !diff ) {
		if( depthL )
			diff = QUERY_RESULT(&r->QueryResult)
				- QUERY_RESULT(&l->QueryResult);
		else
			diff = QUERY_RESULT(&l->QueryResult)
				- QUERY_RESULT(&r->QueryResult);
	}
	if( !diff ) {
		diff = l->index - r->index;
	}
	return diff;
}
void R_SortShaders( void ) {
	shader_t	*tmp = tr.sortedShaders[tr.numShaders - 1];
	int		idx = tr.numShaders - 2;

	for( idx = tr.numShaders - 2; idx >= 0; idx-- ) {
		if( cmpShader( tmp, tr.sortedShaders[idx] ) >= 0 ) {
			// order is correct
			tr.sortedShaders[idx + 1] = tmp;
			tmp->sortedIndex = idx + 1;
			tmp = tr.sortedShaders[idx];
		} else {
			// swap necessary
			tr.sortedShaders[idx + 1] = tr.sortedShaders[idx];
			tr.sortedShaders[idx + 1]->sortedIndex = idx + 1;
		}
	}
	tr.sortedShaders[0] = tmp;
	tmp->sortedIndex = 0;
}


/*
====================
GeneratePermanentShader
====================
*/
static shader_t *GeneratePermanentShader( void ) {
	shader_t	*newShader;
	int			i, b;
	int			size, hash;

	if ( tr.numShaders == MAX_SHADERS ) {
		ri.Printf( PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
		return tr.defaultShader;
	}

	newShader = ri.Hunk_Alloc( sizeof( shader_t ), h_low );

	*newShader = shader;

	if ( shader.sort <= SS_OPAQUE ) {
		newShader->fogPass = FP_EQUAL;
	} else if ( shader.contentFlags & CONTENTS_FOG ) {
		newShader->fogPass = FP_LE;
	}

	tr.shaders[ tr.numShaders ] = newShader;
	newShader->index = tr.numShaders;
	
	tr.sortedShaders[ tr.numShaders ] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	for ( i = 0 ; i < newShader->numUnfoggedPasses ; i++ ) {
		if ( !stages[i].active ) {
			break;
		}
		newShader->stages[i] = ri.Hunk_Alloc( sizeof( stages[i] ), h_low );
		*newShader->stages[i] = stages[i];

		for ( b = 0 ; b < NUM_TEXTURE_BUNDLES ; b++ ) {
			size = newShader->stages[i]->bundle[b].numTexMods * sizeof( texModInfo_t );
			newShader->stages[i]->bundle[b].texMods = ri.Hunk_Alloc( size, h_low );
			Com_Memcpy( newShader->stages[i]->bundle[b].texMods, stages[i].bundle[b].texMods, size );
		}
	}
	if( shader.optimalStageIteratorFunc == RB_StageIteratorGLSL ) {
		newShader->stages[0]->stateBits &= ~GLS_ATEST_BITS;
	}

	R_SortShaders();

	// prepare occlusion queries for shaders that write to depth buffer
	if ( qglGenQueriesARB &&
	     !r_depthPass->integer &&
	     !newShader->isDepth &&
	     newShader->stages[0] &&
	     !(newShader->stages[0]->stateBits & GLS_COLORMASK_FALSE) ) {
		qglGenQueriesARB( 1, &newShader->QueryID );
		newShader->QueryResult = 0;
	}

	hash = generateHashValue(newShader->name, FILE_HASH_SIZE);
	newShader->next = hashTable[hash];
	hashTable[hash] = newShader;

	return newShader;
}

static void GetBufferUniforms( GLuint program, const GLchar *blockName,
			       int binding, GLuint *buffer, GLsizei *size,
			       int numUniforms,
			       const GLchar **uniformNames,
			       BufUniform_t **uniforms ) {
	GLuint	blockIndex;

	blockIndex = qglGetUniformBlockIndex( program, blockName );
	if( blockIndex != GL_INVALID_INDEX ) {
		if( !*buffer ) {
			GLuint	uniformIndices[16];
			GLint	uniformParams[16];
			int	i;

			if( numUniforms > 16 )
				ri.Error( ERR_DROP, "too many uniforms (%d)", numUniforms );

			qglGetActiveUniformBlockiv( program, blockIndex,
						    GL_UNIFORM_BLOCK_DATA_SIZE,
						    size );

			// create UBO for dlights
			qglGenBuffersARB( 1, buffer );
			GL_UBO( *buffer );
			qglBufferDataARB( GL_UNIFORM_BUFFER, *size,
					  NULL, GL_DYNAMIC_DRAW_ARB );
			GL_UBO( 0 );

			qglBindBufferBase( GL_UNIFORM_BUFFER, binding, *buffer );

			// get the offset and stride of all uniforms
			qglGetUniformIndices( program, numUniforms,
					      uniformNames, uniformIndices );
			qglGetActiveUniformsiv( program, numUniforms,
						uniformIndices,
						GL_UNIFORM_OFFSET,
						uniformParams );
			for( i = 0; i < numUniforms; i++ )
				uniforms[i]->offset = uniformParams[i];

			qglGetActiveUniformsiv( program, numUniforms,
						uniformIndices,
						GL_UNIFORM_ARRAY_STRIDE,
						uniformParams );
			for( i = 0; i < numUniforms; i++ )
				uniforms[i]->stride = uniformParams[i];
		}

		qglUniformBlockBinding( program, blockIndex, binding );
	}
}

/*
=================
CollapseGLSL

Try to compile a GLSL vertex and fragment shader that
computes the same effect as a multipass fixed function shader.
=================
*/
static unsigned short GLSLversion = 0x0000;
static char GLSLTexNames[MAX_SHADER_STAGES][6];
static char *GLSLfnGetLight = ""
	"float fresnel(const float f0, const float x) {\n"
	"  float y = 1.0 - x;\n"
	"  float y2 = y * y;\n"
	"  return mix(f0, 1.0, y2 * y2 * y);\n"
	"}\n"
	"\n"
	"void getLight(const vec4 material, const vec3 pos,\n"
	"              const vec3 normal, const vec3 eye,\n"
	"              out vec3 diffuse, out vec4 specular) {\n"
	"  const float inv_pi = 0.318309886;\n"
	"  float exponent = pow(10000.0, material.x);\n"
	"  float normalReflectance = material.y * 0.05 + 0.017;\n"
	"  float NdotE = dot(normal, eye);\n"
	"  vec3  diffuseLight = directedLight * material.z;\n"
	"  vec4  specularLight = vec4(directedLight - diffuseLight, 1.0);\n"
	"  \n"
	"  vec3  L = normalize(lightDir);\n"
	"  float NdotL = max(dot(normal, L), 0.0);\n"
	"  vec3  H = normalize(eye + L);\n"
	"  float G = (0.039 * exponent + 0.085)/* / max(NdotL, NdotE)*/;\n"
	"  \n"
	"  diffuse = ambientLight + directedLight * (1.0 - normalReflectance) * inv_pi * NdotL;\n"
	"  float HdotE = max(dot(H, eye), 0.0);\n"
	"  float HdotN = max(dot(H, normal), 0.0);\n"
	"  float specFactor = G * fresnel(normalReflectance, HdotE) * NdotL;\n"
	"  float specPart = max(specFactor * pow(HdotN, exponent), 0.0);\n"
	"  diffuse += diffuseLight * specPart;\n"
	"  specular = specularLight * specPart;\n"
	"#ifdef SHADER_DLIGHTS\n"
	"  int node = 0, light = 0;\n"
	"  while( node < dlNum ) {\n"
	"    vec4 sphere = dlSpheres[node];\n"
	"    L = sphere.xyz - pos;\n"
	"    if( length(L) <= sphere.w ) {\n"
	"      if( dlLinks[node] == 1 ) {\n"
	"        float attenuation = 0.125 * sphere.w / length(L);\n"
	"        vec3 lightColor = dlColors[light] * (attenuation * attenuation);\n"
	"        L = normalize(L);\n"
	"        NdotL = max(dot(normal, L), 0.0);\n"
	"        H = normalize(eye + L);\n"
	"        G = (0.039 * exponent + 0.085)/* / max(NdotL, NdotE)*/;\n"
	"        \n"
	"        diffuse += lightColor * (1.0 - normalReflectance) * inv_pi * NdotL;\n"
	"        HdotE = max(dot(H, eye), 0.0);\n"
	"        HdotN = max(dot(H, normal), 0.0);\n"
	"        specFactor = G * fresnel(normalReflectance, HdotE) * NdotL;\n"
	"        specPart = max(specFactor * pow(HdotN, exponent), 0.0);\n"
	"        diffuseLight = lightColor * material.z;\n"
	"        specularLight = lightColor - diffuseLight;\n"
	"        diffuse += diffuseLight * specPart;\n"
	"        specular += specularLight * specPart;\n"
	"        light++;\n"
	"      }\n"
	"      node++;\n"
	"    } else {\n"
	"      light += dlLinks[node];\n"
	"      node += max(2 * dlLinks[node] - 1, 1);\n"
	"    }\n"
	"  }\n"
	"#endif\n"
	"}\n\n";
static char *GLSLfnGenSin =
	"float genFuncSin(in float x) {\n"
	"  return sin(6.283185308 * x);\n"
	"}\n\n";
static char *GLSLfnGenSquare =
	"float genFuncSquare(in float x) {\n"
	"  return sign(fract(x) - 0.5);\n"
	"}\n\n";
static char *GLSLfnGenTriangle =
	"float genFuncTriangle(in float x) {\n"
	"  return 4.0 * abs(fract(x - 0.25) - 0.5) - 1.0;\n"
	"}\n\n";
static char *GLSLfnGenSawtooth =
	"float genFuncSawtooth(in float x) {\n"
	"  return fract(x);\n"
	"}\n\n";
static char *GLSLfnGenInverseSawtooth =
	"float genFuncInverseSawtooth(in float x) {\n"
	"  return 1.0 - fract(x);\n"
	"}\n\n";
static const char *VSHeader, *GSHeader, *FSHeader;

static int CollapseGLSL( void ) {
	enum VSFeatures {
		vsfShaderTime = 0x00000001,
		vsfNormal     = 0x00000002,
		vsfColor      = 0x00000004,
		vsfEntColor   = 0x00000008,
		vsfTexCoord   = 0x00000010,
		vsfTexCoord2  = 0x00000020,
		vsfCameraPos  = 0x00000040,
		vsfEntLight   = 0x00000080,
		vsfLightDir   = 0x00000100,
		vsfFogNum     = 0x00000200,
		vsfGenSin     = 0x00001000,
		vsfGenSquare  = 0x00002000,
		vsfGenTri     = 0x00004000,
		vsfGenSaw     = 0x00008000,
		vsfGenInvSaw  = 0x00010000,
		vsfGenNoise   = 0x00020000
	} vsFeatures = 0;
	enum FSFeatures {
		fsfShaderTime = 0x00000001,
		fsfVertex     = 0x00000002,
		fsfReflView   = 0x00000004,
		fsfLightDir   = 0x00000008,
		fsfCameraPos  = 0x00000010,
		fsfNormal     = 0x00000020,
		fsfTangents   = 0x00000040,
		fsfDiffuse    = 0x00000080,
		fsfSpecular   = 0x00000100,
		fsfGenSin     = 0x00001000,
		fsfGenSquare  = 0x00002000,
		fsfGenTri     = 0x00004000,
		fsfGenSaw     = 0x00008000,
		fsfGenInvSaw  = 0x00010000,
		fsfGenNoise   = 0x00020000,
		fsfGenRotate  = 0x00040000,
		fsfGetLight   = 0x00080000
	} fsFeatures = 0;
	unsigned int attributes = (1 << AL_VERTEX) |
		(1 << AL_TRANSX) | (1 << AL_TRANSY) | (1 << AL_TRANSZ);

	const char *VS[1000];
	const char *GS[1000];
	const char *FS[1000];
	byte        constantColor[MAX_SHADER_STAGES][4];
	int         texIndex[MAX_SHADER_STAGES];
	char        shaderConsts[100][20];
	int         VSidx = 0;
	int         GSidx = 0;
	int         FSidx = 0;
	int         constidx = 0;
	int         i, j;
	int         lightmapStage = -1;
	int         normalStage = -1;
	int         materialStage = -1;
	int         srcBlend, dstBlend;
	alphaGen_t  aGen;
	qboolean    showDepth = qfalse;
	qboolean    MultIsZero, AddIsZero, MultIsOnePlus;
	int         aTestStart = -1;

	// helper macros to build the Vertex and Fragment Shaders
#define VSText(text) VS[VSidx++] = text
#define VSConst(format, value) VS[VSidx++] = shaderConsts[constidx]; Com_sprintf( shaderConsts[constidx++], sizeof(shaderConsts[0]), format, value)

#define GSText(text) GS[GSidx++] = text
#define GSConst(format, value) GS[GSidx++] = shaderConsts[constidx]; Com_sprintf( shaderConsts[constidx++], sizeof(shaderConsts[0]), format, value)

#define FSText(text) FS[FSidx++] = text
#define FSConst(format, value) FS[FSidx++] = shaderConsts[constidx]; Com_sprintf( shaderConsts[constidx++], sizeof(shaderConsts[0]), format, value)
#define FSGenFunc(wave) FSText("(");					\
			FSConst("%f", wave.base);			\
			FSText(" + ");					\
			FSConst("%f", wave.amplitude);			\
			switch( wave.func ) {				\
			case GF_NONE:					\
				return qfalse;				\
			case GF_SIN:					\
				FSText(" * genFuncSin(");		\
				break;					\
			case GF_SQUARE:					\
				FSText(" * genFuncSquare(");		\
				break;					\
			case GF_TRIANGLE:				\
				FSText(" * genFuncTriangle(");		\
				break;					\
			case GF_SAWTOOTH:				\
				FSText(" * genFuncSawtooth(");		\
				break;					\
			case GF_INVERSE_SAWTOOTH:			\
				FSText(" * genFuncInverseSawtooth(");	\
				break;					\
			case GF_NOISE:					\
				FSText(" * genFuncNoise(");		\
				break;					\
			}						\
			FSConst("%f", wave.phase);			\
			FSText(" + ");					\
			FSConst("%f", wave.frequency);			\
			FSText("* vShadertime))");

	if( !(qglCreateShader && stages[0].active) || shader.isSky ) {
		// single stage can be rendered without GLSL
		return qfalse;
	}

	if( shader.lightmapIndex == LIGHTMAP_MD3 && !qglGenBuffersARB ) {
		// frame interpolation requires VBOs
		return qfalse;
	}

	if( !GLSLversion ) {
		const char *GLSLString = (const char *)glGetString( GL_SHADING_LANGUAGE_VERSION_ARB );
		int major, minor;

		sscanf( GLSLString, "%d.%d", &major, &minor );
		GLSLversion = (unsigned short)(major << 8 | minor);
		
		for( i = 0; i < MAX_SHADER_STAGES; i++ ) {
			Com_sprintf( GLSLTexNames[i], sizeof(GLSLTexNames[i]),
				     "tex%02d", i );
		}

		if ( GLSLversion >= 0x0132 ) {
			// GLSL 1.50 (OpenGL 3.2) supported
			VSHeader =
				"#version 150 compatibility\n"
				"\n"
				"#define IN(decl) in decl\n"
				"#define OUT(qual, decl) qual out decl\n"
				"#define tex2D(s, tc) texture(s, tc)\n"
				"#define texFetch(s, tc) texelFetch(s, tc)\n"
				"\n";
			GSHeader =
				"#version 150 compatibility\n"
				"\n"
				"#define IN(qual, decl) qual in decl\n"
				"#define OUT(qual, decl) qual out decl\n"
				"\n";
			FSHeader =
				"#version 150 compatibility\n"
				"\n"
				"#define IN(qual, decl) qual in decl\n"
				"#define OUT(decl) out decl\n"
				"#define tex2D(s, tc) texture(s, tc)\n"
				"#define tex2DBias(s, tc, bias) texture(s, tc, bias)\n"
				"#define tex2DLod(s, tc, lod) textureLod(s, tc, lod)\n"
				"#define tex3D(s, tc) texture(s, tc)\n"
				"\n";
		} else {
			VSHeader =
				"#version 110\n"
				"\n"
				"#define IN(decl) attribute decl\n"
				"#define OUT(qual, decl) varying decl\n"
				"#define tex2D(s, tc) texture2D(s, tc)\n"
				"#define texFetch(s, tc) texelFetchBuffer(s, tc)\n"
				"\n";
			GSHeader =
				"#version 110\n"
				"#extension GL_EXT_geometry_shader4 : enable\n"
				"#extension GL_EXT_gpu_shader4 : enable\n"
				"\n"
				"#define IN(qual, decl) varying in decl\n"
				"#define OUT(qual, decl) varying out decl\n"
				"\n";
			FSHeader =
				"#version 110\n"
				"\n"
				"#define IN(qual, decl) varying decl\n"
				"#define OUT(decl) varying out decl\n"
				"#define tex2D(s, tc) texture2D(s, tc)\n"
				"#define tex2DBias(s, tc, bias) texture2D(s, tc, bias)\n"
				"#define tex2DLod(s, tc, lod) texture2DLod(s, tc, lod)\n"
				"#define tex3D(s, tc) texture3D(s, tc)\n"
				"\n";
		}
	}

	// debug option
	if( r_depthPass->integer >= 2 && shader.lightmapIndex != LIGHTMAP_2D ) {
		if( stages[0].stateBits & GLS_COLORMASK_FALSE ) {
			stages[0].stateBits &= ~GLS_COLORMASK_FALSE;
			vsFeatures |= vsfCameraPos;
			fsFeatures |= fsfVertex | fsfCameraPos;
			showDepth = qtrue;
		} else {
			stages[0].stateBits |= GLS_COLORMASK_FALSE;
		}
	}

	srcBlend = stages[0].stateBits & GLS_SRCBLEND_BITS;
	dstBlend = stages[0].stateBits & GLS_DSTBLEND_BITS;
#if 1
	// hack shader for tremulous medistation
	if( srcBlend == GLS_SRCBLEND_ONE_MINUS_DST_COLOR &&
	    dstBlend == GLS_DSTBLEND_ONE) {
		stages[0].stateBits &= ~GLS_SRCBLEND_BITS;
		stages[0].stateBits |= GLS_SRCBLEND_ONE;
		srcBlend = GLS_SRCBLEND_ONE;
	}
	if( srcBlend == GLS_SRCBLEND_ONE &&
	    dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR) {
		stages[0].stateBits &= ~GLS_DSTBLEND_BITS;
		stages[0].stateBits |= GLS_DSTBLEND_ONE;
		dstBlend = GLS_DSTBLEND_ONE;
	}
#endif

	// *** compute required features ***

	// deforms
	for( i = 0; i < shader.numDeforms; i++ ) {
		switch( shader.deforms[i].deformation ) {
		case DEFORM_NONE:
			break;
		case DEFORM_WAVE:
			vsFeatures |= vsfNormal;
			// fall through
		case DEFORM_MOVE:
			switch( shader.deforms[i].deformationWave.func ) {
			case GF_NONE:
				return qfalse;
			case GF_SIN:
				vsFeatures |= vsfShaderTime | vsfGenSin;
				break;
			case GF_SQUARE:
				vsFeatures |= vsfShaderTime | vsfGenSquare;
				break;
			case GF_TRIANGLE:
				vsFeatures |= vsfShaderTime | vsfGenTri;
				break;
			case GF_SAWTOOTH:
				vsFeatures |= vsfShaderTime | vsfGenSaw;
				break;
			case GF_INVERSE_SAWTOOTH:
				vsFeatures |= vsfShaderTime | vsfGenInvSaw;
				break;
			case GF_NOISE:
				vsFeatures |= vsfShaderTime | vsfGenNoise;
				break;
			}
			break;
		case DEFORM_NORMALS:
			vsFeatures |= vsfShaderTime;
			break;
		case DEFORM_BULGE:
			vsFeatures |= vsfTexCoord | vsfNormal | vsfShaderTime;
			break;
		default:
			return qfalse;
		}
	}
	// textures
	for( i = 0; i < MAX_SHADER_STAGES; i++ ) {
		shaderStage_t *pStage = &stages[i];
		
		if( !pStage->active ) {
			break;
		}

		if( pStage->bundle[0].isLightmap ) {
			lightmapStage = i;
		}

		srcBlend = pStage->stateBits & GLS_SRCBLEND_BITS;
		dstBlend = pStage->stateBits & GLS_DSTBLEND_BITS;
		
		// this is called before CollapseMultitexture,
		// so each stages has at most one bundle
		switch( pStage->bundle[0].tcGen ) {
		case TCGEN_IDENTITY:
			break;
		case TCGEN_LIGHTMAP:
			vsFeatures |= vsfTexCoord;
			break;
		case TCGEN_TEXTURE:
			vsFeatures |= vsfTexCoord;
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
			vsFeatures |= vsfNormal | vsfCameraPos;
			fsFeatures |= fsfNormal | fsfReflView | fsfCameraPos;
			break;
		case TCGEN_FOG:
			return qfalse;
		case TCGEN_VECTOR:
			fsFeatures |= fsfVertex;
			break;
		default:
			return qfalse;
		}
		
		for( j = 0; j < pStage->bundle[0].numTexMods; j++ ) {
			texModInfo_t *pTexMod = &(pStage->bundle[0].texMods[j]);
			
			switch( pTexMod->type ) {
			case TMOD_NONE:
				break;
			case TMOD_TRANSFORM:
				break;
			case TMOD_TURBULENT:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfVertex;
				break;
			case TMOD_SCROLL:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime;
				break;
			case TMOD_SCALE:
				break;
			case TMOD_STRETCH:
				switch( pTexMod->wave.func ) {
				case GF_NONE:
					return qfalse;
				case GF_SIN:
					vsFeatures |= vsfShaderTime;
					fsFeatures |= fsfShaderTime | fsfGenSin;
					break;
				case GF_SQUARE:
					vsFeatures |= vsfShaderTime;
					fsFeatures |= fsfShaderTime | fsfGenSquare;
					break;
				case GF_TRIANGLE:
					vsFeatures |= vsfShaderTime;
					fsFeatures |= fsfShaderTime | fsfGenTri;
					break;
				case GF_SAWTOOTH:
					vsFeatures |= vsfShaderTime;
					fsFeatures |= fsfShaderTime | fsfGenSaw;
					break;
				case GF_INVERSE_SAWTOOTH:
					vsFeatures |= vsfShaderTime;
					fsFeatures |= fsfShaderTime | fsfGenInvSaw;
					break;
				case GF_NOISE:
					vsFeatures |= vsfShaderTime;
					fsFeatures |= fsfShaderTime | fsfGenNoise;
					break;
				}
				break;
			case TMOD_ROTATE:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenRotate;
				break;
			case TMOD_ENTITY_TRANSLATE:
				vsFeatures |= vsfTexCoord2;
				break;
			default:
				return qfalse;
			}
		}
		if( pStage->bundle[0].combinedImage ) {
			vsFeatures |= vsfShaderTime;
			fsFeatures |= fsfShaderTime;
		}

		switch( pStage->rgbGen ) {
		case CGEN_IDENTITY_LIGHTING:
			constantColor[i][0] =
			constantColor[i][1] =
			constantColor[i][2] = tr.identityLightByte;
			aGen = AGEN_IDENTITY;
			break;
		case CGEN_IDENTITY:
			constantColor[i][0] =
			constantColor[i][1] =
			constantColor[i][2] = 255;
			aGen = AGEN_IDENTITY;
			break;
		case CGEN_ENTITY:
			aGen = AGEN_ENTITY;
			vsFeatures |= vsfEntColor;
			break;
		case CGEN_ONE_MINUS_ENTITY:
			aGen = AGEN_ONE_MINUS_ENTITY;
			vsFeatures |= vsfEntColor;
			break;
		case CGEN_EXACT_VERTEX:
			aGen = AGEN_VERTEX;
			vsFeatures |= vsfColor;
			break;
		case CGEN_VERTEX:
			aGen = AGEN_VERTEX;
			vsFeatures |= vsfColor;
			break;
		case CGEN_ONE_MINUS_VERTEX:
			aGen = AGEN_ONE_MINUS_VERTEX;
			vsFeatures |= vsfColor;
			break;
		case CGEN_WAVEFORM:
			switch( pStage->rgbWave.func ) {
			case GF_NONE:
				return qfalse;
			case GF_SIN:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenSin;
				break;
			case GF_SQUARE:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenSquare;
				break;
			case GF_TRIANGLE:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenTri;
				break;
			case GF_SAWTOOTH:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenSaw;
				break;
			case GF_INVERSE_SAWTOOTH:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenInvSaw;
				break;
			case GF_NOISE:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenNoise;
				break;
			};
			aGen = AGEN_IDENTITY;
			break;
		case CGEN_LIGHTING_DIFFUSE:
			aGen = AGEN_IDENTITY;
			vsFeatures |= vsfEntLight | vsfLightDir | vsfNormal;
			if( r_perPixelLighting->integer ) {
				vsFeatures |= vsfCameraPos;
				fsFeatures |= fsfDiffuse | fsfLightDir | fsfNormal | fsfCameraPos | fsfGetLight;
			} else {
				fsFeatures |= fsfDiffuse;
			}
			break;
		case CGEN_FOG:
			aGen = AGEN_IDENTITY;
			return qfalse;
		case CGEN_CONST:
			constantColor[i][0] = pStage->constantColor[0];
			constantColor[i][1] = pStage->constantColor[1];
			constantColor[i][2] = pStage->constantColor[2];
			aGen = AGEN_IDENTITY;
			break;
		default:
			return qfalse;
		}

		if ( pStage->alphaGen == AGEN_SKIP )
			pStage->alphaGen = aGen;

		switch( pStage->alphaGen ) {
		case AGEN_IDENTITY:
			constantColor[i][3] = 255;
			break;
		case AGEN_ENTITY:
			vsFeatures |= vsfColor;
			break;
		case AGEN_ONE_MINUS_ENTITY:
			vsFeatures |= vsfColor;
			break;
		case AGEN_VERTEX:
			vsFeatures |= vsfColor;
			break;
		case AGEN_ONE_MINUS_VERTEX:
			vsFeatures |= vsfColor;
			break;
		case AGEN_LIGHTING_SPECULAR:
			vsFeatures |= vsfNormal | vsfLightDir | vsfCameraPos;
			if( r_perPixelLighting->integer )
				fsFeatures |= fsfNormal | fsfCameraPos | fsfLightDir | fsfGetLight;
			else
				fsFeatures |= fsfReflView | fsfSpecular;
			break;
		case AGEN_WAVEFORM:
			switch( pStage->alphaWave.func ) {
			case GF_NONE:
				return qfalse;
			case GF_SIN:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenSin;
				break;
			case GF_SQUARE:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenSquare;
				break;
			case GF_TRIANGLE:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenTri;
				break;
			case GF_SAWTOOTH:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenSaw;
				break;
			case GF_INVERSE_SAWTOOTH:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenInvSaw;
				break;
			case GF_NOISE:
				vsFeatures |= vsfShaderTime;
				fsFeatures |= fsfShaderTime | fsfGenNoise;
				break;
			};
			break;
		case AGEN_PORTAL:
			vsFeatures |= vsfCameraPos | vsfNormal;
			fsFeatures |= fsfCameraPos;
			break;
		case AGEN_CONST:
			constantColor[i][3] = pStage->constantColor[3];
			break;
		default:

			return qfalse;
		}
	}

	if( r_perPixelLighting->integer &&
	    shader.lightmapIndex != LIGHTMAP_2D &&
	    qglUniformBlockBinding ) {
		vsFeatures |= vsfFogNum | vsfCameraPos;
		fsFeatures |= fsfVertex | fsfCameraPos;
	}

	if( stages[0].bundle[0].isLightmap ) {
		j = 1;
	} else {
		j = 0;
	}
	if( r_perPixelLighting->integer &&
	    shader.lightmapIndex != LIGHTMAP_2D &&
	    stages[j].active &&
	    stages[j].bundle[0].tcGen == TCGEN_TEXTURE &&
	    stages[j].bundle[0].numTexMods == 0 &&
	    stages[j].bundle[0].image[0] &&
	    *stages[j].bundle[0].image[0]->imgName > '/' ) {
		vsFeatures |= vsfNormal | vsfLightDir | vsfCameraPos;
		fsFeatures |= fsfNormal | fsfVertex | fsfCameraPos | fsfLightDir | fsfGetLight;

		// check if a normal/bump map exists
		if( i < MAX_SHADER_STAGES ) {
			char name[MAX_QPATH];
			COM_StripExtension( stages[j].bundle[0].image[0]->imgName, name, sizeof(name) );
			strcat( name, "_nm" );
			stages[i].bundle[0].image[0] = R_FindHeightMapFile( name, qtrue, GL_REPEAT );
			if( stages[i].bundle[0].image[0] ) {
				normalStage = i++;
				stages[normalStage].active = qtrue;
				stages[normalStage].bundle[0].tcGen = TCGEN_TEXTURE;
				fsFeatures |= fsfTangents;
			}
		}
		// check if a material map exists
		if( i < MAX_SHADER_STAGES ) {
			char name[MAX_QPATH];
			COM_StripExtension( stages[j].bundle[0].image[0]->imgName, name, sizeof(name) );
			strcat( name, "_mat" );
			stages[i].bundle[0].image[0] = R_FindImageFile( name, qtrue, qtrue, GL_REPEAT );
			if( stages[i].bundle[0].image[0] ) {
				materialStage = i++;
				stages[materialStage].active = qtrue;
				stages[materialStage].bundle[0].tcGen = TCGEN_TEXTURE;
			}
		}
	}

	// *** assemble shader fragments ***
	// version pragma
	if ( GLSLversion < 0x010a ) {
		return qfalse;
	}
	//VSText("/*");VSText(shader.name);VSText("*/\n");
	VSText( VSHeader );
	if( (fsFeatures & fsfTangents) &&
	    GLSLversion <= 0x0132 ) {
		if ( qglProgramParameteriEXT ) {
			// compute tangets in geometry shader for higher precision
			VSText("#extension GL_EXT_geometry_shader4 : enable\n"
			       "#extension GL_EXT_gpu_shader4 : enable\n");
			GSText( GSHeader );
			FSText("#extension GL_EXT_geometry_shader4 : enable\n"
			       "#extension GL_EXT_gpu_shader4 : enable\n");
		} else {
			VSText("#extension GL_EXT_gpu_shader4 : enable\n");
			FSText("#extension GL_EXT_gpu_shader4 : enable\n");
		}
	}
	FSText( FSHeader );

	if( r_perPixelLighting->integer &&
	    shader.lightmapIndex != LIGHTMAP_2D &&
	    qglUniformBlockBinding ) {
		VSText("#extension GL_ARB_uniform_buffer_object : enable\n");
		FSText("#extension GL_ARB_uniform_buffer_object : enable\n"
		       "\n"
		       "#define SHADER_DLIGHTS\n");
	}

	FSText("const vec3 constants = vec3( 0.0, 1.0, ");
	FSConst("%f", tr.identityLight );
	FSText(" );\n\n");
	
	// VS inputs
	VSText("// IN(vec4 aVertex);\n"
	       "#define aVertex gl_Vertex\n");
	if( vsFeatures & vsfTexCoord ) {
		VSText("// IN(vec4 aTexCoord);\n"
		       "#define aTexCoord gl_MultiTexCoord0\n");
	}
	if( vsFeatures & (vsfTexCoord2 | vsfFogNum) ) {
		VSText("// IN(vec4 aTexCoord2);\n"
		       "#define aTexCoord2 gl_MultiTexCoord1\n");
	}
	if( vsFeatures & vsfColor ) {
		VSText("// IN(vec4 aColor);\n"
		       "#define aColor gl_Color\n");
	}
	if( vsFeatures & vsfEntColor ) {
		VSText("IN(vec4 aEntColor);\n");
	}
	if( shader.lightmapIndex == LIGHTMAP_MD3 ||
	    (vsFeatures & vsfShaderTime) ) {
		VSText("IN(vec4 aTimes);\n");
	}
	
	if( shader.lightmapIndex != LIGHTMAP_MD3 ) {
		if( vsFeatures & vsfNormal ) {
			VSText("// IN(vec3 aNormal);\n"
			       "#define aNormal gl_Normal\n");
		}
	}
	VSText("IN(vec4 aTransX);\n"
	       "IN(vec4 aTransY);\n"
	       "IN(vec4 aTransZ);\n");
	if( vsFeatures & vsfEntLight ) {
		VSText("IN(vec3 aAmbientLight);\n");
		VSText("IN(vec3 aDirectedLight);\n");
	}
	if( vsFeatures & vsfLightDir ) {
		VSText("IN(vec4 aLightDir);\n");
	}
	if( vsFeatures & vsfCameraPos ) {
		VSText("IN(vec3 aCameraPos);\n");
	}
	
	// VS outputs / FS inputs
#define addVarying( qual, type, name )					\
	VSText("OUT(" #qual ", " #type " v" #name ");\n");		\
	if( GSidx ) {							\
		GSText("IN(" #qual ", " #type " v" #name "[]);\n");	\
		GSText("OUT(" #qual ", " #type " g" #name ");\n");	\
		FSText("#define v" #name " g" #name "\n");		\
	}								\
	FSText("IN(" #qual ", " #type " v" #name ");\n");

	if( vsFeatures & vsfTexCoord ) {
		addVarying( smooth, vec4, TexCoord );
	}
	if( vsFeatures & vsfTexCoord2 ) {
		addVarying( flat, vec2, EntTexCoord );
	}
	if( vsFeatures & vsfColor ) {
		addVarying( smooth, vec4, Color );
	}
	if( vsFeatures & vsfEntColor ) {
		addVarying( flat, vec4, EntColor );
	}
	if( fsFeatures & fsfVertex ) {
		addVarying( smooth, vec3, Vertex );
	}
	if( fsFeatures & fsfNormal ) {
		addVarying( smooth, vec3, Normal );
	}
	if( fsFeatures & fsfLightDir ) {
		addVarying( flat, vec4, LightDir );
	}
	if( r_perPixelLighting->integer ) {
		if( fsFeatures & fsfDiffuse ) {
			// per pixel diffuse light
			addVarying( flat, vec3, AmbientLight );
			addVarying( flat, vec3, DirectedLight );
		}
	} else {
		if( fsFeatures & fsfDiffuse ) {
			// interpolated diffuse
			addVarying( smooth, vec3, Diffuse );
		}
		if( fsFeatures & fsfSpecular ) {
			// interpolated specular
			addVarying( smooth, float, Specular );
		}
	}
	if( fsFeatures & fsfReflView ) {
		if( r_perPixelLighting->integer ) {
			// calculated in fragment shader
		} else {
			// interpolated
			addVarying( smooth, vec3, ReflView );
		}
	}
	if( fsFeatures & fsfShaderTime ) {
		addVarying( flat, float, Shadertime );
	}
	if( fsFeatures & fsfCameraPos ) {
		addVarying( smooth, vec3, CameraPos );
	}
	if( vsFeatures & vsfFogNum ) {
		addVarying( flat, float, FogNum );
	}
	if( GSidx && (fsFeatures & fsfTangents) ) {
		// pass tangent vectors only from GS to FS
		GSText("OUT(smooth, vec3 gUTangent);\n"
		       "OUT(smooth, vec3 gVTangent);\n");
		FSText("#define vUTangent gUTangent\n"
		       "#define vVTangent gVTangent\n"
		       "IN(smooth, vec3 vUTangent);\n"
		       "IN(smooth, vec3 vVTangent);\n");
	}

	// uniforms
	if( shader.lightmapIndex == LIGHTMAP_MD3 ) {
		if( qglTexBufferEXT ) {
			VSText("uniform samplerBuffer texData;\n");
		} else {
			VSText("uniform sampler2D texData;\n");
		}
	}
	for( i = 0; i < MAX_SHADER_STAGES; i++ ) {
		shaderStage_t *pStage = &stages[i];
		
		if( !pStage->active )
			break;
		
		for( j = 0; j < i; j++ ) {
			if( pStage->bundle[0].image[0] == stages[j].bundle[0].image[0] )
				break;
		}
		if( j < i ) {
			texIndex[i] = j;
		} else {
			texIndex[i] = i;
			FSText("uniform sampler2D ");
			FSText(GLSLTexNames[i]);
			FSText(";\n");
		}
	}
	if( normalStage >= 0 ) {
		FSText("const int HMLevels = ");
		FSConst("%d", stages[normalStage].bundle[0].image[0]->maxMipLevel);
		FSText(";\n"
		       "const float HMSize = ");
		FSConst("%f", (float)stages[normalStage].bundle[0].image[0]->uploadWidth);
		FSText(";\n"
		       "\n");
	}
	if( r_perPixelLighting->integer &&
	    shader.lightmapIndex != LIGHTMAP_2D &&
	    qglUniformBlockBinding ) {
		FSText("layout( shared ) uniform dLights {\n"
		       "  vec4 dlSpheres[128+127];\n"  // center, radius
		       "  vec3 dlColors[128];\n"
		       "  int  dlLinks[128+127];\n"
		       "  int  dlNum;\n"
		       "  vec2 dlDebug;\n"
		       "};\n"
		       "\n"
		       "layout( shared ) uniform fogs {\n"
		       "  vec3 lightGridScale;\n"
		       "  vec3 lightGridOffset;\n"
		       "  vec4 fogColors[256];\n"
		       "  vec4 fogPlanes[256];\n"
		       "};\n"
		       "\n"
		       "uniform sampler3D texLightGrid;\n"
		       "\n");
	}
	
	// other global variables
	if( (vsFeatures & vsfLightDir) && !r_perPixelLighting->integer ) {
		VSText("vec3 lightDir;\n"
		       "#define ambientLight aAmbientLight\n"
		       "#define directedLight aDirectedLight\n");
	}
	if( fsFeatures & fsfGetLight ) {
		FSText("vec3 lightDir;\n"
		       "vec3 ambientLight;\n"
		       "vec3 directedLight;\n");
	}

	// functions
	VSText("\n"
	       "vec3 transform3(vec3 vector) {\n"
	       "  return vec3( dot( aTransX.xyz, vector ),\n"
	       "               dot( aTransY.xyz, vector ),\n"
	       "               dot( aTransZ.xyz, vector ) );\n"
	       "}\n"
	       "vec3 transform4(vec4 point) {\n"
	       "  return vec3( dot( aTransX, point ),\n"
	       "               dot( aTransY, point ),\n"
	       "               dot( aTransZ, point ) );\n"
	       "}\n");

	if( fsFeatures & fsfGetLight ) {
		FSText( GLSLfnGetLight );
	}

	if( vsFeatures & vsfGenSin ) {
		VSText( GLSLfnGenSin );
	}
	if( fsFeatures & fsfGenSin ) {
		FSText( GLSLfnGenSin );
	}
	if( vsFeatures & vsfGenSquare ) {
		VSText( GLSLfnGenSquare );
	}
	if( fsFeatures & fsfGenSquare ) {
		FSText( GLSLfnGenSquare );
	}
	if( vsFeatures & vsfGenTri ) {
		VSText( GLSLfnGenTriangle );
	}
	if( fsFeatures & fsfGenTri ) {
		FSText( GLSLfnGenTriangle );
	}
	if( vsFeatures & vsfGenSaw ) {
		VSText( GLSLfnGenSawtooth );
	}
	if( fsFeatures & fsfGenSaw ) {
		FSText( GLSLfnGenSawtooth );
	}
	if( vsFeatures & vsfGenInvSaw ) {
		VSText( GLSLfnGenInverseSawtooth );
	}
	if( fsFeatures & fsfGenInvSaw ) {
		FSText( GLSLfnGenInverseSawtooth );
	}
	if( vsFeatures & vsfGenNoise ) {
		VSText("float genFuncNoise(in float x) {\n"
		       "  //return noise1(x);\n"
		       "  vec2 xi = floor(vec2(x, x + 1.0));\n"
		       "  vec2 xf = x - xi;\n"
		       "  vec2 grad = 4.0 * fract((xi * 34.0 + 1.0) * xi / 289.0) - 2.0;\n"
		       "  grad *= xf;\n"
		       "  return mix(grad.x, grad.y, xf.x*xf.x*xf.x*(xf.x*(xf.x*6.0-15.0)+10.0));\n"
		       "}\n\n");
	}
	if( fsFeatures & fsfGenNoise ) {
		FSText("float genFuncNoise(in float x) {\n"
		       "  //return noise1(x);\n"
		       "  vec2 xi = floor(vec2(x, x + 1.0));\n"
		       "  vec2 xf = x - xi;\n"
		       "  vec2 grad = 4.0 * fract((xi * 34.0 + 1.0) * xi / 289.0) - 2.0;\n"
		       "  grad *= xf;\n"
		       "  return mix(grad.x, grad.y, xf.x*xf.x*xf.x*(xf.x*(xf.x*6.0-15.0)+10.0));\n"
		       "}\n\n");
	}
	if( fsFeatures & fsfGenRotate ) {
		FSText("mat2 genFuncRotate(in float x) {\n"
		       "  vec2 sincos = sin(6.283185308 / 360.0 * vec2(x, x + 90.0));\n"
		       "  return mat2(sincos.y, -sincos.x, sincos.x, sincos.y);\n"
		       "}\n\n");
	}
	if( normalStage >= 0 && r_parallax->integer ) {
		if( !glGlobals.gpuShader4 ) {
			// linear stepping
			FSText("vec3 intersectHeightMap(const vec3 start,\n"
			       "                        const vec3 dir) {\n"
			       "  vec3  tracePos = start;\n"
			       "  float diff0, diff1 = 1.0;\n"
			       "  float steps = 64.0; //HMSize * length(dir.xy);\n"
			       "  vec3  step = dir / steps;\n"
			       "  while( diff1 > 0.0 ) {\n"
			       "    diff0 = diff1;\n"
			       "    diff1 = tracePos.z - tex2DBias(");
			FSText(GLSLTexNames[texIndex[normalStage]]);
			FSText(", tracePos.xy, -99.0).a;\n"
			       "    tracePos += step;\n"
			       "  }\n"
			       "  return tracePos + (diff1 / (diff0 - diff1) - 1.0) * step;\n"
			       "}\n"
			       "\n");
		} else {
			FSText("vec3 intersectHeightMap(const vec3 start,\n"
			       "                        const vec3 dir) {\n"
			       "  float lod = log2(max(length(dFdx(start.xy)),\n"
			       "                       length(dFdy(start.xy))));\n"
			       "  vec3  tracePos = start;\n"
			       "  float diff0, diff1 = 1.0;\n"
			       "  float steps = 64.0;\n"
			       "  vec3  step = dir / steps;\n"
			       "  while( diff1 > 0.0 ) {\n"
			       "    diff0 = diff1;\n"
			       "    diff1 = tracePos.z - tex2DLod(");
			FSText(GLSLTexNames[texIndex[normalStage]]);
			FSText(", tracePos.xy, lod).a;\n"
			       "    tracePos += step;\n"
			       "  }\n"
			       "  return tracePos + (diff1 / (diff0 - diff1) - 1.0) * step;\n"
			       "}\n"
			       "\n");
		}
	}
	if( shader.lightmapIndex == LIGHTMAP_MD3 ) {
		if( qglTexBufferEXT && ( vsFeatures & vsfNormal ) ) {
			VSText("vec4 fetchVertex(const float frameNo, const vec2 offset,\n"
			       "                 out vec4 normal) {\n"
			       "  int  tc = int(floor(frameNo + offset));\n"
			       "  vec4 data = texFetch(texData, tc);\n"
			       "  vec4 lo = fract(data);\n"
			       "  vec4 hi = floor(data);\n"
			       "  normal = vec4((hi.xyz - 128.0) / 127.0, 0.0);\n"
			       "  return vec4(lo.xyz * 1024.0 - 512.0,\n"
			       "              1.0);\n"
			       "}\n\n");
		} else if( qglTexBufferEXT ) {
			VSText("vec4 fetchVertex(const float frameNo, const vec2 offset) {\n"
			       "  int  tc = int(floor(frameNo + offset));\n"
			       "  vec4 data = texFetch(texData, tc);\n"
			       "  vec4 lo = fract(data);\n"
			       "  return vec4(lo.xyz * 1024.0 - 512.0,\n"
			       "              1.0);\n"
			       "}\n\n");
		} else if( vsFeatures & vsfNormal ) {
			VSText("vec4 fetchVertex(const float frameNo, const vec2 offset,\n"
			       "                 out vec4 normal) {\n"
			       "  vec2 tc = vec2(fract(frameNo), floor(frameNo)/1024.0) + offset;\n"
			       "  vec4 data = tex2D(texData, tc);\n"
			       "  vec4 lo = fract(data);\n"
			       "  vec4 hi = floor(data);\n"
			       "  normal = vec4((hi.xyz - 128.0) / 127.0, 0.0);\n"
			       "  return vec4(lo.xyz * 1024.0 - 512.0,\n"
			       "              1.0);\n"
			       "}\n\n");
		} else {
			VSText("vec4 fetchVertex(const float frameNo, const vec2 offset) {\n"
			       "  vec2 tc = vec2(fract(frameNo), floor(frameNo)/1024.0) + offset;\n"
			       "  vec4 data = tex2D(texData, tc);\n"
			       "  vec4 lo = fract(data);\n"
			       "  return vec4(lo.xyz * 1024.0 - 512.0,\n"
			       "              1.0);\n"
			       "}\n\n");
		}
	}
	
	// main vertex shader
	VSText("\n"
	       "void main() {\n"
	       "  vec4 vertex;\n");
	if( vsFeatures & vsfNormal ) {
		VSText("  vec4 normal;\n");
	}
	if( shader.lightmapIndex == LIGHTMAP_MD3 ) {
		// interpolate position and normal from two frames
		if( vsFeatures & vsfNormal ) {
			VSText("  vec4 normal1, normal2;\n"
			       "  vertex = mix(fetchVertex(aTimes.z, aVertex.zw, normal1),\n"
			       "               fetchVertex(aTimes.w, aVertex.zw, normal2),\n"
			       "               aTimes.y);\n"
			       "  normal = normalize(mix(normal1, normal2, aTimes.y));\n");
		} else {
			VSText("  vertex = mix(fetchVertex(aTimes.z, aVertex.zw),\n"
			       "               fetchVertex(aTimes.w, aVertex.zw),\n"
			       "               aTimes.y);\n");
		}
		if( vsFeatures & vsfTexCoord ) {
			VSText("  vTexCoord = aVertex.xyxy;\n");
		}
		if( vsFeatures & vsfTexCoord2 ) {
			VSText("  vEntTexCoord = aTexCoord2.xy;\n");
		}
		if( vsFeatures & vsfColor ) {
			VSText("  vColor = aColor;\n");
		}
		if( vsFeatures & vsfEntColor ) {
			VSText("  vEntColor = aEntColor;\n");
		}
	} else {
		VSText("  \n"
		       "  vertex = vec4(aVertex.xyz, 1.0);\n");
		if( vsFeatures & vsfNormal ) {
			VSText("  normal = vec4(aNormal, 0.0);\n");
		}
		if( vsFeatures & vsfTexCoord ) {
			VSText("  vTexCoord = aTexCoord;\n");
		}
		if( vsFeatures & vsfTexCoord2 ) {
			VSText("  vEntTexCoord = aTexCoord2.xy;\n");
		}
		if( vsFeatures & vsfColor ) {
			VSText("  vColor = aColor;\n");
		}
		if( vsFeatures & vsfEntColor ) {
			VSText("  vEntColor = aEntColor;\n");
		}
	}
	if( fsFeatures & fsfShaderTime ) {
		VSText("  \n"
		       "  vShadertime = aTimes.x;\n");
	}
	if( vsFeatures & vsfFogNum ) {
		if( shader.lightmapIndex == LIGHTMAP_MD3 )
			VSText("  vFogNum = aTexCoord2.z;\n");
		else
			VSText("  vFogNum = aVertex.w + aTexCoord2.z;\n");
	}
	
	// apply deforms
	for( i = 0; i < shader.numDeforms; i++ ) {
		switch ( shader.deforms[i].deformation ) {
		case DEFORM_NONE:
			break;
		case DEFORM_WAVE:
			VSText("  \n"
			       "  vertex += (");
			VSConst("%f", shader.deforms[i].deformationWave.base);
			VSText(" + ");
			VSConst("%f", shader.deforms[i].deformationWave.amplitude);
			switch( shader.deforms[i].deformationWave.func ) {
			case GF_NONE:
				return qfalse;
			case GF_SIN:
				VSText(" * genFuncSin(");
				break;
			case GF_SQUARE:
				VSText(" * genFuncSquare(");
				break;
			case GF_TRIANGLE:
				VSText(" * genFuncTriangle(");
				break;
			case GF_SAWTOOTH:
				VSText(" * genFuncSawtooth(");
				break;
			case GF_INVERSE_SAWTOOTH:
				VSText(" * genFuncInverseSawtooth(");
				break;
			case GF_NOISE:
				VSText(" * genFuncNoise(");
				break;
			}
			VSConst("%f", shader.deforms[i].deformationWave.phase);
			VSText(" + dot(vertex.xyz, vec3(");
			VSConst("%f", shader.deforms[i].deformationSpread);
			VSText(")) + ");
			VSConst("%f", shader.deforms[i].deformationWave.frequency);
			VSText(" * aTimes.x)) * normal;\n");
			break;
		case DEFORM_NORMALS:
			VSText("  \n"
			       "  normal.xyz = normalize(normal.xyz + 0.98*noise3(vec4(vertex.xyz, aTimes.x * ");
			VSConst("%f", shader.deforms[i].deformationWave.frequency);
			VSText(")));\n");
			break;
		case DEFORM_BULGE:
			VSText("  \n"
			       "  vertex += (");
			VSConst("%f", shader.deforms[i].bulgeHeight);
			VSText(" * sin(aTexCoord.x * ");
			VSConst("%f", shader.deforms[i].bulgeWidth);
			VSText(" + aTimes.x * ");
			VSConst("%f", shader.deforms[i].bulgeSpeed * 0.001f);
			VSText(")) * normal;\n");
			break;
		case DEFORM_MOVE:
			VSText("  \n"
			       "  vertex.xyz += (");
			VSConst("%f", shader.deforms[i].deformationWave.base);
			VSText(" + ");
			VSConst("%f", shader.deforms[i].deformationWave.amplitude);
			switch( shader.deforms[i].deformationWave.func ) {
			case GF_NONE:
				return qfalse;
			case GF_SIN:
				VSText(" * genFuncSin(");
				break;
			case GF_SQUARE:
				VSText(" * genFuncSquare(");
				break;
			case GF_TRIANGLE:
				VSText(" * genFuncTriangle(");
				break;
			case GF_SAWTOOTH:
				VSText(" * genFuncSawtooth(");
				break;
			case GF_INVERSE_SAWTOOTH:
				VSText(" * genFuncInverseSawtooth(");
				break;
			case GF_NOISE:
				VSText(" * genFuncNoise(");
				break;
			}
			VSConst("%f", shader.deforms[i].deformationWave.phase);
			VSText(" + ");
			VSConst("%f", shader.deforms[i].deformationWave.frequency);
			VSText(" * aTimes.x)) * vec3(");
			VSConst("%f", shader.deforms[i].moveVector[0]);
			VSText(", ");
			VSConst("%f", shader.deforms[i].moveVector[1]);
			VSText(", ");
			VSConst("%f", shader.deforms[i].moveVector[2]);
			VSText(");\n");
			break;
		default:
			return qfalse;
		}
	}
	VSText("  vertex = vec4(transform4(vertex), 1.0);\n");
	if( vsFeatures & vsfNormal ) {
		VSText("  normal = vec4( transform3( normal.xyz ), 0.0 );\n");
	}
	if( fsFeatures & fsfVertex ) {
		VSText("  vVertex = vertex.xyz;\n");
	}
	if( fsFeatures & fsfNormal ) {
		VSText("  \n"
		       "  vNormal = normal.xyz;\n");
	}
	if( fsFeatures & fsfCameraPos ) {
		VSText("  vCameraPos = vertex.xyz - aCameraPos.xyz;\n");
	}
	if( fsFeatures & fsfLightDir ) {
		VSText("  vec3 lightDir = normalize(transform3(aLightDir.xyz));\n");
		if( r_perPixelLighting->integer ) {
			VSText("  vLightDir = vec4(lightDir, aLightDir.w);\n" );
		}
	}
	
	if( fsFeatures & fsfDiffuse ) {
		if( r_perPixelLighting->integer ) {
			VSText("  \n"
			       "  vAmbientLight  = aAmbientLight;\n"
			       "  vDirectedLight = aDirectedLight;\n");
		} else {
			VSText("  \n"
			       "  float diffuse = max(0.0, dot(normal.xyz, lightDir.xyz));\n"
			       "  vDiffuse = clamp(aAmbientLight + diffuse * aDirectedLight, 0.0, 1.0);\n");
		}
	}
	if( fsFeatures & fsfReflView ) {
		if( r_perPixelLighting->integer ) {
		} else {
			VSText("  \n"
			       "  vReflView = reflect(vertex.xyz - aCameraPos, normalize(normal.xyz));\n");
		}
	}
	if( fsFeatures & fsfSpecular ) {
		if( r_perPixelLighting->integer ) {
		} else {
			VSText("  vSpecular = max(0.0, 4.0 * dot(lightDir.xyz, normalize(vReflView)) - 3.0);\n"
			       "  vSpecular *= vSpecular; vSpecular *= vSpecular; vSpecular *= vSpecular;\n");
		}
	}
	VSText("  gl_Position = gl_ModelViewProjectionMatrix * vertex;\n"
	       "}\n");
	
	// main geometry shader
	if( GSidx ) {
		GSText("\n"
		       "void main() {\n"
		       "  vec3 uTangent, vTangent;\n"
		       "  vec3 dpx = vVertex[2] - vVertex[0];\n"
		       "  vec3 dpy = vVertex[1] - vVertex[0];\n"
		       "  vec2 dtx = vTexCoord[2].xy - vTexCoord[0].xy;\n"
		       "  vec2 dty = vTexCoord[1].xy - vTexCoord[0].xy;\n"
		       "  float scale = sign(dty.y*dtx.x - dtx.y*dty.x);\n"
		       "  vec3 normal = cross( dpx, dpy );\n"
		       "  uTangent =  dpx * dty.y - dpy * dtx.y;\n"
		       "  vTangent = -dpx * dty.x + dpy * dtx.x;\n"
		       "  uTangent -= normal * dot( uTangent, normal );\n"
		       "  vTangent -= normal * dot( vTangent, normal );\n"
		       "  uTangent = normalize( scale*uTangent );\n"
		       "  vTangent = normalize( scale*vTangent );\n"
		       "  int i;\n"
		       "  for( i = 0; i < 3; i++ ) {\n"
		       "    gTexCoord = vTexCoord[i];\n");
		if( vsFeatures & vsfTexCoord2 ) {
			GSText("    gEntTexCoord = vEntTexCoord[i];\n");
		}
		if( vsFeatures & vsfColor ) {
			GSText("    gColor = vColor[i];\n");
		}
		if( vsFeatures & vsfEntColor ) {
			GSText("    gEntColor = vEntColor[i];\n");
		}
		if( fsFeatures & fsfVertex ) {
			GSText("    gVertex = vVertex[i];\n");
		}
		if( fsFeatures & fsfNormal ) {
			GSText("    gNormal = vNormal[i];\n");
		}
		if( fsFeatures & fsfCameraPos ) {
			GSText("    gCameraPos = vCameraPos[i];\n");
		}
		if( fsFeatures & fsfLightDir ) {
			if( r_perPixelLighting->integer ) {
				GSText("    gLightDir = vLightDir[i];\n" );
			}
		}
		if( fsFeatures & fsfDiffuse ) {
			if( r_perPixelLighting->integer ) {
				GSText("    gAmbientLight  = vAmbientLight[i];\n"
				       "    gDirectedLight = vDirectedLight[i];\n");
			} else {
				GSText("    gDiffuse = vDiffuse[i];\n");
			}
		}
		if( fsFeatures & fsfReflView ) {
			if( r_perPixelLighting->integer ) {
			} else {
				GSText("    gReflView = vReflView[i];\n");
			}
		}
		if( fsFeatures & fsfSpecular ) {
			if( r_perPixelLighting->integer ) {
			} else {
				GSText("    gSpecular = vSpecular[i];\n");
			}
		}
		if( fsFeatures & fsfShaderTime ) {
			GSText("    gShadertime = vShadertime[i];\n");
		}
		if( vsFeatures & vsfFogNum ) {
			GSText("    gFogNum = vFogNum[i];\n");
		}
		GSText("    gUTangent = uTangent;\n"
		       "    gVTangent = vTangent;\n"
		       "    gl_Position = gl_PositionIn[i];\n"
		       "    EmitVertex();\n"
		       "  }\n"
		       "  EndPrimitive();\n"
		       "}\n");
	}

	// main fragment shader
	if( qglBindFragDataLocationIndexed ) {
		FSText("OUT(vec3 dstColorMult);\n"
		       "OUT(vec3 dstColorAdd);\n"
		       "\n"
		       "void main() {\n"
		       "  vec4  srcColor = constants.xxxx;\n"
		       "  vec3  tmpColor;\n"
		       "  vec2  tc;\n"
		       "  vec4  genColor;\n"
		       "  dstColorMult = constants.yyy;\n"
		       "  dstColorAdd = constants.xxx;\n");
	} else {
		FSText("void main() {\n"
		       "  vec4  srcColor = constants.xxxx;\n"
		       "  vec3  tmpColor;\n"
		       "  vec2  tc;\n"
		       "  vec4  genColor;\n"
		       "  vec3  dstColorMult = constants.yyy;\n"
		       "  vec3  dstColorAdd = constants.xxx;\n");
	}
	MultIsZero = qfalse;
	AddIsZero = qtrue;
	if( vsFeatures & vsfTexCoord ) {
		FSText("  vec2  baseTC = vTexCoord.st;\n");
	}
	if( fsFeatures & fsfVertex ) {
		FSText("  vec3 vertex = vVertex.xyz;\n");
	} else {
		FSText("  vec3 vertex = vec3(0.0);\n");
	}

	if( fsFeatures & fsfNormal ) {
		FSText("  vec3 normal = normalize(vNormal);\n");
	}
	if( fsFeatures & fsfTangents ) {
		if( !GSidx ) {
			// compute from derivates, may be unprecise
			FSText("  vec3 dpx = dFdx(vVertex);\n"
			       "  vec3 dpy = dFdy(vVertex);\n"
			       "  vec2 dtx = dFdx(vTexCoord.xy);\n"
			       "  vec2 dty = dFdy(vTexCoord.xy);\n"
			       "  float scale = sign(dty.y*dtx.x - dtx.y*dty.x);\n"
			       "  vec3 uTangent =  dpx * dty.y - dpy * dtx.y;\n"
			       "  vec3 vTangent = -dpx * dty.x + dpy * dtx.x;\n"
			       "  uTangent -= normal * dot( uTangent, normal );\n"
			       "  vTangent -= normal * dot( vTangent, normal );\n"
			       "  uTangent = normalize( scale*uTangent );\n"
			       "  vTangent = normalize( scale*vTangent );\n"
			       // calculate non-interpolated normal for testing			       
			       // "  normal = scale * cross( uTangent, vTangent );\n"
				);
		} else {
			// computed in geometry shader
			FSText("  vec3 uTangent = normalize( vUTangent );\n"
			       "  vec3 vTangent = normalize( vVTangent );\n");
		}
	}
	if( normalStage >= 0 ) {
		// parallax calculation
		if( r_parallax->integer ) {
			// implementation is in intersectHeightMap function

			// z coord is upscaled, change to make parallax effect stronger
			FSText("  vec3 traceVec = vec3(dot(vCameraPos, uTangent),\n"
			       "                       dot(vCameraPos, vTangent),\n"
			       "                       dot(vCameraPos, normal) * 8.0);\n"
			       "  traceVec /= -traceVec.z;\n"
			       "  vec3 tracePos = intersectHeightMap(vec3(baseTC, 1.0), traceVec);\n"
			       "  baseTC = tracePos.xy;\n");
		} else {
			// no parallax
			FSText("  vec3 tracePos = vec3(baseTC, 1.0);\n");
		}
	}
	
	// normal mapping
	if( fsFeatures & fsfNormal ) {
		if( normalStage >= 0 ) {
			FSText("  vec3 n = tex2D(");
			FSText(GLSLTexNames[texIndex[normalStage]]);
			FSText(", baseTC).xyz * 2.0 - 1.0;\n"
			       "  normal = normalize(n.x * uTangent + n.y * vTangent + n.z * normal);\n");
		}
	}

	// compute light
	if( fsFeatures & fsfGetLight ) {
		// getLight requires ambient and directed lights
		if( r_perPixelLighting->integer &&
		    shader.lightmapIndex != LIGHTMAP_2D &&
		    qglUniformBlockBinding ) {
			FSText("  if( lightGridScale.x > 0.0 ) {\n"
			       "    vec3 vert = vertex.xyz * lightGridScale - lightGridOffset;\n"
			       "    vec4 lg1 = tex3D(texLightGrid, vert);\n"
			       "    vec4 lg2 = tex3D(texLightGrid, vec3(vert.xy, 1.0 - vert.z));\n"
			       "    ambientLight = lg1.rgb;\n"
			       "    directedLight = lg2.rgb;\n"
			       "    lightDir.x = 2.0 * lg1.a - 1.0;\n"
			       "    lightDir.y = 2.0 * lg2.a - 1.0;\n"
			       "    lightDir.z = 1.0 - abs(lightDir.x) - abs(lightDir.y);\n"
			       "    if( lightDir.z < 0.0 ) {\n"
			       "      lightDir.xy = sign(lightDir.xy) - lightDir.xy;\n"
			       "    }\n"
			       "    lightDir = normalize(lightDir);\n"
			       "  } else {\n"
			       "    lightDir = vNormal;\n"
			       "    ambientLight = constants.xxx;\n"
			       "    directedLight = constants.xxx;\n"
			       "  }\n");
		} else {
			FSText("  lightDir = vNormal;\n"
			       "  ambientLight = constants.xxx;\n"
			       "  directedLight = constants.xxx;\n");
		}

		if( tr.hasDeluxemaps && lightmapStage >= 0 ) {
			FSText("  lightDir = normalize(tex2D(");
			FSText(GLSLTexNames[texIndex[lightmapStage]]);
			FSText(", vec2(vTexCoord.p + vLightDir.w, vTexCoord.q)).xyz * 2.0 - 1.0);\n");
		} else if( fsFeatures & fsfLightDir ) {
			FSText("  lightDir = normalize(vLightDir.xyz);\n");
		}
		if( lightmapStage >= 0 ) {
			FSText("  ambientLight = tex2D(");
			FSText(GLSLTexNames[texIndex[lightmapStage]]);
			FSText(", vTexCoord.pq).xyz;\n");
		} else if ( fsFeatures & fsfDiffuse ) {
			FSText("  ambientLight = vAmbientLight.xyz;\n"
			       "  directedLight = vDirectedLight.xyz;\n");
		} else if ( vsFeatures & vsfColor ){
			FSText("  ambientLight = vec3(vColor.xyz);\n");
		}

		// get material properties
		if( materialStage >= 0 ) {
			FSText("  vec4 material = tex2D(");
			FSText(GLSLTexNames[texIndex[materialStage]]);
			FSText(", baseTC);\n"
			       "  float VdotN = max(0.0, dot(normalize(vCameraPos), normal));\n"
			       "  float opacity = fresnel(material.w, VdotN);\n");
		} else {
			FSText("  vec4 material = vec4(0.5);\n"
			       "  float opacity = 0.0;\n");
		}
		FSText("  vec3 diffuse;\n"
		       "  vec4 specular;\n"
		       "  getLight(material, vertex.xyz, normal, normalize(-vCameraPos), diffuse, specular);\n");
	}
	if( fsFeatures & fsfReflView ) {
		if( r_perPixelLighting->integer ) {
			FSText("  vec3 reflView = normalize(reflect(vCameraPos, normal));\n");
		} else {
			FSText("  vec3 reflView = normalize(vReflView);\n");
		}
	}
	if( fsFeatures & fsfSpecular ) {
		if( fsFeatures & fsfGetLight ) {
		} else {
			FSText("  vec4 specular = vec4(vSpecular);\n");
		}
	}

	for( i = 0; i < MAX_SHADER_STAGES; i++ ) {
		shaderStage_t *pStage = &stages[i];
		qboolean	lightBlend = qfalse;

		if( !pStage->active || i == normalStage || i == materialStage )
			break;

		srcBlend = pStage->stateBits & GLS_SRCBLEND_BITS;
		dstBlend = pStage->stateBits & GLS_DSTBLEND_BITS;

		// continue alpha test ?
		if( aTestStart >= 0 && !((pStage->stateBits & GLS_DEPTHFUNC_BITS) == GLS_DEPTHFUNC_EQUAL && aTestStart == 0 ) ) {
			FSText("  }\n");
			aTestStart = -1;
		}

		if( pStage->bundle[0].image[0] != tr.whiteImage &&
		    pStage->bundle[0].image[0] != tr.identityLightImage ) {
			switch( pStage->bundle[0].tcGen ) {
			case TCGEN_IDENTITY:
				FSText("  tc = constants.xx;\n");
				break;
			case TCGEN_LIGHTMAP:
				if( pStage->bundle[0].isLightmap )
					FSText("  tc = vTexCoord.pq;\n");
				else {
					FSText("  tc = vTexCoord.pq * vec2(");
					FSConst("%f", (float)tr.lightmapWidth);
					FSText(", ");
					FSConst("%f", (float)tr.lightmapHeight);
					FSText(");\n");
				}
				break;
			case TCGEN_TEXTURE:
				FSText("  tc = baseTC;\n");
				break;
			case TCGEN_ENVIRONMENT_MAPPED:
				FSText("  tc = vec2(0.5) + 0.5 * normalize(reflView).yz;\n");
				break;
			case TCGEN_FOG:
				return qfalse;
			case TCGEN_VECTOR:
				FSText("  tc = vec2(dot(vVertex, vec3(");
				FSConst("%f", pStage->bundle[0].tcGenVectors[0][0]);
				FSText(", ");
				FSConst("%f", pStage->bundle[0].tcGenVectors[0][1]);
				FSText(", ");
				FSConst("%f", pStage->bundle[0].tcGenVectors[0][2]);
				FSText(")),\n");
				FSText("            dot(vVertex, vec3(");
				FSConst("%f", pStage->bundle[0].tcGenVectors[1][0]);
				FSText(", ");
				FSConst("%f", pStage->bundle[0].tcGenVectors[1][1]);
				FSText(", ");
				FSConst("%f", pStage->bundle[0].tcGenVectors[1][2]);
				FSText(")));\n");
				break;
			default:
				return qfalse;
			}
			for( j = 0; j < pStage->bundle[0].numTexMods; j++ ) {
				texModInfo_t *pTexMod = &(pStage->bundle[0].texMods[j]);
				
				switch( pTexMod->type ) {
				case TMOD_NONE:
					break;
				case TMOD_TRANSFORM:
					FSText("  tc = tc.s * vec2(");
					FSConst("%f", pTexMod->matrix[0][0]);
					FSText(", ");
					FSConst("%f", pTexMod->matrix[0][1]);
					FSText(") + tc.t * vec2(");
					FSConst("%f", pTexMod->matrix[1][0]);
					FSText(", ");
					FSConst("%f", pTexMod->matrix[1][1]);
					FSText(") + vec2(");
					FSConst("%f", pTexMod->translate[0]);
					FSText(", ");
					FSConst("%f", pTexMod->translate[1]);
					FSText(");\n");
					break;
				case TMOD_TURBULENT:
					FSText("  tc += ");
					FSConst("%f", pTexMod->wave.amplitude);
					FSText(" * sin(6.283185308 * (0.000976563 * vVertex.xy + vec2(");
					FSConst("%f", pTexMod->wave.phase);
					FSText(" + ");
					FSConst("%f", pTexMod->wave.frequency);
					FSText("* vShadertime)));\n");
					break;
				case TMOD_SCROLL:
					FSText("  tc += vShadertime * vec2(");
					FSConst("%f", pTexMod->scroll[0]);
					FSText(", ");
					FSConst("%f", pTexMod->scroll[1]);
					FSText(");\n");
					break;
				case TMOD_SCALE:
					FSText("  tc *= vec2(");
					FSConst("%f", pTexMod->scale[0]);
					FSText(", ");
					FSConst("%f", pTexMod->scale[1]);
					FSText(");\n");
					break;
				case TMOD_STRETCH:
					FSText(" tc = vec2(0.5) + (tc - 0.5) / ");
					FSGenFunc(pTexMod->wave);
					FSText(";\n");
					break;
				case TMOD_ROTATE:
					FSText("  tc = vec2(0.5) + genFuncRotate(");
					FSConst("%f", pTexMod->rotateSpeed);
					FSText(" * vShadertime) * (tc - vec2(0.5));\n");
					break;
				case TMOD_ENTITY_TRANSLATE:
					FSText("  tc += vShadertime * vEntTexCoord;\n");
					break;
				default:
					return qfalse;
				}
			}
			// adjust for combined image
			if ( pStage->bundle[0].combinedImage ) {
				float xScale = (float)pStage->bundle[0].image[0]->uploadWidth /
					(float)pStage->bundle[0].combinedImage->uploadWidth;
				FSText("  tc.x = (tc.x + mod(floor(vShadertime * ");
				FSConst("%f", pStage->bundle[0].imageAnimationSpeed);
				FSText("), ");
				FSConst("%f", (float)pStage->bundle[0].numImageAnimations);
				FSText(")) * ");
				FSConst("%f", xScale);
				FSText(";\n");
			}
		}

		if( i == lightmapStage && (fsFeatures & fsfGetLight) ) {
			// use custom blend for lightmap
			lightBlend = qtrue;
			FSText("  srcColor = vec4(1.0);\n");
		} else {
			switch( pStage->rgbGen ) {
			case CGEN_IDENTITY_LIGHTING:
			case CGEN_IDENTITY:
			case CGEN_CONST:
				FSText("  genColor = vec4(");
				FSConst( "%f", constantColor[i][0] / 255.0 );
				FSText(", ");
				FSConst( "%f", constantColor[i][1] / 255.0 );
				FSText(", ");
				FSConst( "%f", constantColor[i][2] / 255.0 );
				FSText(", ");
				switch( pStage->alphaGen ) {
				case AGEN_IDENTITY:
				case AGEN_CONST:
					FSConst("%f", constantColor[i][3] / 255.0);
					break;
				case AGEN_ENTITY:
					FSText("vEntColor.a");
					break;
				case AGEN_ONE_MINUS_ENTITY:
					FSText("constants.y - vEntColor.a");
					break;
				default:
					FSText("constants.x"); // will be overwritten later
					break;
				}
				FSText(");\n");
				break;
			case CGEN_ENTITY:
				FSText("  genColor = vEntColor;\n");
				break;
			case CGEN_ONE_MINUS_ENTITY:
				FSText("  genColor = constants.yyyy - vEntColor;\n");
				break;
			case CGEN_EXACT_VERTEX:
				FSText("  genColor = vColor;\n");
				break;
			case CGEN_VERTEX:
				FSText("  genColor = vColor * constants.zzzy;\n");
				break;
			case CGEN_ONE_MINUS_VERTEX:
				FSText("  genColor = constants.zzzy - vColor * constants.zzzy;\n");
				break;
			case CGEN_WAVEFORM:
				FSText("  genColor = vec4(clamp(");
				FSGenFunc(pStage->rgbWave);
				FSText(", 0.0, 1.0));\n");
				break;
			case CGEN_LIGHTING_DIFFUSE:
				if( fsFeatures & fsfGetLight ) {
					lightBlend = qtrue;
					FSText("  genColor.rgb = constants.yyy;\n");
				} else {
					FSText("  genColor.rgb = vDiffuse;\n");
				}
				break;
			case CGEN_FOG:
				return qfalse;
			default:
				return qfalse;
			}
			
			switch( pStage->alphaGen ) {
			case AGEN_IDENTITY:
			case AGEN_CONST:
				if( pStage->rgbGen != CGEN_IDENTITY &&
				    pStage->rgbGen != CGEN_IDENTITY_LIGHTING &&
				    pStage->rgbGen != CGEN_CONST ) {
					FSText("  genColor.a = ");
					FSConst("%f", constantColor[i][3] / 255.0 );
					FSText(";\n");
				}
				break;
			case AGEN_ENTITY:
				if( pStage->rgbGen != CGEN_ENTITY &&
				    pStage->rgbGen != CGEN_CONST )
					FSText("  genColor.a = vEntColor.a;\n");
				break;
			case AGEN_ONE_MINUS_ENTITY:
				if( pStage->rgbGen != CGEN_ONE_MINUS_ENTITY &&
				    pStage->rgbGen != CGEN_CONST )
					FSText("  genColor.a = constants.y - vEntColor.a;\n");
				break;
			case AGEN_VERTEX:
				if( pStage->rgbGen != CGEN_VERTEX &&
				    pStage->rgbGen != CGEN_EXACT_VERTEX )
					FSText("  genColor.a = vColor.a;\n");
				break;
			case AGEN_ONE_MINUS_VERTEX:
				if( pStage->rgbGen != CGEN_ONE_MINUS_VERTEX )
					FSText("  genColor.a = constants.y - vColor.a;\n");
				break;
			case AGEN_LIGHTING_SPECULAR:
				FSText("  genColor.a = specular.a;\n");
				break;
			case AGEN_WAVEFORM:
				FSText("  genColor.a = clamp(");
				FSGenFunc(pStage->alphaWave);
				FSText(", 0.0, 1.0);\n");
				break;
			case AGEN_PORTAL:
				FSText("  genColor.a = clamp(");
				FSConst("%f", 1.0/shader.portalRange);
				FSText(" * length(vCameraPos), 0.0, 1.0);\n");
				break;
			default:
				return qfalse;
			}

			if( pStage->bundle[0].image[0] == tr.whiteImage ) {
				FSText("  srcColor = genColor;\n");
			} else if( pStage->bundle[0].image[0] == tr.identityLightImage ) {
				FSText("  srcColor = constants.zzzy * genColor;\n");
			} else {
				FSText("  srcColor = tex2D(");
				FSText(GLSLTexNames[texIndex[i]]);
				FSText(", tc) * genColor;\n");
			}
		}
		
		// alpha test
		switch( pStage->stateBits & GLS_ATEST_BITS ) {
		case 0:
			break;
		case GLS_ATEST_GT_0:
			FSText("  if( srcColor.a > 0.0 ) {\n");
			if( aTestStart < 0) aTestStart = i;
			break;
		case GLS_ATEST_LT_80:
			FSText("  if( srcColor.a < 0.5 ) {\n");
			if( aTestStart < 0) aTestStart = i;
			break;
		case GLS_ATEST_GE_80:
			FSText("  if( srcColor.a >= 0.5 ) {\n");
			if( aTestStart < 0) aTestStart = i;
			break;
		}

		// blend
		MultIsOnePlus = qfalse;
		switch( dstBlend ) {
		case 0:
		case GLS_DSTBLEND_ZERO:
			FSText("  tmpColor = constants.xxx;\n");
			break;
		case GLS_DSTBLEND_ONE:
			FSText("  tmpColor = constants.yyy;\n");
			break;
		case GLS_DSTBLEND_SRC_COLOR:
			FSText("  tmpColor = srcColor.rgb;\n");
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
			FSText("  tmpColor = 1.0 - srcColor.rgb;\n");
			break;
		case GLS_DSTBLEND_SRC_ALPHA:
			FSText("  tmpColor = srcColor.aaa;\n");
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
			FSText("  tmpColor = 1.0 - srcColor.aaa;\n");
			break;
		}
		switch( srcBlend ) {
		case GLS_SRCBLEND_ZERO:
			FSText("  dstColorMult *= tmpColor;\n"
			       "  dstColorAdd *= tmpColor;\n");
			MultIsZero = MultIsZero || dstBlend == GLS_DSTBLEND_ZERO;
			break;
		case 0:
		case GLS_SRCBLEND_ONE:
			FSText("  dstColorMult *= tmpColor;\n"
			       "  dstColorAdd *= tmpColor;\n"
			       "  dstColorAdd += srcColor.rgb;\n");
			MultIsZero = MultIsZero || dstBlend == GLS_DSTBLEND_ZERO;
			AddIsZero = qfalse;
			break;
		case GLS_SRCBLEND_DST_COLOR:
			MultIsOnePlus = (dstBlend == GLS_DSTBLEND_ONE);
			FSText("  tmpColor += srcColor.rgb;\n"
			       "  dstColorMult *= tmpColor;\n"
			       "  dstColorAdd *= tmpColor;\n");
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
			FSText("  tmpColor -= srcColor.rgb;\n"
			       "  dstColorMult *= tmpColor;\n"
			       "  dstColorAdd *= tmpColor;\n"
			       "  dstColorAdd += srcColor.rgb;\n");
			AddIsZero = qfalse;
			break;
		case GLS_SRCBLEND_SRC_ALPHA:
			FSText("  dstColorMult *= tmpColor;\n"
			       "  dstColorAdd *= tmpColor;\n"
			       "  dstColorAdd += srcColor.rgb * srcColor.a;\n");
			MultIsZero = MultIsZero || dstBlend == GLS_DSTBLEND_ZERO;
			AddIsZero = qfalse;
			break;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
			FSText("  dstColorMult *= tmpColor;\n"
			       "  dstColorAdd *= tmpColor;\n"
			       "  dstColorAdd += srcColor.rgb * (1.0 - srcColor.a);\n");
			MultIsZero = MultIsZero || dstBlend == GLS_DSTBLEND_ZERO;
			AddIsZero = qfalse;
			break;
		}
		if( lightBlend ) {
			FSText("  dstColorMult = mix(dstColorMult, vec3(0.0), opacity);\n"
			       "  dstColorAdd *= diffuse;\n"
			       "  dstColorAdd += specular.rgb;\n");
		}
		if ( (pStage->stateBits & GLS_ATEST_BITS) != 0 &&
		     aTestStart < i ) {
			FSText("  }\n");
		}

	}
	if ( aTestStart >= 0 ) {
		FSText("  }\n");
	}
	if ( aTestStart == 0 ) {
		FSText("  else\n"
		       "    discard;\n");
	}
	shader.numUnfoggedPasses = i;
	if( normalStage >= 0 )
		shader.numUnfoggedPasses++;
	if( materialStage >= 0 )
		shader.numUnfoggedPasses++;

	// add fog
	if( vsFeatures & vsfFogNum ) {
		FSText("  int fogNum = int(vFogNum + 0.5);\n"
		       "  if( fogNum > 0 ) {\n"
		       "    vec4 plane = fogPlanes[fogNum - 1];\n"
		       "    vec4 fogCol = fogColors[fogNum - 1];\n"
		       "    float fog = dot(plane.xyz, vVertex.xyz) - plane.w;\n"
		       "    if( fog > -0.5 ) {\n"
		       "      float eyeT = fog - dot(plane.xyz, vCameraPos);\n"
		       "      if( eyeT < 0.0 )\n"
		       "        fog = fog / (fog - eyeT);\n"
		       "      else\n"
		       "        fog = 1.0;\n"
		       "    } else {\n"
		       "      fog = 0.0;\n"
		       "    }\n"
		       "    fog *= clamp(length(vCameraPos) * fogCol.w, 0.0, 1.0);\n"
		       "    dstColorAdd.xyz = mix(dstColorAdd.xyz, fogCol.xyz, fog);\n"
		       "  }\n");
	}

	// shader debugging
	if( qglUniformBlockBinding &&
	    r_perPixelLighting->integer &&
	    shader.lightmapIndex != LIGHTMAP_2D &&
	    ( fsFeatures & fsfGetLight ) ) {
		FSText("  if( dlDebug.x > 0.0 ) {\n"
		       "    vec2 tileXY = floor(gl_FragCoord.xy * dlDebug);\n"
		       "    int  tile = int(tileXY.y) * 4 + int(tileXY.x);\n"
		       "    if( tile == 0 ) {\n"
		       "      dstColorMult = constants.xxx;\n"
		       "      dstColorAdd = ambientLight.rgb;\n"
		       "    } else if( tile == 4 ) {\n"
		       "      dstColorMult = constants.xxx;\n"
		       "      dstColorAdd = directedLight.rgb;\n"
		       "    } else if( tile == 8 ) {\n"
		       "      dstColorMult = constants.xxx;\n"
		       "      dstColorAdd = 0.5 * lightDir.xyz + 0.5;\n"
		       "    } else if( tile == 3 ) {\n"
		       "      dstColorMult = constants.xxx;\n"
		       "      dstColorAdd = diffuse.rgb;\n"
		       "    } else if( tile == 7 ) {\n"
		       "      dstColorMult = constants.xxx;\n"
		       "      dstColorAdd = specular.rgb;\n"
		       "    } else if( tile == 11 ) {\n"
		       "      dstColorMult = constants.xxx;\n"
		       "      dstColorAdd = 0.5 * normal.xyz + 0.5;\n"
		       "    }\n"
		       "  }\n");
	}

	if( MultIsOnePlus )
		FSText("  dstColorMult -= vec3(1.0);\n");

	if( showDepth ) {
		FSText("  gl_FragColor = vec4(vec3(0.001 * length(vCameraPos)), 0.0);\n");
	} else if( qglBindFragDataLocationIndexed ) {
		// dstColorAdd and dstColorMult are already the output vars
	} else if( AddIsZero ) {
		FSText("  gl_FragColor = vec4(dstColorMult.xyz, 1.0);\n");
	} else if( MultIsZero ) {
		FSText("  gl_FragColor = vec4(dstColorAdd.xyz, 1.0);\n");
	} else {
		FSText("  gl_FragColor = vec4(dstColorAdd.xyz, dot(vec3(0.3333), dstColorMult.xyz));\n");
	}
	FSText("}\n");
	
	// collect attributes
	if( shader.lightmapIndex != LIGHTMAP_MD3 &&
	    (vsFeatures & vsfNormal) )
		attributes |= (1 << AL_NORMAL);
	if( vsFeatures & vsfColor )
		attributes |= (1 << AL_COLOR);
	if( vsFeatures & vsfEntColor )
		attributes |= (1 << AL_COLOR2);
	if( vsFeatures & vsfTexCoord )
		attributes |= (1 << AL_TEXCOORD);
	if( vsFeatures & vsfTexCoord2 )
		attributes |= (1 << AL_TEXCOORD2);
	if( shader.lightmapIndex == LIGHTMAP_MD3 ||
	    (vsFeatures & vsfShaderTime) )
		attributes |= (1 << AL_TIMES);
	if( vsFeatures & vsfCameraPos ) {
		attributes |= (1 << AL_CAMERAPOS);
	}
	if( vsFeatures & vsfLightDir )
		attributes |= (1 << AL_LIGHTDIR);
	if( vsFeatures & vsfEntLight )
		attributes |= (1 << AL_AMBIENTLIGHT) | (1 << AL_DIRECTEDLIGHT);

	// *** compile and link ***
	shader.GLSLprogram = RB_CompileProgram( shader.name, VS, VSidx,
						GS, GSidx,
						3, GL_TRIANGLES,
						GL_TRIANGLE_STRIP,
						FS, FSidx, attributes );
	if ( !shader.GLSLprogram )
		return qfalse;
	
	// sampler uniforms are set to the TMU once at the start
	GL_Program( shader.GLSLprogram );
	// try to move lightmap to TMU 1 to avoid rebinds
	// always leave TMU 0 as it may be used for alpha test
	if( lightmapStage > 1 ) {
		textureBundle_t temp;
		
		j = qglGetUniformLocation( shader.GLSLprogram->handle, GLSLTexNames[0] );
		if( j != -1 ) {
			qglUniform1i( j, 0 );
		}

		Com_Memcpy( &temp, &stages[1].bundle[0], sizeof(textureBundle_t) );
		Com_Memcpy( &stages[1].bundle[0], &stages[lightmapStage].bundle[0], sizeof(textureBundle_t) );
		Com_Memcpy( &stages[lightmapStage].bundle[0], &temp, sizeof(textureBundle_t) );
		j = qglGetUniformLocation( shader.GLSLprogram->handle, GLSLTexNames[lightmapStage] );
		if( j != -1 ) {
			qglUniform1i( j, 1 );
		}
		
		for( i = 2; i < lightmapStage; i++ ) {
			j = qglGetUniformLocation( shader.GLSLprogram->handle, GLSLTexNames[i] );
			if( j != -1 ) {
				qglUniform1i( j, i );
			}
		}
		j = qglGetUniformLocation( shader.GLSLprogram->handle, GLSLTexNames[1] );
		if( j != -1 ) {
			qglUniform1i( j, lightmapStage );
		}
		for( i = lightmapStage + 1; i < shader.numUnfoggedPasses; i++ ) {
			j = qglGetUniformLocation( shader.GLSLprogram->handle, GLSLTexNames[i] );
			if( j != -1 ) {
				qglUniform1i( j, i );
			}
		}
	} else {
		for( i = 0; i < shader.numUnfoggedPasses; i++ ) {
			j = qglGetUniformLocation( shader.GLSLprogram->handle, GLSLTexNames[i] );
			if( j != -1 ) {
				qglUniform1i( j, i );
			}
		}
		if( shader.lightmapIndex == LIGHTMAP_MD3 ) {
			j = qglGetUniformLocation( shader.GLSLprogram->handle, "texData" );
			if( j != -1 ) {
				qglUniform1i( j, shader.numUnfoggedPasses );
			}
		}
	}
	j = qglGetUniformLocation( shader.GLSLprogram->handle, "texLightGrid" );
	if( j != -1 ) {
		qglUniform1i( j, TMU_LIGHTGRID );
	}

	if( shader.lightmapIndex != LIGHTMAP_2D && qglUniformBlockBinding ) {
		const GLchar *dlightNames[] = {
			"dlSpheres",
			"dlColors",
			"dlLinks",
			"dlNum",
			"dlDebug"
		};
		BufUniform_t *dlightUniforms[] = {
			&backEnd.uDLSpheres,
			&backEnd.uDLColors,
			&backEnd.uDLLinks,
			&backEnd.uDLNum,
			&backEnd.uDLDebug
		};
		const GLchar *fogNames[] = {
			"lightGridScale",
			"lightGridOffset",
			"fogColors",
			"fogPlanes"
		};
		BufUniform_t *fogUniforms[] = {
			&backEnd.uLightGridScale,
			&backEnd.uLightGridOffset,
			&backEnd.uFogColors,
			&backEnd.uFogPlanes
		};

		GetBufferUniforms( shader.GLSLprogram->handle, "fogs",
				   0, &backEnd.fogBuffer,
				   &backEnd.fogBufferSize, 4,
				   fogNames, fogUniforms );

		GetBufferUniforms( shader.GLSLprogram->handle, "dLights",
				   1, &backEnd.dlightBuffer,
				   &backEnd.dlightBufferSize, 5,
				   dlightNames, dlightUniforms );
	}

	stages[0].stateBits &= ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
	if( !MultIsZero ) {
		if( qglBindFragDataLocationIndexed )
			stages[0].stateBits |= GLS_SRCBLEND_ONE
				| GLS_DSTBLEND_SRC1_COLOR;
		else if( AddIsZero )
			stages[0].stateBits |= GLS_SRCBLEND_DST_COLOR
				| (MultIsOnePlus ? GLS_DSTBLEND_ONE : GLS_DSTBLEND_ZERO);
		else
			stages[0].stateBits |= GLS_SRCBLEND_ONE
				| GLS_DSTBLEND_SRC_ALPHA;
	}
	shader.optimalStageIteratorFunc = RB_StageIteratorGLSL;
	// mirror and portal shaders
	if ( shader.sort <= SS_PORTAL )
		shader.anyGLAttr = GLA_FULL_dynamic;
	else {
		shader.anyGLAttr = GLA_COLOR_vtxcolor | GLA_TC1_texcoord | GLA_TC2_lmcoord;
		shader.allGLAttr = GLA_COLOR_vtxcolor | GLA_TC1_texcoord | GLA_TC2_lmcoord;
	}
	
	return qtrue;
}

/*
=================
VertexLightingCollapse

If vertex lighting is enabled, only render a single
pass, trying to guess which is the correct one to best aproximate
what it is supposed to look like.
=================
*/
static void VertexLightingCollapse( void ) {
	int		stage;
	shaderStage_t	*bestStage;
	int		bestImageRank;
	int		rank;

	// if we aren't opaque, just use the first pass
	if ( shader.sort == SS_OPAQUE ) {

		// pick the best texture for the single pass
		bestStage = &stages[0];
		bestImageRank = -999999;

		for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
			shaderStage_t *pStage = &stages[stage];

			if ( !pStage->active ) {
				break;
			}
			rank = 0;

			if ( pStage->bundle[0].isLightmap ) {
				rank -= 100;
			}
			if ( pStage->bundle[0].tcGen != TCGEN_TEXTURE ) {
				rank -= 5;
			}
			if ( pStage->bundle[0].numTexMods ) {
				rank -= 5;
			}
			if ( pStage->rgbGen != CGEN_IDENTITY && pStage->rgbGen != CGEN_IDENTITY_LIGHTING ) {
				rank -= 3;
			}

			if ( rank > bestImageRank  ) {
				bestImageRank = rank;
				bestStage = pStage;
			}
		}

		stages[0].bundle[0] = bestStage->bundle[0];
		stages[0].stateBits &= ~( GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS );
		stages[0].stateBits |= GLS_DEPTHMASK_TRUE;
		if ( shader.lightmapIndex == LIGHTMAP_NONE ) {
			stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		} else {
			stages[0].rgbGen = CGEN_EXACT_VERTEX;
		}
		stages[0].alphaGen = AGEN_SKIP;		
	} else {
		// don't use a lightmap (tesla coils)
		if ( stages[0].bundle[0].isLightmap ) {
			stages[0] = stages[1];
		}

		// if we were in a cross-fade cgen, hack it to normal
		if ( stages[0].rgbGen == CGEN_ONE_MINUS_ENTITY || stages[1].rgbGen == CGEN_ONE_MINUS_ENTITY ) {
			stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if ( ( stages[0].rgbGen == CGEN_WAVEFORM && stages[0].rgbWave.func == GF_SAWTOOTH )
			&& ( stages[1].rgbGen == CGEN_WAVEFORM && stages[1].rgbWave.func == GF_INVERSE_SAWTOOTH ) ) {
			stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
		if ( ( stages[0].rgbGen == CGEN_WAVEFORM && stages[0].rgbWave.func == GF_INVERSE_SAWTOOTH )
			&& ( stages[1].rgbGen == CGEN_WAVEFORM && stages[1].rgbWave.func == GF_SAWTOOTH ) ) {
			stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		}
	}

	for ( stage = 1; stage < MAX_SHADER_STAGES; stage++ ) {
		shaderStage_t *pStage = &stages[stage];

		if ( !pStage->active ) {
			break;
		}

		Com_Memset( pStage, 0, sizeof( *pStage ) );
	}
}

/*
=========================
FinishShader

Returns a freshly allocated shader with all the needed info
from the current global working shader
=========================
*/
static shader_t *FinishShader( void ) {
	int stage;
	qboolean		hasLightmapStage;
	qboolean		vertexLightmap;
	shader_t		*sh;

	hasLightmapStage = qfalse;
	vertexLightmap = qfalse;

	//
	// set sky stuff appropriate
	//
	if ( shader.isSky ) {
		shader.sort = SS_ENVIRONMENT;
	}

	//
	// set polygon offset
	//
	if ( (stages[0].stateBits & GLS_POLYGON_OFFSET) && !shader.sort ) {
		shader.sort = SS_DECAL;
	}

	//
	// set appropriate stage information
	//
	for ( stage = 0; stage < MAX_SHADER_STAGES; ) {
		shaderStage_t *pStage = &stages[stage];

		if ( !pStage->active ) {
			break;
		}

		// check for a missing texture
		if ( !pStage->bundle[0].image[0] ) {
			ri.Printf( PRINT_WARNING, "Shader %s has a stage with no image\n", shader.name );
			pStage->active = qfalse;
			stage++;
			continue;
		}

		//
		// ditch this stage if it's detail and detail textures are disabled
		//
		if ( pStage->isDetail && !r_detailTextures->integer )
		{
			int index;
			
			for(index = stage + 1; index < MAX_SHADER_STAGES; index++)
			{
				if(!stages[index].active)
					break;
			}
			
			if(index < MAX_SHADER_STAGES)
				memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage));
			else
			{
				if(stage + 1 < MAX_SHADER_STAGES)
					memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage - 1));
				
				Com_Memset(&stages[index - 1], 0, sizeof(*stages));
			}
			
			continue;
		}

		//
		// default texture coordinate generation
		//
		if ( pStage->bundle[0].isLightmap ) {
			if ( pStage->bundle[0].tcGen == TCGEN_BAD ) {
				pStage->bundle[0].tcGen = TCGEN_LIGHTMAP;
			}
			hasLightmapStage = qtrue;
		} else {
			if ( pStage->bundle[0].tcGen == TCGEN_BAD ) {
				pStage->bundle[0].tcGen = TCGEN_TEXTURE;
			}
		}


    // not a true lightmap but we want to leave existing 
    // behaviour in place and not print out a warning
    //if (pStage->rgbGen == CGEN_VERTEX) {
    //  vertexLightmap = qtrue;
    //}



		//
		// determine sort order and fog color adjustment
		//
		if ( ( pStage->stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) &&
			 ( stages[0].stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) ) {
			int blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
			int blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

			// fog color adjustment only works for blend modes that have a contribution
			// that aproaches 0 as the modulate values aproach 0 --
			// GL_ONE, GL_ONE
			// GL_ZERO, GL_ONE_MINUS_SRC_COLOR
			// GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA

			// modulate, additive
			if ( ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE ) ) ||
				( ( blendSrcBits == GLS_SRCBLEND_ZERO ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) ) {
				pStage->adjustColorsForFog = ACFF_MODULATE_RGB;
			}
			// strict blend
			else if ( ( blendSrcBits == GLS_SRCBLEND_SRC_ALPHA ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_ALPHA;
			}
			// premultiplied alpha
			else if ( ( blendSrcBits == GLS_SRCBLEND_ONE ) && ( blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->adjustColorsForFog = ACFF_MODULATE_RGBA;
			} else {
				// we can't adjust this one correctly, so it won't be exactly correct in fog
			}

			// don't screw with sort order if this is a portal or environment
			if ( !shader.sort ) {
				// see through item, like a grill or grate
				if ( pStage->stateBits & GLS_DEPTHMASK_TRUE ) {
					shader.sort = SS_SEE_THROUGH;
				} else {
					shader.sort = SS_BLEND0;
				}
			}
		}

		if( shader.isSky ) {
			pStage->stateBits |= GLS_DEPTHRANGE_1_TO_1;
		}
		
		stage++;
	}

	// there are times when you will need to manually apply a sort to
	// opaque alpha tested shaders that have later blend passes
	if ( !shader.sort ) {
		shader.sort = SS_OPAQUE;
	}

	//
	// try to generate a GLSL shader if possible
	//
	if ( !CollapseGLSL() ) {
		//
		// if we are in r_vertexLight mode, never use a lightmap texture
		//
		if ( stage > 1 && ( (r_vertexLight->integer && !r_uiFullScreen->integer) || glConfig.hardwareType == GLHW_PERMEDIA2 ) ) {
			VertexLightingCollapse();
			stage = 1;
			hasLightmapStage = qfalse;
		}
		
		//
		// look for multitexture potential
		//
		if ( stage > 1 ) {
			stage = CollapseMultitexture();
		}
		
		if ( shader.lightmapIndex >= 0 && !hasLightmapStage ) {
			if (vertexLightmap) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: shader '%s' has VERTEX forced lightmap!\n", shader.name );
			} else {
				ri.Printf( PRINT_DEVELOPER, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name );
				shader.lightmapIndex = LIGHTMAP_NONE;
			}
		}
		
		
		//
		// compute number of passes
		//
		shader.numUnfoggedPasses = stage;
		
		// fogonly shaders don't have any normal passes
		if (stage == 0 && !shader.isSky)
			shader.sort = SS_FOG;
		
		// determine which stage iterator function is appropriate
		ComputeStageIteratorFunc();
	}

	shader.isDepth = qfalse;
	sh = GeneratePermanentShader();

	// generate depth-only shader if necessary
	if( r_depthPass->integer && !shader.isSky ) {
		if( (stages[0].stateBits & GLS_DEPTHMASK_TRUE) &&
		    !(stages[0].stateBits & GLS_DEPTHFUNC_EQUAL) &&
		    !(shader.lightmapIndex == LIGHTMAP_2D) ) {
			// this shader may update depth
			stages[1].active = qfalse;
			strcat(shader.name, "*");
			
			if( stages[0].stateBits & GLS_ATEST_BITS ) {
				// alpha test requires a custom depth shader
				shader.sort = SS_DEPTH;
				shader.isDepth = qtrue;
				stages[0].stateBits &= ~GLS_SRCBLEND_BITS & ~GLS_DSTBLEND_BITS;
				stages[0].stateBits |= GLS_COLORMASK_FALSE;
				
				if( !CollapseGLSL() ) {
					shader.numUnfoggedPasses = 1;
					ComputeStageIteratorFunc();
				}
				sh->depthShader = GeneratePermanentShader();
			} else if ( shader.lightmapIndex == LIGHTMAP_MD3 &&
				    shader.cullType == 0 &&
				    shader.numDeforms == 0 &&
				    tr.defaultMD3Shader ) {
				// can use the default MD3 depth shader
				sh->depthShader = tr.defaultMD3Shader->depthShader;
			} else if ( shader.lightmapIndex != LIGHTMAP_MD3 &&
				    shader.cullType == 0 &&
				    shader.numDeforms == 0 &&
				    tr.defaultShader ) {
				// can use the default depth shader
				sh->depthShader = tr.defaultShader->depthShader;
			} else {
				// requires a custom depth shader, but can skip
				// the texturing
				shader.sort = SS_DEPTH;
				stages[0].stateBits &= ~GLS_SRCBLEND_BITS & ~GLS_DSTBLEND_BITS;
				stages[0].stateBits |= GLS_COLORMASK_FALSE;
				stages[0].bundle[0].image[0] = tr.whiteImage;
				stages[0].bundle[0].tcGen = TCGEN_IDENTITY;
				stages[0].bundle[0].numTexMods = 0;
				stages[0].rgbGen = CGEN_IDENTITY;
				stages[0].alphaGen = AGEN_IDENTITY;

				if( !CollapseGLSL() ) {
					shader.numUnfoggedPasses = 1;
					ComputeStageIteratorFunc();
				}
				shader.isDepth = qtrue;
				sh->depthShader = GeneratePermanentShader();
			}
			// disable depth writes in the main pass
			sh->stages[0]->stateBits &= ~GLS_DEPTHMASK_TRUE;
		} else {
			sh->depthShader = NULL;
		}
	} else {
		sh->depthShader = NULL;
	}
	return sh;
}

//========================================================================================

/*
====================
FindShaderInShaderText

Scans the combined text description of all the shader files for
the given shader name.

return NULL if not found

If found, it will return a valid shader
=====================
*/
static char *FindShaderInShaderText( const char *shadername ) {

	char *token, *p;

	int i, hash;

	hash = generateHashValue(shadername, MAX_SHADERTEXT_HASH);

	for (i = 0; shaderTextHashTable[hash][i]; i++) {
		p = shaderTextHashTable[hash][i];
		token = COM_ParseExt(&p, qtrue);
		if ( !Q_stricmp( token, shadername ) ) {
			return p;
		}
	}
#if 0
	p = s_shaderText;

	if ( !p ) {
		return NULL;
	}

	// look for label
	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if ( token[0] == 0 ) {
			break;
		}

		if ( !Q_stricmp( token, shadername ) ) {
			return p;
		}
		else {
			// skip the definition
			SkipBracedSection( &p );
		}
	}
#endif
	return NULL;
}


/*
==================
R_FindShaderByName

Will always return a valid shader, but it might be the
default shader if the real one can't be found.
==================
*/
shader_t *R_FindShaderByName( const char *name ) {
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh;

	if ( (name==NULL) || (name[0] == 0) ) {
		return tr.defaultShader;
	}

	COM_StripExtension(name, strippedName, sizeof(strippedName));

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh=hashTable[hash]; sh; sh=sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if (Q_stricmp(sh->name, strippedName) == 0) {
			// match found
			return sh;
		}
	}

	return tr.defaultShader;
}


/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If lightmapIndex == LIGHTMAP_NONE, then the image will have
dynamic diffuse lighting applied to it, as apropriate for most
entity skin surfaces.

If lightmapIndex == LIGHTMAP_2D, then the image will be used
for 2D rendering unless an explicit shader is found

If lightmapIndex == LIGHTMAP_BY_VERTEX, then the image will use
the vertex rgba modulate values, as apropriate for misc_model
pre-lit surfaces.

Other lightmapIndex values will have a lightmap stage created
and src*dest blending applied with the texture, as apropriate for
most world construction surfaces.

===============
*/
shader_t *R_FindShader( const char *name, int lightmapIndex, qboolean mipRawImage ) {
	char		strippedName[MAX_QPATH];
	int			i, hash;
	char		*shaderText;
	image_t		*image;
	shader_t	*sh;

	if ( name[0] == 0 ) {
		return lightmapIndex == LIGHTMAP_MD3 ? tr.defaultMD3Shader : tr.defaultShader;
	}

	// use (fullbright) vertex lighting if the bsp file doesn't have
	// lightmaps
	if ( lightmapIndex >= 0 && lightmapIndex >= tr.numLightmaps ) {
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	} else if ( lightmapIndex < LIGHTMAP_2D ) {
		// negative lightmap indexes cause stray pointers (think tr.lightmaps[lightmapIndex])
		ri.Printf( PRINT_WARNING, "WARNING: shader '%s' has invalid lightmap index of %d\n", name, lightmapIndex  );
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	}

	COM_StripExtension(name, strippedName, sizeof(strippedName));

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh = hashTable[hash]; sh; sh = sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( (sh->lightmapIndex == lightmapIndex || sh->defaultShader) &&
		     !Q_stricmp(sh->name, strippedName)) {
			// match found
			return sh;
		}
	}

	// make sure the render thread is stopped, because we are probably
	// going to have to upload an image
	if (r_smp->integer) {
		R_SyncRenderThread();
	}

	// clear the global shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );
	Q_strncpyz(shader.name, strippedName, sizeof(shader.name));
	shader.lightmapIndex = lightmapIndex;
	for ( i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
		stages[i].bundle[0].texMods = texMods[i];
	}

	//
	// attempt to define shader from an explicit parameter file
	//
	shaderText = FindShaderInShaderText( strippedName );
	if ( shaderText ) {
		// enable this when building a pak file to get a global list
		// of all explicit shaders
		if ( r_printShaders->integer ) {
			ri.Printf( PRINT_ALL, "*SHADER* %s\n", name );
		}

		if ( !ParseShader( &shaderText ) ) {
			// had errors, so use default shader
			shader.defaultShader = qtrue;
		}
		sh = FinishShader();
		return sh;
	}


	//
	// if not defined in the in-memory shader descriptions,
	// look for a single supported image file
	//
	image = R_FindImageFile( name, mipRawImage, mipRawImage, mipRawImage ? GL_REPEAT : GL_CLAMP_TO_EDGE );
	if ( !image ) {
		ri.Printf( PRINT_DEVELOPER, "Couldn't find image file for shader %s\n", name );
		shader.defaultShader = qtrue;
		return FinishShader();
	}

	//
	// create the default shading commands
	//
	if ( shader.lightmapIndex == LIGHTMAP_NONE ||
	     shader.lightmapIndex == LIGHTMAP_MD3 ) {
		// dynamic colors at vertexes
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		stages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_BY_VERTEX ) {
		// explicit colors at vertexes
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_EXACT_VERTEX;
		stages[0].alphaGen = AGEN_SKIP;
		stages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_2D ) {
		// GUI elements
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_VERTEX;
		stages[0].alphaGen = AGEN_VERTEX;
		stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	} else if ( shader.lightmapIndex == LIGHTMAP_WHITEIMAGE ) {
		// fullbright level
		stages[0].bundle[0].image[0] = tr.whiteImage;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = image;
		stages[1].active = qtrue;
		stages[1].rgbGen = CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	} else {
		// two pass lightmap
		stages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
		stages[0].bundle[0].isLightmap = qtrue;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_IDENTITY;	// lightmaps are scaled on creation
													// for identitylight
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = image;
		stages[1].active = qtrue;
		stages[1].rgbGen = CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}

	return FinishShader();
}


qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, qboolean mipRawImage) {
	int			i, hash;
	shader_t	*sh;

	hash = generateHashValue(name, FILE_HASH_SIZE);

	// probably not necessary since this function
	// only gets called from tr_font.c with lightmapIndex == LIGHTMAP_2D
	// but better safe than sorry.
	if ( lightmapIndex >= tr.numLightmaps ) {
		lightmapIndex = LIGHTMAP_WHITEIMAGE;
	}

	//
	// see if the shader is already loaded
	//
	for (sh=hashTable[hash]; sh; sh=sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ( (sh->lightmapIndex == lightmapIndex || sh->defaultShader) &&
			// index by name
			!Q_stricmp(sh->name, name)) {
			// match found
			return sh->index;
		}
	}

	// make sure the render thread is stopped, because we are probably
	// going to have to upload an image
	if (r_smp->integer) {
		R_SyncRenderThread();
	}

	// clear the global shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );
	Q_strncpyz(shader.name, name, sizeof(shader.name));
	shader.lightmapIndex = lightmapIndex;
	for ( i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
		stages[i].bundle[0].texMods = texMods[i];
	}

	//
	// create the default shading commands
	//
	if ( shader.lightmapIndex == LIGHTMAP_NONE ) {
		// dynamic colors at vertexes
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_LIGHTING_DIFFUSE;
		stages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_BY_VERTEX ) {
		// explicit colors at vertexes
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_EXACT_VERTEX;
		stages[0].alphaGen = AGEN_SKIP;
		stages[0].stateBits = GLS_DEFAULT;
	} else if ( shader.lightmapIndex == LIGHTMAP_2D ) {
		// GUI elements
		stages[0].bundle[0].image[0] = image;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_VERTEX;
		stages[0].alphaGen = AGEN_VERTEX;
		stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
			  GLS_SRCBLEND_SRC_ALPHA |
			  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	} else if ( shader.lightmapIndex == LIGHTMAP_WHITEIMAGE ) {
		// fullbright level
		stages[0].bundle[0].image[0] = tr.whiteImage;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_IDENTITY_LIGHTING;
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = image;
		stages[1].active = qtrue;
		stages[1].rgbGen = CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	} else {
		// two pass lightmap
		stages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
		stages[0].bundle[0].isLightmap = qtrue;
		stages[0].active = qtrue;
		stages[0].rgbGen = CGEN_IDENTITY;	// lightmaps are scaled on creation
													// for identitylight
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = image;
		stages[1].active = qtrue;
		stages[1].rgbGen = CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}

	sh = FinishShader();
	return sh->index; 
}


/* 
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShaderLightMap( const char *name, int lightmapIndex ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_WARNING, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, lightmapIndex, qtrue );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}


/* 
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShader( const char *name ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_WARNING, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qtrue );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}


/*
====================
RE_RegisterShaderNoMip

For menu graphics that should never be picmiped
====================
*/
qhandle_t RE_RegisterShaderNoMip( const char *name ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_WARNING, "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, LIGHTMAP_2D, qfalse );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}

/*
====================
R_GetShaderByHandle

When a handle is passed in by another module, this range checks
it and returns a valid (possibly default) shader_t to be used internally.
====================
*/
shader_t *R_GetShaderByHandle( qhandle_t hShader ) {
	if ( hShader < 0 ) {
	  ri.Printf( PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	if ( hShader >= tr.numShaders ) {
		ri.Printf( PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	return tr.shaders[hShader];
}

/*
===============
R_ShaderList_f

Dump information on all valid shaders to the console
A second parameter will cause it to print in sorted order
===============
*/
void	R_ShaderList_f (void) {
	int			i, j, k;
	int			count;
	shader_t	*shader;
	GLhandleARB	handle;
	char		*source;

	ri.Printf (PRINT_ALL, "-----------------------\n");
	count = 0;

	if( ri.Cmd_Argc() > 1 && !strcmp( ri.Cmd_Argv(1), "glsl" ) ) {
		for ( i = 0 ; i < tr.numGLSLshaders ; i++ ) {
			if( !tr.GLSLshaders[i] )
				continue;

			handle = tr.GLSLshaders[i]->handle;

			if( !handle )
				continue;

			qglGetShaderiv( handle, GL_SHADER_TYPE, &k );
			switch( k ) {
			case GL_VERTEX_SHADER_ARB:
				ri.Printf( PRINT_ALL, "--- VS %d:\n", handle );
				break;
			case GL_GEOMETRY_SHADER_EXT:
				ri.Printf( PRINT_ALL, "--- GS %d:\n", handle );
				break;
			case GL_FRAGMENT_SHADER_ARB:
				ri.Printf( PRINT_ALL, "--- FS %d:\n", handle );
				break;
			}

			qglGetShaderiv( handle, GL_SHADER_SOURCE_LENGTH, &k );
			source = ri.Hunk_AllocateTempMemory( k + 1 );
			qglGetShaderSource( handle, k, &k, source );
			ri.Printf( PRINT_ALL, "%s\n", source );
			ri.Hunk_FreeTempMemory( source );

			count++;
		}
	} else {
		for ( i = 0 ; i < tr.numShaders ; i++ ) {
			if ( ri.Cmd_Argc() > 1 ) {
				shader = tr.sortedShaders[i];
			} else {
				shader = tr.shaders[i];
			}
			
			ri.Printf( PRINT_ALL, "%i ", shader->numUnfoggedPasses );

			if (shader->lightmapIndex >= 0 ) {
				ri.Printf (PRINT_ALL, "L ");
			} else {
				ri.Printf (PRINT_ALL, "  ");
			}
			if ( shader->explicitlyDefined ) {
				ri.Printf( PRINT_ALL, "E " );
			} else {
				ri.Printf( PRINT_ALL, "  " );
			}

			if ( shader->optimalStageIteratorFunc == RB_StageIteratorGeneric ) {
				ri.Printf( PRINT_ALL, "gen " );
			} else if ( shader->optimalStageIteratorFunc == RB_StageIteratorSky ) {
				ri.Printf( PRINT_ALL, "sky " );
			} else if ( shader->optimalStageIteratorFunc == RB_StageIteratorLightmappedMultitexture ) {
				ri.Printf( PRINT_ALL, "lmmt" );
			} else if ( shader->optimalStageIteratorFunc == RB_StageIteratorVertexLitTexture ) {
				ri.Printf( PRINT_ALL, "vlt " );
			} else if ( shader->optimalStageIteratorFunc == RB_StageIteratorGLSL ) {
				ri.Printf( PRINT_ALL, "glsl" );
			} else {
				ri.Printf( PRINT_ALL, "    " );
			}

			if ( shader->GLSLprogram ) {
				ri.Printf (PRINT_ALL,  ": %s (id %d)\n", shader->name,
					   shader->GLSLprogram->handle);
			} else if ( shader->defaultShader ) {
				ri.Printf (PRINT_ALL,  ": %s (DEFAULTED)\n", shader->name);
			} else {
				ri.Printf (PRINT_ALL,  ": %s\n", shader->name);
			}
			for ( j = 0; j < shader->numUnfoggedPasses; j++ ) {
				shaderStage_t *stage = shader->stages[j];

				if ( !stage->active )
					break;

				ri.Printf (PRINT_ALL, " stage %d\n", j );

				for ( k = 0; i < NUM_TEXTURE_BUNDLES; k++ ) {
					if ( !stage->bundle[k].image[0] )
						break;

					ri.Printf (PRINT_ALL, "  %s\n", stage->bundle[k].image[0]->imgName );
				}
			}
			count++;
		}
	}
	ri.Printf (PRINT_ALL, "%i total shaders\n", count);
	ri.Printf (PRINT_ALL, "------------------\n");
}

/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
#define	MAX_SHADER_FILES	4096
static void ScanAndLoadShaderFiles( const char *extension,
				    char **text,
				    char **hashTable[MAX_SHADERTEXT_HASH])
{
	char **shaderFiles;
	char *buffers[MAX_SHADER_FILES];
	char *p;
	int numShaderFiles;
	int i;
	char *oldp, *token, *hashMem;
	int shaderTextHashTableSizes[MAX_SHADERTEXT_HASH], hash, size;

	long sum = 0, summand;
	// scan for shader files
	shaderFiles = ri.FS_ListFiles( "scripts", extension, &numShaderFiles );

	if ( (!shaderFiles || !numShaderFiles) &&
	     !strcmp( extension, ".shader") )
	{
		ri.Printf( PRINT_WARNING, "WARNING: no shader files found\n" );
		return;
	}

	if ( numShaderFiles > MAX_SHADER_FILES ) {
		numShaderFiles = MAX_SHADER_FILES;
	}

	// load and parse shader files
	for ( i = 0; i < numShaderFiles; i++ )
	{
		char filename[MAX_QPATH];

		Com_sprintf( filename, sizeof( filename ), "scripts/%s", shaderFiles[i] );
		ri.Printf( PRINT_DEVELOPER, "...loading '%s'\n", filename );
		summand = ri.FS_ReadFile( filename, (void **)&buffers[i] );
		
		if ( !buffers[i] )
			ri.Error( ERR_DROP, "Couldn't load %s", filename );
		
		// Do a simple check on the shader structure in that file to make sure one bad shader file cannot fuck up all other shaders.
		p = buffers[i];
		while(1)
		{
			token = COM_ParseExt(&p, qtrue);
			
			if(!*token)
				break;
			
			oldp = p;
			
			token = COM_ParseExt(&p, qtrue);
			if(token[0] != '{' && token[1] != '\0')
			{
				ri.Printf(PRINT_WARNING, "WARNING: Bad shader file %s has incorrect syntax.\n", filename);
				ri.FS_FreeFile(buffers[i]);
				buffers[i] = NULL;
				break;
			}

			SkipBracedSection(&oldp);
			p = oldp;
		}
		
		if (buffers[i])
			sum += summand;		
	}

	// build single large buffer
	*text = ri.Hunk_Alloc( sum + numShaderFiles*2, h_low );
	(*text)[ 0 ] = '\0';

	// free in reverse order, so the temp files are all dumped
	for ( i = numShaderFiles - 1; i >= 0 ; i-- )
	{
		if(buffers[i])
		{
			p = &(*text)[strlen(*text)];
			strcat( *text, buffers[i] );
			ri.FS_FreeFile( buffers[i] );
			COM_Compress(p);
			strcat( *text, "\n" );
		}
	}

	// free up memory
	ri.FS_FreeFileList( shaderFiles );

	Com_Memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));
	size = 0;

	p = *text;
	// look for shader names
	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if ( token[0] == 0 ) {
			break;
		}

		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTableSizes[hash]++;
		size++;
		SkipBracedSection(&p);
	}

	size += MAX_SHADERTEXT_HASH;

	hashMem = ri.Hunk_Alloc( size * sizeof(char *), h_low );

	for (i = 0; i < MAX_SHADERTEXT_HASH; i++) {
		hashTable[i] = (char **) hashMem;
		hashMem = ((char *) hashMem) + ((shaderTextHashTableSizes[i] + 1) * sizeof(char *));
	}

	Com_Memset(shaderTextHashTableSizes, 0, sizeof(shaderTextHashTableSizes));

	p = *text;
	// look for shader names
	while ( 1 ) {
		oldp = p;
		token = COM_ParseExt( &p, qtrue );
		if ( token[0] == 0 ) {
			break;
		}

		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		hashTable[hash][shaderTextHashTableSizes[hash]++] = oldp;

		SkipBracedSection(&p);
	}

	return;

}


static const char *normalVS = 
	"// IN(vec4 aVertex);\n"
	"#define aVertex gl_Vertex\n"
	"IN(vec4 aTransX);\n"
	"IN(vec4 aTransY);\n"
	"IN(vec4 aTransZ);\n"
	"IN(vec3 aNormal);\n"
	"\n"
	"OUT(smooth, vec4 vVertex);\n"
	"OUT(smooth, vec4 vNormal);\n"
	"\n"
	"vec4 transform4(vec4 point) {\n"
	"  return vec4( dot( aTransX, point ),\n"
	"               dot( aTransY, point ),\n"
	"               dot( aTransZ, point ),\n"
	"               1.0 );\n"
	"}\n"
	"\n"
	"void main() {\n"
	"  vec4 vertex = vec4(aVertex.xyz, 1.0);\n"
	"  vVertex = gl_ModelViewProjectionMatrix * transform4(vertex);\n"
	"  vNormal = gl_ModelViewProjectionMatrix * transform4(vertex + 2.0 * vec4(aNormal, 0.0));\n"
	"}\n";
static const char *normalMD3VS = 
	"// IN(vec4 aVertex);\n"
	"#define aVertex gl_Vertex\n"
	"IN(vec4 aTransX);\n"
	"IN(vec4 aTransY);\n"
	"IN(vec4 aTransZ);\n"
	"IN(vec4 aTimes);\n"
	"\n"
	"uniform sampler2D texData;\n"
	"\n"
	"OUT(smooth, vec4 vVertex);\n"
	"OUT(smooth, vec4 vNormal);\n"
	"\n"
	"vec4 transform4(vec4 point) {\n"
	"  return vec4( dot( aTransX, point ),\n"
	"               dot( aTransY, point ),\n"
	"               dot( aTransZ, point ),\n"
	"               1.0 );\n"
	"}\n"
	"\n"
	"vec4 fetchVertex(const float frameNo, const vec2 offset,\n"
	"                 out vec4 normal) {\n"
	"  vec2 tc = vec2(fract(frameNo), floor(frameNo)/1024.0) + offset;\n"
	"  vec4 data = tex2D(texData, tc);\n"
	"  vec4 hi = floor(data);\n"
	"  vec4 lo = fract(data);\n"
	"  normal = vec4((hi.xyz - 128.0) / 127.0, 0.0);\n"
	"  return vec4(lo.xyz * 1024.0 - 512.0,\n"
	"              1.0);\n"
	"}\n"
	"\n"
	"void main() {\n"
	"  vec4 normal1, normal2;\n"
	"  vec4 vertex = mix(fetchVertex(aTimes.z, aVertex.zw, normal1),\n"
	"                    fetchVertex(aTimes.w, aVertex.zw, normal2),\n"
	"                    aTimes.y);\n"
	"  vec3 normal = normalize(mix(normal1.xyz, normal2.xyz, aTimes.y));\n"
	"  vVertex = gl_ModelViewProjectionMatrix * transform4(vertex);\n"
	"  vNormal = gl_ModelViewProjectionMatrix * transform4(vertex + 2.0*vec4(normal, 0.0));\n"
	"}\n";
static const char *normalGS =
	"IN(smooth, vec4 vVertex[]);\n"
	"IN(smooth, vec4 vNormal[]);\n"
	"OUT(smooth, vec4 vColor);\n"
	"\n"
	"void main() {\n"
	"  vColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
	"  gl_Position = vVertex[0];\n"
	"  EmitVertex();\n"
	"  vColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
	"  gl_Position = vNormal[0];\n"
	"  EmitVertex();\n"
	"  EndPrimitive();\n"
	"}\n";
static const char *normalFS =
	"IN(smooth, vec4 vColor);\n"
	"void main() {\n"
	"  gl_FragColor = vColor;\n"
	"}\n";

/*
====================
CreateInternalShaders
====================
*/
static void CreateInternalShaders( void ) {
	tr.numShaders = 0;

	// init the default shader
	Com_Memset( &shader, 0, sizeof( shader ) );
	Com_Memset( &stages, 0, sizeof( stages ) );

	Q_strncpyz( shader.name, "<default>", sizeof( shader.name ) );

	shader.lightmapIndex = LIGHTMAP_NONE;
	shader.sort = SS_OPAQUE;
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].active = qtrue;
	stages[0].stateBits = GLS_DEFAULT;
	stages[0].rgbGen = CGEN_VERTEX;
	stages[0].alphaGen = AGEN_VERTEX;
	tr.defaultShader = FinishShader();

	if( qglCreateShader && qglGenBuffersARB && glGlobals.floatTextures ) {
		Q_strncpyz( shader.name, "<default md3>", sizeof( shader.name ) );
		shader.lightmapIndex = LIGHTMAP_MD3;
		shader.sort = SS_OPAQUE;
		stages[0].bundle[0].image[0] = tr.defaultImage;
		stages[0].active = qtrue;
		stages[0].stateBits = GLS_DEFAULT;
		stages[0].rgbGen = CGEN_VERTEX;
		stages[0].alphaGen = AGEN_VERTEX;
		tr.defaultMD3Shader = FinishShader();
	} else {
		tr.defaultMD3Shader = tr.defaultShader;
	}

	// fogShader exists only to generate a GLSL program for fog blending
	Q_strncpyz( shader.name, "<fog>", sizeof( shader.name ) );
	shader.lightmapIndex = LIGHTMAP_NONE;
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].rgbGen = CGEN_VERTEX;
	stages[0].stateBits = GLS_DEFAULT | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	tr.fogShader = FinishShader();

	// shadow shader is just a marker
	Q_strncpyz( shader.name, "<stencil shadow>", sizeof( shader.name ) );
	shader.sort = SS_STENCIL_SHADOW;
	tr.shadowShader = FinishShader();

	// prepare portal shader is just a marker
	Q_strncpyz( shader.name, "<prepare portal>", sizeof( shader.name ) );
	shader.sort = SS_PORTAL;
	tr.preparePortalShader = FinishShader();
	tr.preparePortalShader->optimalStageIteratorFunc = RB_StageIteratorPreparePortal;
	tr.preparePortalShader->anyGLAttr = GLA_FULL_dynamic;

	// finalise portal shader is just a marker
	Q_strncpyz( shader.name, "<finalise portal>", sizeof( shader.name ) );
	shader.sort = SS_PORTAL;
	tr.finalisePortalShader = FinishShader();
	tr.finalisePortalShader->optimalStageIteratorFunc = RB_StageIteratorFinalisePortal;
	tr.finalisePortalShader->anyGLAttr = GLA_FULL_dynamic;

	if( qglGenBuffersARB ) {
		// build VBO shader
		Q_strncpyz( shader.name, "<buildVBO>", sizeof( shader.name ) );
		shader.sort = 0;
		stages[0].active = qfalse;
		tr.buildVBOShader = FinishShader();
		tr.buildVBOShader->optimalStageIteratorFunc = RB_StageIteratorBuildWorldVBO;

		// build IBO shader
		Q_strncpyz( shader.name, "<buildIBO>", sizeof( shader.name ) );
		tr.buildIBOShader = FinishShader();
		tr.buildIBOShader->optimalStageIteratorFunc = RB_StageIteratorBuildIBO;
	}

	// internal GLSL programs for r_showNormals mode
	if( qglCreateShader && qglProgramParameteriEXT ) {
		int i;
		const char *VS[2] = { VSHeader, normalVS };
		const char *MD3VS[2] = { VSHeader, normalMD3VS };
		const char *GS[2] = { GSHeader, normalGS };
		const char *FS[2] = { FSHeader, normalFS };

		backEnd.normalProgram = RB_CompileProgram( "<normal>",
							   VS, 2,
							   GS, 2,
							   6, GL_POINTS,
							   GL_LINE_STRIP,
							   FS, 2,
							   (1 << AL_VERTEX) |
							   (1 << AL_NORMAL) |
							   (1 << AL_TRANSX) |
							   (1 << AL_TRANSY) |
							   (1 << AL_TRANSZ) );
		backEnd.normalProgramMD3 = RB_CompileProgram( "<normal md3>",
							      MD3VS, 2,
							      GS, 2,
							      6, GL_POINTS,
							      GL_LINE_STRIP,
							      FS, 2,
							      (1 << AL_VERTEX) |
							      (1 << AL_NORMAL) |
							      (1 << AL_TRANSX) |
							      (1 << AL_TRANSY) |
							      (1 << AL_TRANSZ) |
							      (1 << AL_TIMES) );
		if( backEnd.normalProgramMD3 ) {
			i = qglGetUniformLocation( backEnd.normalProgramMD3->handle, "texData" );
			if( i != -1 ) {
				qglUniform1i( i, 0 );
			}
		}
	} else {
		backEnd.normalProgram = NULL;
		backEnd.normalProgramMD3 = NULL;
	}

	if( tr.depthImage ) {
		Q_strncpyz( shader.name, "<copyDepth>", sizeof( shader.name ) );
		shader.sort = SS_COPYDEPTH;
		tr.copyDepthShader = FinishShader();
		tr.copyDepthShader->optimalStageIteratorFunc = RB_StageIteratorCopyDepth;
		tr.copyDepthShader->anyGLAttr = GLA_VERTEX_dynamic;
	}
}

static void CreateExternalShaders( void ) {
	tr.projectionShadowShader = R_FindShader( "projectionShadow", LIGHTMAP_NONE, qtrue );
	tr.flareShader = R_FindShader( "flareShader", LIGHTMAP_NONE, qtrue );

	// Hack to make fogging work correctly on flares. Fog colors are calculated
	// in tr_flare.c already.
	if(!tr.flareShader->defaultShader)
	{
		int index;
		
		for(index = 0; index < tr.flareShader->numUnfoggedPasses; index++)
		{
			tr.flareShader->stages[index]->adjustColorsForFog = ACFF_NONE;
			tr.flareShader->stages[index]->stateBits |= GLS_DEPTHTEST_DISABLE;
		}
	}

	tr.sunShader = R_FindShader( "sun", LIGHTMAP_NONE, qtrue );
	if(!tr.sunShader->defaultShader)
	{
		int index;
		
		for(index = 0; index < tr.sunShader->numUnfoggedPasses; index++)
		{
			tr.sunShader->stages[index]->stateBits |= GLS_DEPTHRANGE_1_TO_1;
		}
	}
}

/*
==================
R_InitShaders
==================
*/
void R_InitShaders( void ) {
	ri.Printf( PRINT_ALL, "Initializing Shaders\n" );

	Com_Memset(hashTable, 0, sizeof(hashTable));

	CreateInternalShaders();

	ScanAndLoadShaderFiles( ".shader", &s_shaderText,
				shaderTextHashTable );

	CreateExternalShaders();
}
