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

backEndData_t	*backEndData[SMP_FRAMES];
backEndState_t	backEnd;

static glIndex_t	quadIndexes[6] = { 3, 0, 2, 2, 0, 1 };

/*
** GL_ActiveTexture
*/
void GL_ActiveTexture( int unit ) {
	if( qglActiveTextureARB && glState.activeTexture != unit ) {
		qglActiveTextureARB( GL_TEXTURE0_ARB + unit );
		glState.activeTexture = unit;
	}
}
void GL_ClientActiveTexture( int unit ) {
	if( qglClientActiveTextureARB && glState.clientActiveTexture != unit ) {
		qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );
		glState.clientActiveTexture = unit;
	}
}

/*
** GL_BindTexture
** 
** binds a texture to texture unit 0 for texture manipulation.
** This is called by the frontend, so it may use a separate context for SMP,
** in which case the glState must not be changed !
*/
void GL_BindTexture( int texnum ) {
	if( !GLimp_InBackend() ) {
		qglBindTexture( GL_TEXTURE_2D, texnum );
	} else if ( glState.currenttextures[0] != texnum ) {
		GL_ActiveTexture( 0 );
		glState.currenttextures[0] = texnum;
		qglBindTexture( GL_TEXTURE_2D, texnum );
	}
}

/*
** GL_UnbindAllTextures
**
** unbind all texture units for renderer cleanup.
*/
void GL_UnbindAllTextures( void ) {
	int	i;

	for( i = 0; i < MAX_SHADER_STAGES; i++ ) {
		if( i >= glGlobals.maxTextureImageUnits )
			break;

		if( glState.currenttextures[i] ) {
			glState.currenttextures[i] = 0;
			GL_ActiveTexture( i );
			qglBindTexture( glState.texTargets[i], 0 );
			if( i < NUM_TEXTURE_BUNDLES ) {
				glState.texEnabled[i] = qfalse;
				qglDisable( GL_TEXTURE_2D );
			}
		}
	}
}

/*
** GL_Bind
**
** bind a list of images to the texture units. For GLSL shader it's not
** required to glEnable them and we may keep unused textures bound, so
** we can avoid to rebind them later.
*/
static void GL_BindImages( int count, image_t **images, qboolean isGLSL ) {
	int i, texnum;
	GLenum target;
	
	if( !qglActiveTextureARB && count > 1 ) {
		ri.Printf( PRINT_WARNING, "GL_BindImages: Multitexturing not enabled\n" );
		count = 1;
	}
	
	for( i = 0; i < count; i++ ) {
		if ( !images[i] ) {
			ri.Printf( PRINT_WARNING, "GL_BindImages: NULL image\n" );
			target = GL_TEXTURE_2D;
			texnum = tr.defaultImage->texnum;
		} else {
			target = images[i]->target;
			texnum = images[i]->texnum;
		}
		
		if ( r_nobind->integer &&
		     tr.dlightImage &&
		     target == GL_TEXTURE_2D ) {		// performance evaluation option
			texnum = tr.dlightImage->texnum;
		}
		
		if ( glState.texTargets[i] &&
		     glState.texTargets[i] != target ) {
			GL_ActiveTexture( i );
			qglBindTexture( glState.texTargets[i], 0 );
			glState.currenttextures[i] = 0;
		}
		if ( glState.currenttextures[i] != texnum ) {
			images[i]->frameUsed = tr.frameCount;
			glState.currenttextures[i] = texnum;
			glState.texTargets[i] = target;
			GL_ActiveTexture( i );
			qglBindTexture( target, texnum );
		}
		if( !isGLSL && !glState.texEnabled[i] &&
		    target == GL_TEXTURE_2D ) {
			GL_ActiveTexture( i );
			qglEnable( GL_TEXTURE_2D );
			glState.texEnabled[i] = qtrue;
		}
	}

	if( !isGLSL ) {
		// have to disable further textures for non-GLSL shaders
		for( ; i < NUM_TEXTURE_BUNDLES; i++ ) {
			if( i >= glGlobals.maxTextureImageUnits )
				break;

			if( glState.texEnabled[i] ) {
				GL_ActiveTexture( i );
				qglDisable( GL_TEXTURE_2D );
				glState.texEnabled[i] = qfalse;
			}
		}
	}
}

/*
==================
SetRenderState

set all OpenGL state to the values passed in state, avoid calling gl functions
if the state doesn't actually change
==================
*/
static void GL_State( unsigned long stateBits ) {
	unsigned long diff = stateBits ^ glState.glStateBits;

	if ( !diff ) {
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_BITS ) {
		switch( stateBits & GLS_DEPTHFUNC_BITS ) {
		case GLS_DEPTHFUNC_EQUAL:
			qglDepthFunc( GL_EQUAL );
			break;
		case GLS_DEPTHFUNC_ALWAYS:
			qglDepthFunc( GL_ALWAYS );
			break;
		default:
			qglDepthFunc( GL_LEQUAL );
			break;
		}
	}

	//
	// check blend bits
	//
	if ( diff & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) {
		GLenum srcFactor, dstFactor;

		if ( stateBits & ( GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS ) ) {
			switch ( stateBits & GLS_SRCBLEND_BITS ) {
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			case GLS_DSTBLEND_SRC1_COLOR:
				dstFactor = GL_SRC1_COLOR;
				break;
			default:
				srcFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS ) {
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				dstFactor = GL_ONE;		// to get warning to shut up
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE ) {
		if ( stateBits & GLS_DEPTHMASK_TRUE ) {
			qglDepthMask( GL_TRUE );
		} else {
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// check colormask
	//
	if ( diff & GLS_COLORMASK_FALSE ) {
		if ( stateBits & GLS_COLORMASK_FALSE ) {
			qglColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
		} else {
			qglColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		}
	}

	// check polygon offset
	if ( diff & GLS_POLYGON_OFFSET ) {
		if ( stateBits & GLS_POLYGON_OFFSET ) {
			qglEnable( GL_POLYGON_OFFSET_FILL );
			qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
		} else {
			qglDisable( GL_POLYGON_OFFSET_FILL );
		}
	}
	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE ) {
		if ( stateBits & GLS_POLYMODE_LINE ) {
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		} else {
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE ) {
		if ( stateBits & GLS_DEPTHTEST_DISABLE ) {
			qglDisable( GL_DEPTH_TEST );
		} else {
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// depth range
	//
	if ( diff & GLS_DEPTHRANGE_BITS ) {
		switch ( stateBits & GLS_DEPTHRANGE_BITS ) {
		case GLS_DEPTHRANGE_0_TO_1:
			qglDepthRange( 0.0f, 1.0f );
			break;
		case GLS_DEPTHRANGE_0_TO_0:
			qglDepthRange( 0.0f, 0.0f );
			break;
		case GLS_DEPTHRANGE_1_TO_1:
			qglDepthRange( 1.0f, 1.0f );
			break;
		case GLS_DEPTHRANGE_0_TO_03:
			qglDepthRange( 0.0f, 0.3f );
			break;
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS ) {
		switch ( stateBits & GLS_ATEST_BITS ) {
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		default:
			assert( 0 );
			break;
		}
	}

	glState.glStateBits = stateBits;
}
static void GL_Cull( int cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;

	if ( cullType == CT_TWO_SIDED ) 
	{
		qglDisable( GL_CULL_FACE );
	} 
	else 
	{
		qglEnable( GL_CULL_FACE );

		if ( cullType == CT_BACK_SIDED )
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_FRONT );
			}
			else
			{
				qglCullFace( GL_BACK );
			}
		}
		else
		{
			if ( backEnd.viewParms.isMirror )
			{
				qglCullFace( GL_BACK );
			}
			else
			{
				qglCullFace( GL_FRONT );
			}
		}
	}
}
void GL_Program( GLSLprogram_t *program )
{
	if ( glState.currentProgram != program ) {
		glState.currentProgram = program;
		qglUseProgram( program ? program->handle : 0 );
	}
}
static void DisableAttributePointer( int attr ) {
	switch( attr ) {
	case AL_VERTEX:
		qglDisableClientState( GL_VERTEX_ARRAY );
		break;
	case AL_NORMAL:
		qglDisableClientState( GL_NORMAL_ARRAY );
		break;
	case AL_COLOR:
		qglDisableClientState( GL_COLOR_ARRAY );
		break;
	case AL_TEXCOORD:
	case AL_TEXCOORD2:
	case AL_TEXCOORD3:
	case AL_TEXCOORD4:
		GL_ClientActiveTexture( attr - AL_TEXCOORD );
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		break;
	default:
		qglDisableVertexAttribArrayARB( attr );
		break;
	}
}
static void EnableAttributePointer( int attr ) {
	switch( attr ) {
	case AL_VERTEX:
		qglEnableClientState( GL_VERTEX_ARRAY );
		break;
	case AL_NORMAL:
		qglEnableClientState( GL_NORMAL_ARRAY );
		break;
	case AL_COLOR:
		qglEnableClientState( GL_COLOR_ARRAY );
		break;
	case AL_TEXCOORD:
	case AL_TEXCOORD2:
	case AL_TEXCOORD3:
	case AL_TEXCOORD4:
		GL_ClientActiveTexture( attr - AL_TEXCOORD );
		qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		break;
	default:
		qglEnableVertexAttribArrayARB( attr );
		break;
	}
}
static void SetAttribute4f( int attr, vec_t *values ) {
	switch( attr ) {
	case AL_VERTEX:
		qglVertex4fv( values );
		break;
	case AL_NORMAL:
		qglNormal3fv( values );
		break;
	case AL_COLOR:
		qglColor4fv( values );
		break;
	case AL_TEXCOORD:
		qglTexCoord4fv( values );
		break;
	case AL_TEXCOORD2:
	case AL_TEXCOORD3:
	case AL_TEXCOORD4:
		if( qglMultiTexCoord4fvARB )
			qglMultiTexCoord4fvARB( attr - AL_TEXCOORD + GL_TEXTURE0_ARB, values );
		break;
	default:
		qglVertexAttrib4fvARB( attr, values );
		break;
	}
	glState.glAttribute[attr].attrType = RA_VEC;
	glState.glAttribute[attr].vec[0] = values[0];
	glState.glAttribute[attr].vec[1] = values[1];
	glState.glAttribute[attr].vec[2] = values[2];
	glState.glAttribute[attr].vec[3] = values[3];
}
static void SetAttributePointer( int attr, GLuint VBO, GLint size,
				 GLenum type, GLsizei stride, void *ptr ) {
	GL_VBO( VBO );
	switch( attr ) {
	case AL_VERTEX:
		qglVertexPointer( size, type, stride, ptr );
		break;
	case AL_NORMAL:
		qglNormalPointer( type, stride, ptr );
		break;
	case AL_COLOR:
		qglColorPointer( size, type, stride, ptr );
		break;
	case AL_TEXCOORD:
	case AL_TEXCOORD2:
	case AL_TEXCOORD3:
	case AL_TEXCOORD4:
		GL_ClientActiveTexture( attr - AL_TEXCOORD );
		qglTexCoordPointer( size, type, stride, ptr );
		break;
	default:
		qglVertexAttribPointerARB( attr, size, type, GL_FALSE, stride, ptr );
		break;
	}
	glState.glAttribute[attr].attrType = RA_POINTER;
	glState.glAttribute[attr].VBO = VBO;
	glState.glAttribute[attr].ptr = ptr;
	glState.glAttribute[attr].size = size;
	glState.glAttribute[attr].type = type;
	glState.glAttribute[attr].stride = stride;
}
static void SetAttributeDivisor( int attr, GLuint divisor ) {
	switch( attr ) {
	case AL_VERTEX:
	case AL_NORMAL:
	case AL_COLOR:
	case AL_TEXCOORD:
	case AL_TEXCOORD2:
	case AL_TEXCOORD3:
	case AL_TEXCOORD4:
		// ignore non-generic attributes
		break;
	default:
		qglVertexAttribDivisorARB( attr, divisor );
		break;
	}
	glState.glAttribute[attr].divisor = divisor;
}
static void SetRenderPointers( glRenderState_t *state ) {
	int i;
	unsigned int attributes;

	if( state->program )
		attributes = state->program->attributes;
	else
		attributes = (1 << AL_VERTEX) | (1 << AL_NORMAL) |
			(1 << AL_COLOR) | (1 << AL_TEXCOORD) |
			(1 << AL_TEXCOORD2) | (1 << AL_TEXCOORD3) |
			(1 << AL_TEXCOORD4);

	// NVIDIA recommends to set the glVertexPointer last
	for( i = AL_NUMATTRIBUTES-1; i >= 0; i-- ) {
		if( state->attrib[i].attrType == RA_UNSPEC ||
		    !(attributes & (1 << i)) ) {
			if( glState.glAttribute[i].attrType == RA_POINTER ) {
				// diable pointer for unspecified attrs,
				// otherwise OpenGL may segfault
				DisableAttributePointer( i );
				glState.glAttribute[i].attrType = RA_UNSPEC;
			}
		} else if( state->attrib[i].attrType == RA_VEC ) {
			if( glState.glAttribute[i].attrType == RA_POINTER ) {
				DisableAttributePointer( i );
				SetAttribute4f( i, state->attrib[i].vec );
			} else if( glState.glAttribute[i].attrType == RA_VEC &&
				   glState.glAttribute[i].vec[0] == state->attrib[i].vec[0] &&
				   glState.glAttribute[i].vec[1] == state->attrib[i].vec[1] &&
				   glState.glAttribute[i].vec[2] == state->attrib[i].vec[2] &&
				   glState.glAttribute[i].vec[3] == state->attrib[i].vec[3] ) {
				// do nothing, unchanged attribute
			} else {
				SetAttribute4f( i, state->attrib[i].vec );
			}
		} else {
			if( glState.glAttribute[i].attrType == RA_POINTER ) {
				if( glState.glAttribute[i].VBO != state->attrib[i].VBO ||
				    glState.glAttribute[i].ptr != state->attrib[i].ptr ||
				    glState.glAttribute[i].size != state->attrib[i].size ||
				    glState.glAttribute[i].type != state->attrib[i].type ||
				    glState.glAttribute[i].stride != state->attrib[i].stride ) {
					// pointer or VBO changed
					SetAttributePointer( i, state->attrib[i].VBO,
							     state->attrib[i].size,
							     state->attrib[i].type,
							     state->attrib[i].stride,
							     state->attrib[i].ptr );
				}
			} else {
				EnableAttributePointer( i );
				SetAttributePointer( i, state->attrib[i].VBO,
						     state->attrib[i].size,
						     state->attrib[i].type,
						     state->attrib[i].stride,
						     state->attrib[i].ptr );
			}
			if( state->attrib[i].divisor != glState.glAttribute[i].divisor ) {
				SetAttributeDivisor( i, state->attrib[i].divisor );
			}
		}
	}
}
static void SetRenderState( glRenderState_t *state ) {
	if(backEnd.currentEntity &&
	   (backEnd.currentEntity->e.renderfx & RF_DEPTHHACK) ) {
		if( (state->stateBits & GLS_DEPTHRANGE_BITS) == GLS_DEPTHRANGE_0_TO_1 )
			state->stateBits |= GLS_DEPTHRANGE_0_TO_03;
	}

	GL_Program( state->program );
	GL_State( state->stateBits );
	GL_Cull( state->faceCulling );
	GL_BindImages( state->numImages, state->image, state->program != NULL );
	SetRenderPointers( state );
}

void GL_LockArrays( glRenderState_t *state ) {
	SetRenderPointers( state );
	if( tess.numIndexes[2] > 0 )
		qglLockArraysEXT( tess.minIndex[1], tess.maxIndex[2] - tess.minIndex[1] + 1 );
	else
		qglLockArraysEXT( tess.minIndex[1], tess.maxIndex[1] - tess.minIndex[1] + 1 );
}

void GL_UnlockArrays( void ) {
	qglUnlockArraysEXT();
}

void GL_StartQuery( GLuint query, GLuint *result ) {
	if( !(*result & QUERY_RUNNING_BIT) )
		qglBeginQueryARB( GL_SAMPLES_PASSED_ARB, query );
}
void GL_EndQuery( GLuint query, GLuint *result ) {
	if( !(*result & QUERY_RUNNING_BIT) )
		qglEndQueryARB( GL_SAMPLES_PASSED_ARB );
	*result |= QUERY_RUNNING_BIT;
}
void GL_GetQuery( GLuint query, GLuint *result ) {
	GLuint available;

	if( *result & QUERY_RUNNING_BIT ) {
		qglGetQueryObjectuivARB( query,
					 GL_QUERY_RESULT_AVAILABLE_ARB,
					 &available);
		if ( available ) {
			qglGetQueryObjectuivARB( query,
						 GL_QUERY_RESULT_ARB,
						 result);

			if( *result & QUERY_RUNNING_BIT )
				*result = QUERY_MASK;		// overflow
		}
	}
}

void GL_DrawElements( glRenderState_t *state,
		      int numIndexes, GLuint IBO,
		      const glIndex_t *indexes,
		      glIndex_t start, glIndex_t end ) {
	if( numIndexes <= 0 )
		return;

	SetRenderState( state );
	GL_IBO( IBO );
	
	if ( qglDrawRangeElementsEXT )
		qglDrawRangeElementsEXT( GL_TRIANGLES, 
					 start, end,
					 numIndexes,
					 GL_INDEX_TYPE,
					 indexes );
	else
		qglDrawElements( GL_TRIANGLES,
				 numIndexes,
				 GL_INDEX_TYPE,
				 indexes );
}
void GL_DrawInstanced( glRenderState_t *state,
		       int numIndexes, GLuint IBO,
		       const glIndex_t *indexes,
		       glIndex_t start, glIndex_t end,
		       int instances ) {
	if( numIndexes <= 0 )
		return;

	SetRenderState( state );
	GL_IBO( IBO );
	
	qglDrawElementsInstancedARB( GL_TRIANGLES, 
				     numIndexes,
				     GL_INDEX_TYPE,
				     indexes,
				     instances );
}
void GL_DrawArrays( glRenderState_t *state,
		    GLenum mode, GLint first, GLuint count ) {
	SetRenderState( state );
	
	qglDrawArrays( mode, first, count );
}


/*
** GL_TexEnv
*/
void GL_TexEnv( int tmu, int env )
{
	if ( env == glState.texEnv[tmu] )
	{
		return;
	}

	GL_ActiveTexture( tmu );
	glState.texEnv[tmu] = env;

	switch ( env )
	{
	case GL_MODULATE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		break;
	case GL_REPLACE:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
		break;
	case GL_DECAL:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL );
		break;
	case GL_ADD:
		qglTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD );
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed\n", env );
		break;
	}
}

/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_VBO( GLuint vbo )
{
	if ( glState.currentVBO != vbo ) {
		glState.currentVBO = vbo;
		qglBindBufferARB (GL_ARRAY_BUFFER_ARB, vbo );
	}
}
void GL_IBO( GLuint ibo )
{
	if ( glState.currentIBO != ibo ) {
		glState.currentIBO = ibo;
		qglBindBufferARB (GL_ELEMENT_ARRAY_BUFFER_ARB, ibo );
	}
}
void GL_UBO( GLuint ubo )
{
	if ( glState.currentUBO != ubo ) {
		glState.currentUBO = ubo;
		qglBindBufferARB (GL_UNIFORM_BUFFER, ubo );
	}
}

// simple, fast allocator for the backend thread
// memory must be freed in LIFO order
static byte	*scratchStart, *scratchPtr;
static size_t	freeScratch;
void RB_InitScratchMemory( void ) {
	freeScratch = r_scratchmegs->integer;
	if( freeScratch <= SMP_SCRATCHMEGS )
		freeScratch = SMP_SCRATCHMEGS;
	freeScratch *= 1024 * 1024;

	scratchStart = scratchPtr = ri.Hunk_Alloc( freeScratch, h_low );
}
void *RB_AllocScratch( size_t amount ) {
	byte *mem = scratchPtr;

	amount = (amount + 31) & -32;
	if( amount > freeScratch ) {
		ri.Error( ERR_DROP, "RB_AllocScratch: out of scratch memory, try to increase /r_scratchmegs\n" );
	}
	scratchPtr += amount;
	freeScratch -= amount;
	return mem;
}
void RB_FreeScratch( void *ptr ) {
	if( (byte *)ptr < scratchStart || (byte *)ptr > scratchPtr ) {
		ri.Error( ERR_DROP, "RB_FreeScratch: bad pointer\n" );
	}
	freeScratch += (scratchPtr - (byte *)ptr);
	scratchPtr = ptr;
}

GLSLshader_t *RB_CompileShader( const char *name, GLenum type,
				const char **code, int parts ) {
	GLSLshader_t	*shader;
	GLint 		status;
	GLint		i, j;
	unsigned int	hash;
	const char	*ptr;
	
	// try to reuse existing shader
	for( i = 0, hash = 0; i < parts; i++ ) {
		for( ptr = code[i]; *ptr; ptr++ ) {
			hash = *ptr + hash * 65599;
		}
	}
	
	for( i = 0; i < tr.numGLSLshaders; i++ ) {
		if( tr.GLSLshaders[i]->hash == hash ) {
			GLint   length;
			char    *source, *sourcePtr;
			qboolean same = qtrue;

			qglGetShaderiv( tr.GLSLshaders[i]->handle,
					GL_SHADER_SOURCE_LENGTH,
					&length );
			sourcePtr = source = ri.Hunk_AllocateTempMemory( length + 1 );
			qglGetShaderSource( tr.GLSLshaders[i]->handle, length + 1,
					    &length, source );
			
			for( j = 0; j < parts; j++ ) {
				for( ptr = code[j]; *ptr; ptr++, sourcePtr++ ) {
					if( *ptr != *sourcePtr )
						break;
				}
			}
			same = *sourcePtr == '\0';
			ri.Hunk_FreeTempMemory( source );

			if( same )
				return tr.GLSLshaders[i];
		}
	}

	shader = ri.Hunk_Alloc( sizeof(GLSLprogram_t), h_low );
	shader->handle = qglCreateShader( type );
	shader->hash = hash;
	qglShaderSource( shader->handle, parts, code, NULL );
	qglCompileShader( shader->handle );
	qglGetShaderiv( shader->handle, GL_OBJECT_COMPILE_STATUS_ARB, &status );
	if( !status ) {
		char *log;
		GLint len;
		qglGetShaderiv( shader->handle, GL_OBJECT_INFO_LOG_LENGTH_ARB, &len );
		log = ri.Hunk_AllocateTempMemory( len + 1 );
		qglGetShaderInfoLog( shader->handle, len + 1, &len, log );
		
		ri.Printf( PRINT_WARNING, "compile shader (%s) error: %s\n",
			   name, log );
		while( parts > 0 ) {
			ri.Printf( PRINT_WARNING, "%s", *(code++) );
			parts--;
		}
		
		ri.Hunk_FreeTempMemory( log );
		qglDeleteShader( shader->handle );
		return NULL;
	}
	tr.GLSLshaders[tr.numGLSLshaders++] = shader;
	return shader;
}

GLSLprogram_t *RB_CompileProgram( const char *name,
				  const char **VScode, int VSparts,
				  const char **GScode, int GSparts,
				  int nVerticesOut, int inType, int outType,
				  const char **FScode, int FSparts,
				  unsigned int attributes ) {
	GLSLshader_t	*VertexShader = NULL;
	GLSLshader_t	*GeometryShader = NULL;
	GLSLshader_t	*FragmentShader = NULL;
	GLint		Program;
	GLint		i;
	GLSLprogram_t	*newProgram;

	// find shaders
	if( VSparts > 0 ) {
		VertexShader = RB_CompileShader( name, GL_VERTEX_SHADER_ARB,
						 VScode, VSparts );
		if( !VertexShader )
			return NULL;  // compilation error
	}
	if( GSparts > 0 ) {
		GeometryShader = RB_CompileShader( name, GL_GEOMETRY_SHADER_EXT,
						   GScode, GSparts );
		if( !GeometryShader )
			return NULL;  // compilation error
	}
	if( FSparts > 0 ) {
		FragmentShader = RB_CompileShader( name, GL_FRAGMENT_SHADER_ARB,
						   FScode, FSparts );
		if( !FragmentShader )
			return NULL;  // compilation error
	}

	// try to reuse existing program
	for( i = 0; i < tr.numGLSLprograms; i++ ) {
		if( tr.GLSLprograms[i]->vertex == VertexShader &&
		    tr.GLSLprograms[i]->geometry == GeometryShader &&
		    tr.GLSLprograms[i]->fragment == FragmentShader ) {
			return tr.GLSLprograms[i];
		}
	}

	Program = qglCreateProgram();
	if( VertexShader )
		qglAttachShader( Program, VertexShader->handle );
	if( GeometryShader ) {
		qglAttachShader( Program, GeometryShader->handle );

		qglProgramParameteriEXT( Program, GL_GEOMETRY_VERTICES_OUT_EXT, nVerticesOut );
		qglProgramParameteriEXT( Program, GL_GEOMETRY_INPUT_TYPE_EXT, inType );
		qglProgramParameteriEXT( Program, GL_GEOMETRY_OUTPUT_TYPE_EXT, outType );
	}
	if( FragmentShader )
		qglAttachShader( Program, FragmentShader->handle );
	
	if( attributes & (1 << AL_COLOR2) )
		qglBindAttribLocationARB( Program, AL_COLOR2,        "aEntColor" );
	if( attributes & (1 << AL_CAMERAPOS) )
		qglBindAttribLocationARB( Program, AL_CAMERAPOS,     "aCameraPos" );
	if( attributes & (1 << AL_TIMES) )
		qglBindAttribLocationARB( Program, AL_TIMES,         "aTimes" );
	if( attributes & (1 << AL_TRANSX) )
		qglBindAttribLocationARB( Program, AL_TRANSX,        "aTransX" );
	if( attributes & (1 << AL_TRANSY) )
		qglBindAttribLocationARB( Program, AL_TRANSY,        "aTransY" );
	if( attributes & (1 << AL_TRANSZ) )
		qglBindAttribLocationARB( Program, AL_TRANSZ,        "aTransZ" );
	if( attributes & (1 << AL_AMBIENTLIGHT) )
		qglBindAttribLocationARB( Program, AL_AMBIENTLIGHT,  "aAmbientLight" );
	if( attributes & (1 << AL_DIRECTEDLIGHT) )
		qglBindAttribLocationARB( Program, AL_DIRECTEDLIGHT, "aDirectedLight" );
	if( attributes & (1 << AL_LIGHTDIR) )
		qglBindAttribLocationARB( Program, AL_LIGHTDIR,      "aLightDir" );
	
	if( qglBindFragDataLocationIndexed ) {
		qglBindFragDataLocationIndexed( Program, 0, 0, "dstColorAdd" );
		qglBindFragDataLocationIndexed( Program, 0, 1, "dstColorMult" );
	}

	qglLinkProgram( Program );
	qglGetProgramiv( Program, GL_OBJECT_LINK_STATUS_ARB, &i );
	if ( !i ) {
		char *log;
		qglGetProgramiv( Program, GL_OBJECT_INFO_LOG_LENGTH_ARB, &i );
		log = ri.Hunk_AllocateTempMemory( i + 1 );
		qglGetProgramInfoLog( Program, i + 1, &i, log );
		
		ri.Printf( PRINT_WARNING, "link shader %s error: %s\n", name, log );
		while( VSparts > 0 ) {
			ri.Printf( PRINT_WARNING, "%s", *(VScode++) );
			VSparts--;
		}
		while( GSparts > 0 ) {
			ri.Printf( PRINT_WARNING, "%s", *(GScode++) );
			GSparts--;
		}
		while( FSparts > 0 ) {
			ri.Printf( PRINT_WARNING, "%s", *(FScode++) );
			FSparts--;
		}

		ri.Hunk_FreeTempMemory( log );
		qglDeleteProgram( Program );
		if( FragmentShader )
			qglDeleteShader( FragmentShader->handle );
		if( GeometryShader )
			qglDeleteShader( GeometryShader->handle );
		if( VertexShader )
			qglDeleteShader( VertexShader->handle );
		return NULL;
	}

	newProgram = ri.Hunk_Alloc( sizeof(GLSLprogram_t), h_low );
	tr.GLSLprograms[tr.numGLSLprograms++] = newProgram;
	
	newProgram->handle = Program;

	newProgram->vertex = VertexShader;
	newProgram->geometry = GeometryShader;
	newProgram->fragment = FragmentShader;
	newProgram->attributes = attributes;

	return newProgram;
}


/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	float		c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	c = ( backEnd.refdef.time & 255 ) / 255.0f;
	qglClearColor( c, c, c, 1 );
	qglClear( GL_COLOR_BUFFER_BIT );

	backEnd.isHyperspace = qtrue;
}


static void SetViewportAndScissor( void ) {
	float	mat[16], scale;
	vec4_t	q, c;
	
	Com_Memcpy( mat, backEnd.viewParms.projectionMatrix, sizeof(mat) );
	if( backEnd.viewParms.portalLevel ) {
		c[0] = -DotProduct( backEnd.viewParms.portalPlane.normal, backEnd.viewParms.or.axis[1] );
		c[1] = DotProduct( backEnd.viewParms.portalPlane.normal, backEnd.viewParms.or.axis[2] );
		c[2] = -DotProduct( backEnd.viewParms.portalPlane.normal, backEnd.viewParms.or.axis[0] );
		c[3] = DotProduct( backEnd.viewParms.portalPlane.normal, backEnd.viewParms.or.origin ) - backEnd.viewParms.portalPlane.dist;
		
		q[0] = (c[0] < 0.0f ? -1.0f : 1.0f) / mat[0];
		q[1] = (c[1] < 0.0f ? -1.0f : 1.0f) / mat[5];
		q[2] = -1.0f;
		q[3] = (1.0f + mat[10]) / mat[14];
		
		scale = 2.0f / (DotProduct( c, q ) + c[3] * q[3]);
		mat[2]  = c[0] * scale;
		mat[6]  = c[1] * scale;
		mat[10] = c[2] * scale + 1.0f;
		mat[14] = c[3] * scale;
	}
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf( mat );
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY, 
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
}

/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
void RB_BeginDrawingView (void) {
	int clearBits = 0;

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
		qglFinish ();
		glState.finishCalled = qtrue;
	}
	if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );
	// clear relevant buffers
	if ( backEnd.viewParms.isFirst ) {
		clearBits = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
		
		if ( r_clear->integer ) {
			clearBits |= GL_COLOR_BUFFER_BIT;
			qglClearColor( 1.0f, 0.0f, 0.5f, 1.0f );
		}
		if ( r_fastsky->integer && !( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) )
		{
			clearBits |= GL_COLOR_BUFFER_BIT;	// FIXME: only if sky shaders have been used
#ifdef _DEBUG
			qglClearColor( 0.8f, 0.7f, 0.4f, 1.0f );	// FIXME: get color of sky
#else
			qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );	// FIXME: get color of sky
#endif
		}
		qglStencilMask( glGlobals.portalMask | glGlobals.shadowMask );
		qglClear( clearBits );

		RB_PrepareDLights( &backEnd.refdef );
	}

	if ( ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) )
	{
		RB_Hyperspace();
		return;
	}
	else
	{
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;
}


#define	MAC_EVENT_PUMP_MSEC		5


/*
==================
RB_InstanceFromEntity

Fills the current instance vars from an entity
==================
*/
static void RB_InstanceFromEntity( trRefEntity_t *ent ) {
	instanceVars_t *inst = &tess.instances[ tess.numInstances - 1 ];

	inst->times[0] = tess.shaderTime;
	if( tess.dataTexture ) {
		inst->times[1] = ent->e.backlerp;
		inst->times[2] = (ent->e.frame / tess.framesPerRow) * tess.scaleY +
			(ent->e.frame % tess.framesPerRow) * tess.scaleX;
		inst->times[3] = (ent->e.oldframe / tess.framesPerRow) * tess.scaleY +
			(ent->e.oldframe % tess.framesPerRow) * tess.scaleX;
	} else {
		inst->times[1] = 0.0f;
		inst->times[2] = 0.0f;
		inst->times[3] = 0.0f;
	}
	if( ent->e.reType == RT_MODEL ) {
		inst->transX[0] = ent->e.axis[0][0];
		inst->transX[1] = ent->e.axis[1][0];
		inst->transX[2] = ent->e.axis[2][0];
		inst->transX[3] = ent->e.origin[0];
		inst->transY[0] = ent->e.axis[0][1];
		inst->transY[1] = ent->e.axis[1][1];
		inst->transY[2] = ent->e.axis[2][1];
		inst->transY[3] = ent->e.origin[1];
		inst->transZ[0] = ent->e.axis[0][2];
		inst->transZ[1] = ent->e.axis[1][2];
		inst->transZ[2] = ent->e.axis[2][2];
		inst->transZ[3] = ent->e.origin[2];
	} else {
		inst->transX[0] = 1.0f;
		inst->transX[1] = 0.0f;
		inst->transX[2] = 0.0f;
		inst->transX[3] = 0.0f;
		inst->transY[0] = 0.0f;
		inst->transY[1] = 1.0f;
		inst->transY[2] = 0.0f;
		inst->transY[3] = 0.0f;
		inst->transZ[0] = 0.0f;
		inst->transZ[1] = 0.0f;
		inst->transZ[2] = 1.0f;
		inst->transZ[3] = 0.0f;
	}
	if( ent->lightingCalculated ) {
		inst->lightDir[0] = ent->lightDir[0];
		inst->lightDir[1] = ent->lightDir[1];
		inst->lightDir[2] = ent->lightDir[2];
		inst->lightDir[3] = tr.deluxeOffset;
		inst->ambLight[0] = ent->ambientLight[0] / 255.0f;
		inst->ambLight[1] = ent->ambientLight[1] / 255.0f;
		inst->ambLight[2] = ent->ambientLight[2] / 255.0f;
		inst->ambLight[3] = 0.0f;
		inst->dirLight[0] = ent->directedLight[0] / 255.0f;
		inst->dirLight[1] = ent->directedLight[1] / 255.0f;
		inst->dirLight[2] = ent->directedLight[2] / 255.0f;
	} else {
		inst->lightDir[0] = tr.sunDirection[0];
		inst->lightDir[1] = tr.sunDirection[1];
		inst->lightDir[2] = tr.sunDirection[2];
		inst->lightDir[3] = tr.deluxeOffset;
		inst->ambLight[0] = 0.0f;
		inst->ambLight[1] = 0.0f;
		inst->ambLight[2] = 0.0f;
		inst->ambLight[3] = 0.0f;
		inst->dirLight[0] = 0.0f;
		inst->dirLight[1] = 0.0f;
		inst->dirLight[2] = 0.0f;
	}
	inst->texCoord[0] = ent->e.shaderTexCoord[0];
	inst->texCoord[1] = ent->e.shaderTexCoord[1];
	inst->texCoord[2] = tess.fogNum;
	inst->texCoord[3] = 0.0f;
	inst->color[0] = ent->e.shaderRGBA[0];
	inst->color[1] = ent->e.shaderRGBA[1];
	inst->color[2] = ent->e.shaderRGBA[2];
	inst->color[3] = ent->e.shaderRGBA[3];
}

static ID_INLINE trRefEntity_t *getEntity( int entityNum ) {
	if ( entityNum != ENTITYNUM_WORLD ) {
		return &backEnd.refdef.entities[entityNum];
	} else {
		return &tr.worldEntity;
	}
}

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t	*shader, *oldShader;
	int		fogNum, oldFogNum;
	int		entityNum, oldEntityNum;
	int		dlighted, oldDlighted;
	qboolean	depthRange, oldDepthRange, isCrosshair, wasCrosshair, worldMatrix;
	int		i, j;
	int		startBatch, startMixed, startLight, endBatch;
	int		oldSort;
	float		originalTime;
	qboolean	isGLSL = qfalse;
	trRefEntity_t	*ent;

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView ();

	// draw everything
	oldEntityNum = -1;
	ent = backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
	oldDlighted = qfalse;
	oldSort = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;

	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	worldMatrix = qtrue;

	for( i = 0; i < numDrawSurfs ; ) {
#ifndef NDEBUG
		size_t  scratchCheck = freeScratch;
#endif

		// build a batch and count vertices
		entityNum = QSORT_ENTITYNUM( drawSurfs[i].sort );
		shader = tr.shaders[ drawSurfs[i].shaderIndex ];
		fogNum = QSORT_FOGNUM( drawSurfs[i].sort );
		dlighted = QSORT_DLIGHT( drawSurfs[i].sort );

		isGLSL = (shader->optimalStageIteratorFunc == RB_StageIteratorGLSL);
		backEnd.currentEntity = ent = getEntity( entityNum );

		RB_BeginSurface( shader );

		// the first surface is always part of the batch
		oldSort = drawSurfs[i].sort;
		tesselate( TESS_COUNT, drawSurfs[i].surface );

		// set up the initial instance data
		tess.numInstances = 1;
		RB_InstanceFromEntity( ent );

		startBatch = i;
		startMixed = ( QSORT_FOGNUM( drawSurfs[i].sort ) ||
			       QSORT_DLIGHT( drawSurfs[i].sort ) )
			? i : -1;
		startLight = ( QSORT_DLIGHT( drawSurfs[i].sort ) == 2 )
			? i : -1;
		endBatch   = -1;
		oldSort    = -1;

		// find the end of the current batch
		for( i++; i < numDrawSurfs; i++ ) {
			if( drawSurfs[i].sort == oldSort ) {
				tesselate( TESS_COUNT, drawSurfs[i].surface );
				continue;
			}

			if( drawSurfs[i].shaderIndex != shader->index )
				break;

			if( QSORT_ENTITYNUM( drawSurfs[i].sort ) != entityNum
			    && ent->e.reType != RT_SPRITE ) {
				// look for instancing opportunities:
				// if the same surfaces are rendered again,
				// only the entity is different
				qboolean	useInstance = qtrue;
				int		nextEnt = QSORT_ENTITYNUM( drawSurfs[i].sort );

				if( shader->lightmapIndex != LIGHTMAP_MD3 ) {
					useInstance = qfalse;
					break;
				}

				for( j = 0; j < numDrawSurfs - i; j++ ) {
					if( QSORT_ENTITYNUM( drawSurfs[i+j].sort ) != nextEnt )
						break;
					if( drawSurfs[i+j].surface != drawSurfs[startBatch+j].surface ) {
						useInstance = qfalse;
						break;
					}
				}
				if( useInstance ) {
					entityNum = QSORT_ENTITYNUM( drawSurfs[i].sort );
					backEnd.currentEntity = ent = getEntity( entityNum );

					if( endBatch < 0 )
						endBatch = i;
					tess.numInstances++;
					RB_InstanceFromEntity( ent );
					i += j - 1;
					continue;
				}
				break;
			}

			if( startMixed < 0 &&
			    ( QSORT_FOGNUM( drawSurfs[i].sort ) ||
			      QSORT_DLIGHT( drawSurfs[i].sort ) ) ) {
				startMixed = i;
			}

			if( startLight < 0 &&
			    QSORT_DLIGHT( drawSurfs[i].sort ) == 2 ) {
				startLight = i;
			}

			oldSort = drawSurfs[i].sort;
			tesselate( TESS_COUNT, drawSurfs[i].surface );
		}

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != ENTITYNUM_WORLD ) {
				backEnd.refdef.floatTime = originalTime - ent->e.shaderTime;
				// we have to reset the shaderTime as well otherwise image animations start
				// from the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

				// set up the transformation matrix
				R_RotateForEntity( ent, &backEnd.viewParms, &backEnd.or );
				
				// set up the dynamic lighting if needed
				if ( ent->needDlights ) {
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
				}
				if( !isGLSL ) {
					qglLoadMatrixf( backEnd.or.modelMatrix );
					worldMatrix = qfalse;
					
				} else if( !worldMatrix ) {
					qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
					worldMatrix = qtrue;
				}

				if( ent->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;
					
					if( ent->e.renderfx & RF_CROSSHAIR )
						isCrosshair = qtrue;
				}
			} else {
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
				// we have to reset the shaderTime as well otherwise image animations on
				// the world (like water) continue with the wrong frame
				tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
				if( !worldMatrix ) {
					qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
					worldMatrix = qtrue;
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
				}
			}

			//
			// change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//
			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}
				}

				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}

			oldEntityNum = entityNum;
		}
		if( endBatch < 0 )
			endBatch = i;
		if( startLight < 0 )
			startLight = endBatch;
		if( startMixed < 0 )
			startMixed = startLight;

		RB_AllocateSurface( );
		for( j = startBatch; j < startMixed; j++ ) {
			backEnd.currentEntity = getEntity( QSORT_ENTITYNUM( drawSurfs[j].sort ) );
			tesselate( TESS_ALL, drawSurfs[j].surface );
		}
		tess.indexRange = 1;
		tess.numIndexes[1] = tess.numIndexes[0];
		for( ; j < startLight; j++ ) {
			backEnd.currentEntity = getEntity( QSORT_ENTITYNUM( drawSurfs[j].sort ) );
			tess.fogNum = QSORT_FOGNUM( drawSurfs[j].sort );
			tesselate( TESS_ALL, drawSurfs[j].surface );
		}
		tess.indexRange = 2;
		tess.numIndexes[2] = tess.numIndexes[1];
		for( ; j < endBatch; j++ ) {
			backEnd.currentEntity = getEntity( QSORT_ENTITYNUM( drawSurfs[j].sort ) );
			tess.fogNum = QSORT_FOGNUM( drawSurfs[j].sort );
			tesselate( TESS_ALL, drawSurfs[j].surface );
		}

		RB_EndSurface();

		tess.dataTexture = NULL;
		tess.numInstances = 0;

		assert( scratchCheck == freeScratch );
	}

	backEnd.refdef.floatTime = originalTime;

	// go back to the world modelview matrix
	if( !worldMatrix ) {
		qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	}

	if( !backEnd.viewParms.noShadows ) {
#if 0
		RB_DrawSun();
#endif
		// darken down any stencil shadows
		RB_ShadowFinish();

		// add light flares on lights that aren't obscured
		RB_RenderFlares();
	}
}


/*
============================================================================

RENDER BACK END THREAD FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D

================
*/
void	RB_SetGL2D ( glRenderState_t *state ) {
	backEnd.projection2D = qtrue;
	backEnd.currentEntity = &backEnd.entity2D;

	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode(GL_PROJECTION);
    qglLoadIdentity ();
	qglOrtho (0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1);
	qglMatrixMode(GL_MODELVIEW);
    qglLoadIdentity ();

	state->stateBits = GLS_DEPTHTEST_DISABLE |
		GLS_SRCBLEND_SRC_ALPHA |
		GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	state->faceCulling = CT_TWO_SIDED;

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = backEnd.refdef.time * 0.001f;
}


/*
=============
RB_StretchRaw

Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
const void *RB_StretchRaw ( const void *data ) {
	const stretchRawCommand_t	*cmd;
	glRenderState_t			state;

	cmd = (const stretchRawCommand_t *)data;

	// we definately want to sync every frame for the cinematics
	qglFinish();

	RB_SetGL2D( &state );
	RB_BeginSurface( tr.defaultShader );

	tess.numVertexes   = 4;
	tess.numIndexes[0] = 6;

	RB_AllocateSurface( );

	tess.numVertexes   = 4;
	tess.numIndexes[0] = 6;
	tess.minIndex[0]   = 0;
	tess.maxIndex[0]   = 3;

	tess.vertexPtr[0].xyz[0] = cmd->x;
	tess.vertexPtr[0].xyz[1] = cmd->y;
	tess.vertexPtr[0].xyz[2] = 0.0f;
	tess.vertexPtr[0].fogNum = 0.0f;
	tess.vertexPtr[0].tc1[0] = cmd->s1;
	tess.vertexPtr[0].tc1[1] = cmd->t1;
	tess.vertexPtr[0].tc2[0] = cmd->s1;
	tess.vertexPtr[0].tc2[1] = cmd->t1;
	tess.vertexPtr[0].normal[0] = 0.0f;
	tess.vertexPtr[0].normal[1] = 0.0f;
	tess.vertexPtr[0].normal[2] = 0.0f;
	tess.vertexPtr[0].color[0] = tr.identityLightByte;
	tess.vertexPtr[0].color[1] = tr.identityLightByte;
	tess.vertexPtr[0].color[2] = tr.identityLightByte;
	tess.vertexPtr[0].color[3] = 255;

	tess.vertexPtr[1].xyz[0] = cmd->x + cmd->w;
	tess.vertexPtr[1].xyz[1] = cmd->y;
	tess.vertexPtr[1].xyz[2] = 0.0f;
	tess.vertexPtr[1].fogNum = 0.0f;
	tess.vertexPtr[1].tc1[0] = cmd->s2;
	tess.vertexPtr[1].tc1[1] = cmd->t1;
	tess.vertexPtr[1].tc2[0] = cmd->s2;
	tess.vertexPtr[1].tc2[1] = cmd->t1;
	tess.vertexPtr[1].normal[0] = 0.0f;
	tess.vertexPtr[1].normal[1] = 0.0f;
	tess.vertexPtr[1].normal[2] = 0.0f;
	tess.vertexPtr[1].color[0] = tr.identityLightByte;
	tess.vertexPtr[1].color[1] = tr.identityLightByte;
	tess.vertexPtr[1].color[2] = tr.identityLightByte;
	tess.vertexPtr[1].color[3] = 255;

	tess.vertexPtr[2].xyz[0] = cmd->x + cmd->w;
	tess.vertexPtr[2].xyz[1] = cmd->y + cmd->h;
	tess.vertexPtr[2].xyz[2] = 0.0f;
	tess.vertexPtr[2].fogNum = 0.0f;
	tess.vertexPtr[2].tc1[0] = cmd->s2;
	tess.vertexPtr[2].tc1[1] = cmd->t2;
	tess.vertexPtr[2].tc2[0] = cmd->s2;
	tess.vertexPtr[2].tc2[1] = cmd->t2;
	tess.vertexPtr[2].normal[0] = 0.0f;
	tess.vertexPtr[2].normal[1] = 0.0f;
	tess.vertexPtr[2].normal[2] = 0.0f;
	tess.vertexPtr[2].color[0] = tr.identityLightByte;
	tess.vertexPtr[2].color[1] = tr.identityLightByte;
	tess.vertexPtr[2].color[2] = tr.identityLightByte;
	tess.vertexPtr[2].color[3] = 255;

	tess.vertexPtr[3].xyz[0] = cmd->x;
	tess.vertexPtr[3].xyz[1] = cmd->y + cmd->h;
	tess.vertexPtr[3].xyz[2] = 0.0f;
	tess.vertexPtr[3].fogNum = 0.0f;
	tess.vertexPtr[3].tc1[0] = cmd->s1;
	tess.vertexPtr[3].tc1[1] = cmd->t2;
	tess.vertexPtr[3].tc2[0] = cmd->s1;
	tess.vertexPtr[3].tc2[1] = cmd->t2;
	tess.vertexPtr[3].normal[0] = 0.0f;
	tess.vertexPtr[3].normal[1] = 0.0f;
	tess.vertexPtr[3].normal[2] = 0.0f;
	tess.vertexPtr[3].color[0] = tr.identityLightByte;
	tess.vertexPtr[3].color[1] = tr.identityLightByte;
	tess.vertexPtr[3].color[2] = tr.identityLightByte;
	tess.vertexPtr[3].color[3] = 255;

	tess.indexPtr[0] = quadIndexes[0];
	tess.indexPtr[1] = quadIndexes[1];
	tess.indexPtr[2] = quadIndexes[2];
	tess.indexPtr[3] = quadIndexes[3];
	tess.indexPtr[4] = quadIndexes[4];
	tess.indexPtr[5] = quadIndexes[5];

	tess.numInstances = 1;
	tess.instances[0].times[0] = tess.shaderTime;
	tess.instances[0].times[1] = 0.0f;
	tess.instances[0].times[2] = 0.0f;
	tess.instances[0].times[3] = 0.0f;
	tess.instances[0].transX[0] = 1.0f;
	tess.instances[0].transX[1] = 0.0f;
	tess.instances[0].transX[2] = 0.0f;
	tess.instances[0].transX[3] = 0.0f;
	tess.instances[0].transY[0] = 0.0f;
	tess.instances[0].transY[1] = 1.0f;
	tess.instances[0].transY[2] = 0.0f;
	tess.instances[0].transY[3] = 0.0f;
	tess.instances[0].transZ[0] = 0.0f;
	tess.instances[0].transZ[1] = 0.0f;
	tess.instances[0].transZ[2] = 1.0f;
	tess.instances[0].transZ[3] = 0.0f;

	tess.imgOverride = tr.scratchImage[cmd->client];
	RB_EndSurface( );
	tess.imgOverride = NULL;

	return cmd + 1;
}

void RE_UploadCinematic (int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {
	qboolean redefine = cols != tr.scratchImage[client]->width
		|| rows != tr.scratchImage[client]->height;

	GL_BindTexture( tr.scratchImage[client]->texnum );

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if( redefine ) {
		tr.scratchImage[client]->width = tr.scratchImage[client]->uploadWidth = cols;
		tr.scratchImage[client]->height = tr.scratchImage[client]->uploadHeight = rows;
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_RGB8, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );	
	} else if( dirty ) {
		// otherwise, just subimage upload it so that drivers can tell we are going to be changing
		// it and don't try and do a texture compression
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
	}
}


/*
=============
RB_SetColor

=============
*/
const void	*RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D[0] = cmd->color[0] * 255;
	backEnd.color2D[1] = cmd->color[1] * 255;
	backEnd.color2D[2] = cmd->color[2] * 255;
	backEnd.color2D[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}

/*
=============
RB_StretchPic
=============
*/
static qboolean overlap(const stretchPicCommand_t *a,
			const stretchPicCommand_t *b ) {
	if( a->x >= b->x + b->w || b->x >= a->x + a->w )
		return qfalse;

	if( a->y >= b->y + b->h || b->y >= a->y + a->h )
		return qfalse;

	return qtrue;
}
const void *RB_StretchPic ( const void *data ) {
	struct pic {
		const stretchPicCommand_t	*cmd;
		color4ub_t			color;
	} *pics;
	const stretchPicCommand_t	*cmd;
	shader_t	*shader;
	int		i, j, k, n;

	cmd = (const stretchPicCommand_t *)data;

	if ( !backEnd.projection2D ) {
		glRenderState_t dummy;
		RB_SetGL2D( &dummy );
	}

	backEnd.currentEntity = &backEnd.entity2D;
	
	// read all the following StretchPic commands
	n = 1;
	data = cmd + 1;
	for(;;) {
		data = PADP( data, sizeof( void * ) );
		if ( *(int *)data == RC_STRETCH_PIC ) {
			n++;
			data = (byte *)data + sizeof(stretchPicCommand_t);
		} else if ( *(int *)data == RC_SET_COLOR ) {
			data = (byte *)data + sizeof(setColorCommand_t);
		} else
			break;
	}

	// add current color to each pic
	pics = (struct pic *)RB_AllocScratch(n * sizeof(struct pic));
	for( i = 0; ; ) {
		if( cmd->commandId == RC_STRETCH_PIC ) {
			pics[i].cmd = cmd;
			*(int *)&pics[i].color = *(int *)&backEnd.color2D;
			i++; cmd++;
		} else if( cmd->commandId == RC_SET_COLOR ) {
			cmd = RB_SetColor( cmd );
		} else
			break;
		cmd = PADP( cmd, sizeof( void * ) );
	}

	// try to reorder by shader
	// as there is no Z-order we must be careful to not swap pics that
	// overlap
	shader = pics[0].cmd->shader;
	for( i = 1; i < n ; i++ ) {
		for( j = i; j < n; j++ ) {
			if( pics[j].cmd->shader == shader ) {
				while( j > i &&
				       !overlap( pics[j].cmd, pics[j-1].cmd ) ) {
					struct pic tmp = pics[j];
					pics[j] = pics[j-1];
					pics[j-1] = tmp;
					j--;
				}
				break;
			}
		}
		shader = pics[i].cmd->shader;
	}

	// draw maximal batches
	for( i = 0; i < n; ) {
		shader = pics[i].cmd->shader;
		for( j = i+1; j < n; j++ ) {
			if( pics[j].cmd->shader != shader )
				break;
		}

		j -= i;
		RB_BeginSurface( shader );
		tess.numVertexes = 4 * j;
		tess.numIndexes[0] = 6 * j;

		RB_AllocateSurface( );

		tess.numVertexes   = 4 * j;
		tess.numIndexes[0] = 6 * j;
		tess.minIndex[0]   = 0;
		tess.maxIndex[0]   = tess.numVertexes - 1;

		for( k = 0; k < j; k++, i++ ) {
			tess.indexPtr[6*k+0] = quadIndexes[0] + 4*k;
			tess.indexPtr[6*k+1] = quadIndexes[1] + 4*k;
			tess.indexPtr[6*k+2] = quadIndexes[2] + 4*k;
			tess.indexPtr[6*k+3] = quadIndexes[3] + 4*k;
			tess.indexPtr[6*k+4] = quadIndexes[4] + 4*k;
			tess.indexPtr[6*k+5] = quadIndexes[5] + 4*k;

			*(int *)(&tess.vertexPtr[4*k+0].color) =
			*(int *)(&tess.vertexPtr[4*k+1].color) =
			*(int *)(&tess.vertexPtr[4*k+2].color) =
			*(int *)(&tess.vertexPtr[4*k+3].color) = *(int *)&pics[i].color;

			tess.vertexPtr[4*k+0].xyz[0] = pics[i].cmd->x;
			tess.vertexPtr[4*k+0].xyz[1] = pics[i].cmd->y;
			tess.vertexPtr[4*k+0].xyz[2] = 0.0f;
			tess.vertexPtr[4*k+0].fogNum = 0.0f;
			tess.vertexPtr[4*k+0].tc1[0] = pics[i].cmd->s1;
			tess.vertexPtr[4*k+0].tc1[1] = pics[i].cmd->t1;
			tess.vertexPtr[4*k+0].tc2[0] = pics[i].cmd->s1;
			tess.vertexPtr[4*k+0].tc2[1] = pics[i].cmd->t1;

			tess.vertexPtr[4*k+1].xyz[0] = pics[i].cmd->x + pics[i].cmd->w;
			tess.vertexPtr[4*k+1].xyz[1] = pics[i].cmd->y;
			tess.vertexPtr[4*k+1].xyz[2] = 0.0f;
			tess.vertexPtr[4*k+1].fogNum = 0.0f;
			tess.vertexPtr[4*k+1].tc1[0] = pics[i].cmd->s2;
			tess.vertexPtr[4*k+1].tc1[1] = pics[i].cmd->t1;
			tess.vertexPtr[4*k+1].tc2[0] = pics[i].cmd->s2;
			tess.vertexPtr[4*k+1].tc2[1] = pics[i].cmd->t1;

			tess.vertexPtr[4*k+2].xyz[0] = pics[i].cmd->x + pics[i].cmd->w;
			tess.vertexPtr[4*k+2].xyz[1] = pics[i].cmd->y + pics[i].cmd->h;
			tess.vertexPtr[4*k+2].xyz[2] = 0.0f;
			tess.vertexPtr[4*k+2].fogNum = 0.0f;
			tess.vertexPtr[4*k+2].tc1[0] = pics[i].cmd->s2;
			tess.vertexPtr[4*k+2].tc1[1] = pics[i].cmd->t2;
			tess.vertexPtr[4*k+2].tc2[0] = pics[i].cmd->s2;
			tess.vertexPtr[4*k+2].tc2[1] = pics[i].cmd->t2;

			tess.vertexPtr[4*k+3].xyz[0] = pics[i].cmd->x;
			tess.vertexPtr[4*k+3].xyz[1] = pics[i].cmd->y + pics[i].cmd->h;
			tess.vertexPtr[4*k+3].xyz[2] = 0.0f;
			tess.vertexPtr[4*k+3].fogNum = 0.0f;
			tess.vertexPtr[4*k+3].tc1[0] = pics[i].cmd->s1;
			tess.vertexPtr[4*k+3].tc1[1] = pics[i].cmd->t2;
			tess.vertexPtr[4*k+3].tc2[0] = pics[i].cmd->s1;
			tess.vertexPtr[4*k+3].tc2[1] = pics[i].cmd->t2;
		}
		tess.numInstances = 1;
		tess.instances[0].times[0] = tess.shaderTime;
		tess.instances[0].times[1] = 0.0f;
		tess.instances[0].times[2] = 0.0f;
		tess.instances[0].times[3] = 0.0f;
		tess.instances[0].transX[0] = 1.0f;
		tess.instances[0].transX[1] = 0.0f;
		tess.instances[0].transX[2] = 0.0f;
		tess.instances[0].transX[3] = 0.0f;
		tess.instances[0].transY[0] = 0.0f;
		tess.instances[0].transY[1] = 1.0f;
		tess.instances[0].transY[2] = 0.0f;
		tess.instances[0].transY[3] = 0.0f;
		tess.instances[0].transZ[0] = 0.0f;
		tess.instances[0].transZ[1] = 0.0f;
		tess.instances[0].transZ[2] = 1.0f;
		tess.instances[0].transZ[3] = 0.0f;

		tess.instances[0].color[0] = 0;
		tess.instances[0].color[1] = 0;
		tess.instances[0].color[2] = 0;
		tess.instances[0].color[3] = 0;

		RB_EndSurface ();
	}

	RB_FreeScratch( pics );
	return (const void *)cmd;
}


/*
=============
RB_DrawSurfs

=============
*/
const void	*RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t	*cmd;

	cmd = (const drawSurfsCommand_t *)data;

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;

	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer

=============
*/
const void	*RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

	qglDrawBuffer( cmd->buffer );

	return (const void *)(cmd + 1);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages( void ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;
	glRenderState_t	state;

	InitState( &state );
	if ( !backEnd.projection2D ) {
		RB_SetGL2D( &state );
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	start = ri.Milliseconds();

	for ( i=0 ; i<tr.numImages ; i++ ) {
		image = tr.images[i];

		RB_BeginSurface( tr.defaultShader );

		tess.numVertexes   = 4;
		tess.numIndexes[0] = 6;

		RB_AllocateSurface( );

		tess.numVertexes   = 4;
		tess.numIndexes[0] = 6;
		tess.minIndex[0]   = 0;
		tess.maxIndex[0]   = 3;

		w = glConfig.vidWidth / 40;
		h = glConfig.vidHeight / 30;
		x = i % 40 * w;
		y = i / 40 * h;

		tess.vertexPtr[0].xyz[0] = x;
		tess.vertexPtr[0].xyz[1] = y;
		tess.vertexPtr[0].xyz[2] = 0.0f;
		tess.vertexPtr[0].fogNum = 0.0f;
		tess.vertexPtr[0].tc1[0] = 0.0f;
		tess.vertexPtr[0].tc1[1] = 0.0f;
		tess.vertexPtr[0].tc2[0] = 0.0f;
		tess.vertexPtr[0].tc2[1] = 0.0f;
		tess.vertexPtr[0].normal[0] = 0.0f;
		tess.vertexPtr[0].normal[1] = 0.0f;
		tess.vertexPtr[0].normal[2] = 0.0f;
		tess.vertexPtr[0].color[0] = tr.identityLightByte;
		tess.vertexPtr[0].color[1] = tr.identityLightByte;
		tess.vertexPtr[0].color[2] = tr.identityLightByte;
		tess.vertexPtr[0].color[3] = 255;

		tess.vertexPtr[1].xyz[0] = x + w;
		tess.vertexPtr[1].xyz[1] = y;
		tess.vertexPtr[1].xyz[2] = 0.0f;
		tess.vertexPtr[1].fogNum = 0.0f;
		tess.vertexPtr[1].tc1[0] = 1.0f;
		tess.vertexPtr[1].tc1[1] = 0.0f;
		tess.vertexPtr[1].tc2[0] = 1.0f;
		tess.vertexPtr[1].tc2[1] = 0.0f;
		tess.vertexPtr[1].normal[0] = 0.0f;
		tess.vertexPtr[1].normal[1] = 0.0f;
		tess.vertexPtr[1].normal[2] = 0.0f;
		tess.vertexPtr[1].color[0] = tr.identityLightByte;
		tess.vertexPtr[1].color[1] = tr.identityLightByte;
		tess.vertexPtr[1].color[2] = tr.identityLightByte;
		tess.vertexPtr[1].color[3] = 255;

		tess.vertexPtr[2].xyz[0] = x + w;
		tess.vertexPtr[2].xyz[1] = y + h;
		tess.vertexPtr[2].xyz[2] = 0.0f;
		tess.vertexPtr[2].fogNum = 0.0f;
		tess.vertexPtr[2].tc1[0] = 1.0f;
		tess.vertexPtr[2].tc1[1] = 1.0f;
		tess.vertexPtr[2].tc2[0] = 1.0f;
		tess.vertexPtr[2].tc2[1] = 1.0f;
		tess.vertexPtr[2].normal[0] = 0.0f;
		tess.vertexPtr[2].normal[1] = 0.0f;
		tess.vertexPtr[2].normal[2] = 0.0f;
		tess.vertexPtr[2].color[0] = tr.identityLightByte;
		tess.vertexPtr[2].color[1] = tr.identityLightByte;
		tess.vertexPtr[2].color[2] = tr.identityLightByte;
		tess.vertexPtr[2].color[3] = 255;

		tess.vertexPtr[3].xyz[0] = x;
		tess.vertexPtr[3].xyz[1] = y + h;
		tess.vertexPtr[3].xyz[2] = 0.0f;
		tess.vertexPtr[3].fogNum = 0.0f;
		tess.vertexPtr[3].tc1[0] = 0.0f;
		tess.vertexPtr[3].tc1[1] = 1.0f;
		tess.vertexPtr[3].tc2[0] = 0.0f;
		tess.vertexPtr[3].tc2[1] = 1.0f;
		tess.vertexPtr[3].normal[0] = 0.0f;
		tess.vertexPtr[3].normal[1] = 0.0f;
		tess.vertexPtr[3].normal[2] = 0.0f;
		tess.vertexPtr[3].color[0] = tr.identityLightByte;
		tess.vertexPtr[3].color[1] = tr.identityLightByte;
		tess.vertexPtr[3].color[2] = tr.identityLightByte;
		tess.vertexPtr[3].color[3] = 255;

		tess.indexPtr[0] = quadIndexes[0];
		tess.indexPtr[1] = quadIndexes[1];
		tess.indexPtr[2] = quadIndexes[2];
		tess.indexPtr[3] = quadIndexes[3];
		tess.indexPtr[4] = quadIndexes[4];
		tess.indexPtr[5] = quadIndexes[5];

		tess.imgOverride = image;
		RB_EndSurface( );
		tess.imgOverride = NULL;
	}
	qglFinish();

	end = ri.Milliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );

}

/*
=============
RB_ColorMask

=============
*/
const void *RB_ColorMask(const void *data)
{
	const colorMaskCommand_t *cmd = data;
	
	qglColorMask(cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
	
	return (const void *)(cmd + 1);
}

/*
=============
RB_ClearDepth

=============
*/
const void *RB_ClearDepth(const void *data)
{
	const clearDepthCommand_t *cmd = data;
	
	// texture swapping test
	if (r_showImages->integer)
		RB_ShowImages();

	qglClear(GL_DEPTH_BUFFER_BIT);
	
	return (const void *)(cmd + 1);
}

/*
=============
RB_SwapBuffers

=============
*/
const void	*RB_SwapBuffers( const void *data ) {
	const swapBuffersCommand_t	*cmd;

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	// we measure overdraw by reading back the stencil buffer and
	// counting up the number of increments that have happened
	if ( r_measureOverdraw->integer ) {
		int i;
		long sum = 0;
		unsigned char *stencilReadback;

		stencilReadback = RB_AllocScratch( glConfig.vidWidth * glConfig.vidHeight );
		qglReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencilReadback );

		for ( i = 0; i < glConfig.vidWidth * glConfig.vidHeight; i++ ) {
			sum += stencilReadback[i] & glGlobals.shadowMask;
		}

		backEnd.pc.c_overDraw += sum;
		RB_FreeScratch( stencilReadback );
	}


	if ( !glState.finishCalled ) {
		qglFinish();
	}

	GLimp_LogComment( "***************** RB_SwapBuffers *****************\n\n\n" );

	GLimp_EndFrame();

	if ( qglGenQueriesARB && r_ext_occlusion_query->integer ) {
		int shader;

		for( shader = 0; shader < tr.numGLSLprograms; shader++ ) {
			tr.GLSLprograms[shader]->QuerySum = 0;
		}

		for( shader = 0; shader < tr.numShaders; shader++ ) {
			if ( tr.shaders[shader]->QueryID ) {
				GL_GetQuery( tr.shaders[shader]->QueryID,
					     &tr.shaders[shader]->QueryResult );
			}
			if( tr.shaders[shader]->GLSLprogram ) {
				tr.shaders[shader]->GLSLprogram->QuerySum += QUERY_RESULT(&tr.shaders[shader]->QueryResult);
				if( tr.shaders[shader]->GLSLprogram->QuerySum > QUERY_MASK )
					tr.shaders[shader]->GLSLprogram->QuerySum = QUERY_MASK;
			}
		}
	}

	backEnd.projection2D = qfalse;
	
	return (const void *)(cmd + 1);
}

/*
====================
RB_ExecuteRenderCommands

This function will be called synchronously if running without
smp extensions, or asynchronously by another thread.
====================
*/
void RB_ExecuteRenderCommands( const void *data ) {
	int		t1, t2;

	t1 = ri.Milliseconds ();

	if ( !r_smp->integer || data == backEndData[0]->commands.cmds ) {
		backEnd.smpFrame = 0;
	} else {
		backEnd.smpFrame = 1;
	}

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_STRETCH_RAW:
			//Check if it's time for BLOOM!
			data = RB_StretchRaw( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_SCREENSHOT:
			data = RB_TakeScreenshotCmd( data );
			break;
		case RC_VIDEOFRAME:
			data = RB_TakeVideoFrameCmd( data );
			break;
		case RC_COLORMASK:
			data = RB_ColorMask(data);
			break;
		case RC_CLEARDEPTH:
			data = RB_ClearDepth(data);
			break;
		case RC_END_OF_LIST:
		default:
			// stop rendering on this thread
			t2 = ri.Milliseconds ();
			backEnd.pc.msec = t2 - t1;
			return;
		}
	}

}


/*
================
RB_RenderThread
================
*/
void RB_RenderThread( void ) {
	const void	*data;

	// set default state
	GL_SetDefaultState();

	// wait for either a rendering command or a quit command
	while ( 1 ) {
		// sleep until we have work to do
		data = GLimp_RendererSleep();

		if ( !data ) {
			return;	// all done, renderer is shutting down
		}

		renderThreadActive = qtrue;

		RB_ExecuteRenderCommands( data );

		renderThreadActive = qfalse;
	}
}

