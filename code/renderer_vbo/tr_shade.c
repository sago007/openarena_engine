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
// tr_shade.c

#include TR_CONFIG_H
#include TR_LOCAL_H

#if idppc_altivec && !defined(MACOS_X)
#include <altivec.h>
#endif

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

static const size_t	vertexSize = sizeof(glVertex_t);
static ID_INLINE void *BufferOffset(GLsizei offs) {
	return offs + (byte *)NULL;
}

/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;

/*
=================
R_BindAnimatedImage

=================
*/
static void R_GetAnimatedImage( textureBundle_t *bundle, qboolean combined,
				image_t **pImage ) {
	int		index;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		*pImage = bundle->image[0];
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		*pImage = bundle->image[0];
		return;
	}

	if ( combined && bundle->combinedImage ) {
		*pImage = bundle->combinedImage;
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	index = ri.ftol(tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE);
	index >>= FUNCTABLE_SIZE2;

	if ( index < 0 ) {
		index = 0;	// may happen with shader time offsets
	}
	index %= bundle->numImageAnimations;

	*pImage = bundle->image[ index ];
}

/*
================
DrawTris

Draws triangle outlines for debugging
This requires that all vertex pointers etc. are still bound from the StageIterator
================
*/
static void DrawTris ( glRenderState_t *state,
		       int numIndexes, GLuint IBO, const glIndex_t *indexes,
		       glIndex_t start, glIndex_t end,
		       float r, float g, float b ) {
	state->stateBits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE |
		GLS_DEPTHRANGE_0_TO_0;

	state->numImages = 1;
	state->image[0] = tr.whiteImage;

	if ( tess.currentStageIteratorFunc == RB_StageIteratorGLSL ) {
		shader_t *shader = tr.defaultShader;
		if( tess.dataTexture ) {
			shader = tr.defaultMD3Shader;
			state->numImages = 2;
			state->image[1] = tess.dataTexture;
		}
		state->program = shader->GLSLprogram;
	}
	SetAttrVec4f( state, AL_COLOR, r, g, b, 1.0f );
	
	GL_DrawElements( state, numIndexes, IBO, indexes, start, end );
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals ( glRenderState_t *state ) {
	int		i;
	vec3_t		*temp;
	iboInfo_t	*ibo;

	if( backEnd.projection2D )
		return;

	if( backEnd.normalProgram && tess.currentStageIteratorFunc == RB_StageIteratorGLSL ) {
		for( ibo = tess.firstIBO; ibo; ibo = ibo->next ) {
			vboInfo_t	*vbo = ibo->vbo;

			if( backEnd.currentEntity->e.reType != RT_MODEL ) {
				SetAttrVec4f( state, AL_TRANSX,
					      1.0f, 0.0f, 0.0f, 0.0f );
				SetAttrVec4f( state, AL_TRANSY,
					      0.0f, 1.0f, 0.0f, 0.0f );
				SetAttrVec4f( state, AL_TRANSZ,
					      0.0f, 0.0f, 1.0f, 0.0f );
			} else {
				SetAttrVec4f( state, AL_TRANSX,
					      backEnd.currentEntity->e.axis[0][0],
					      backEnd.currentEntity->e.axis[1][0],
					      backEnd.currentEntity->e.axis[2][0],
					      backEnd.currentEntity->e.origin[0] );
				SetAttrVec4f( state, AL_TRANSY,
					      backEnd.currentEntity->e.axis[0][1],
					      backEnd.currentEntity->e.axis[1][1],
					      backEnd.currentEntity->e.axis[2][1],
					      backEnd.currentEntity->e.origin[1] );
				SetAttrVec4f( state, AL_TRANSZ,
						   backEnd.currentEntity->e.axis[0][2],
						   backEnd.currentEntity->e.axis[1][2],
						   backEnd.currentEntity->e.axis[2][2],
						   backEnd.currentEntity->e.origin[2] );
			}

			if( !tess.dataTexture ) {
				state->program = backEnd.normalProgram;
				SetAttrPointer( state, AL_NORMAL, vbo->vbo,
						3, GL_FLOAT, sizeof(glVertex_t),
						&vbo->offset->normal );
			} else {
				float frameOffs    = (backEnd.currentEntity->e.frame / tess.framesPerRow) * tess.scaleY +
					(backEnd.currentEntity->e.frame % tess.framesPerRow) * tess.scaleX;
				float oldFrameOffs = (backEnd.currentEntity->e.oldframe / tess.framesPerRow) * tess.scaleY +
					(backEnd.currentEntity->e.oldframe % tess.framesPerRow) * tess.scaleX;
				state->program = backEnd.normalProgramMD3;
				state->numImages = 1;
				state->image[0] = tess.dataTexture;
				SetAttrVec4f( state, AL_TIMES,
					      tess.shaderTime,
					      backEnd.currentEntity->e.backlerp,
					      frameOffs,
					      oldFrameOffs );
			}
			state->stateBits = GLS_POLYMODE_LINE |
				GLS_DEPTHMASK_TRUE |
				GLS_DEPTHRANGE_0_TO_0;
			SetAttrVec4f( state, AL_COLOR, 1.0f, 1.0f, 1.0f, 1.0f );

			SetAttrPointer( state, AL_VERTEX, vbo->vbo,
					4, GL_FLOAT, sizeof(glVertex_t),
					&vbo->offset->xyz );
			GL_DrawArrays( state, GL_POINTS, ibo->minIndex,
				       ibo->maxIndex - ibo->minIndex + 1 );
		}

		if( tess.numVertexes > 0 ) {
			state->program = backEnd.normalProgram;
			state->numImages = 0;
			SetAttrVec4f( state, AL_COLOR, 1.0f, 1.0f, 1.0f, 1.0f );
			state->stateBits = GLS_POLYMODE_LINE |
				GLS_DEPTHMASK_TRUE |
				GLS_DEPTHRANGE_0_TO_0;

			if( backEnd.currentEntity->e.reType != RT_MODEL ) {
				SetAttrVec4f( state, AL_TRANSX,
					      1.0f, 0.0f, 0.0f, 0.0f );
				SetAttrVec4f( state, AL_TRANSY,
					      0.0f, 1.0f, 0.0f, 0.0f );
				SetAttrVec4f( state, AL_TRANSZ,
					      0.0f, 0.0f, 1.0f, 0.0f );
			} else {
				SetAttrVec4f( state, AL_TRANSX,
					      backEnd.currentEntity->e.axis[0][0],
					      backEnd.currentEntity->e.axis[1][0],
					      backEnd.currentEntity->e.axis[2][0],
					      backEnd.currentEntity->e.origin[0] );
				SetAttrVec4f( state, AL_TRANSY,
					      backEnd.currentEntity->e.axis[0][1],
					      backEnd.currentEntity->e.axis[1][1],
					      backEnd.currentEntity->e.axis[2][1],
					      backEnd.currentEntity->e.origin[1] );
				SetAttrVec4f( state, AL_TRANSZ,
					      backEnd.currentEntity->e.axis[0][2],
					      backEnd.currentEntity->e.axis[1][2],
					      backEnd.currentEntity->e.axis[2][2],
					      backEnd.currentEntity->e.origin[2] );
			}

			SetAttrPointer( state, AL_NORMAL, 0,
					3, GL_FLOAT, sizeof(glVertex_t),
					&tess.vertexPtr->normal );
			SetAttrPointer( state, AL_VERTEX, 0,
					4, GL_FLOAT, sizeof(glVertex_t),
					&tess.vertexPtr->xyz );
			GL_DrawArrays( state, GL_POINTS, 0, tess.numVertexes );
		}
	} else {
		if( tess.numVertexes > 0 ) {
			temp = RB_AllocScratch( tess.numVertexes * 2 * sizeof(vec3_t) );
			state->program = NULL;
			state->numImages = 0;
			SetAttrVec4f( state, AL_COLOR, 1.0f, 1.0f, 1.0f, 1.0f );
			state->stateBits = GLS_POLYMODE_LINE |
				GLS_DEPTHMASK_TRUE |
				GLS_DEPTHRANGE_0_TO_0;

			if( tess.currentStageIteratorFunc == RB_StageIteratorGLSL )
				qglLoadMatrixf( backEnd.or.modelMatrix );

			for (i = 0 ; i < tess.numVertexes ; i++) {
				VectorCopy( tess.vertexPtr[i].xyz, temp[2*i] );
				VectorMA( tess.vertexPtr[i].xyz, 2, tess.vertexPtr[i].normal, temp[2*i+1] );
			}
			SetAttrPointer( state, AL_VERTEX, 0,
					3, GL_FLOAT, sizeof(vec3_t),
					temp );
			GL_DrawArrays( state, GL_LINES, 0, 2 * tess.numVertexes );
			RB_FreeScratch( temp );
			
			if( tess.currentStageIteratorFunc == RB_StageIteratorGLSL )
				qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
		}
	}
}

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader ) {

	shader_t *state = shader;

	tess.indexPtr = NULL;
	tess.vertexPtr = NULL;
	tess.firstIBO = NULL;

	tess.numVertexes = tess.indexRange = 0;
	tess.numIndexes[0] = tess.numIndexes[1] = tess.numIndexes[2] = 0;
	tess.minIndex[0] = tess.minIndex[1] = tess.minIndex[2] = 0x7fffffff;
	tess.maxIndex[0] = tess.maxIndex[1] = tess.maxIndex[2] = 0;

	if( shader->remappedShader ) 
		shader = shader->remappedShader;

	tess.shader = state;
	tess.dlightBits = 0;		// will be OR'd in by surface functions
	tess.fogNum = 0;
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;
	tess.currentStageIteratorFunc = state->optimalStageIteratorFunc;
	tess.numInstances = 0;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime) {
		tess.shaderTime = tess.shader->clampTime;
	}
}

static void *RB_AllocateStreamBuffer(GLenum target, glStreamBuffer_t *buf, GLsizei amount)
{
	void *ptr;

	if( qglMapBufferRange ) {
		// use unsynchronized map_buffer_range to append to the end
		// or orphan the buffer and create a new one if not enough room
		if( amount > buf->size ) {
			for(buf->size = 0x10000; amount > buf->size; )
				buf->size *= 2;
			qglBufferDataARB( target, buf->size, NULL,
					  GL_STREAM_DRAW_ARB );
			buf->nextFree = 0;
		}

		if( amount + buf->nextFree > buf->size ) {
			buf->nextFree = 0;
			ptr = qglMapBufferRange( target, buf->nextFree, amount,
						 GL_MAP_WRITE_BIT |
						 GL_MAP_INVALIDATE_BUFFER_BIT );
		} else {
			ptr = qglMapBufferRange( target, buf->nextFree, amount,
						 GL_MAP_WRITE_BIT |
						 GL_MAP_INVALIDATE_RANGE_BIT |
						 GL_MAP_UNSYNCHRONIZED_BIT );
		}
		buf->mapped  = qtrue;
		buf->current = buf->nextFree;
		buf->nextFree += amount;
	} else {
		// always orphan and create a new buffer
		qglBufferDataARB( target, amount, NULL, GL_STREAM_DRAW_ARB );
		ptr = qglMapBufferARB( target, GL_WRITE_ONLY_ARB );
		buf->size   = amount;
		buf->mapped = qtrue;
	}

	return ptr;
}

/*
==============
RB_AllocateSurface

Allocate Buffers (in RAM or VBO) to store the vertex data.
==============
*/
void RB_AllocateSurface( void ) {
	int		numIndexes;
	int		size;
	
	numIndexes = tess.numIndexes[0]
		+ tess.numIndexes[1]
		+ tess.numIndexes[2];
	if ( tess.shader == tr.shadowShader ) {
		// need more room for stencil shadows
		tess.numVertexes *= 2;
		numIndexes *= 6;
	}

	if( tess.numVertexes == 0 && numIndexes == 0 ) {
		return;
	}

	// round to next multiple of 4 vertexes for alignment
	tess.numVertexes = (tess.numVertexes + 3) & -4;
	
	if( tess.shader == tr.buildVBOShader ) {
		qglBufferDataARB( GL_ARRAY_BUFFER_ARB,
				  tess.numVertexes * vertexSize, NULL,
				  GL_STATIC_DRAW_ARB );
		tess.vertexPtr = (glVertex_t *)qglMapBufferARB( GL_ARRAY_BUFFER_ARB,
								GL_WRITE_ONLY_ARB );
		tess.indexPtr = NULL;
	} else if( tess.shader == tr.buildIBOShader ) {
		qglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB,
				  numIndexes * sizeof(glIndex_t), NULL,
				  GL_STATIC_DRAW_ARB );
		tess.indexPtr = (glIndex_t *)qglMapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB,
							      GL_WRITE_ONLY_ARB );
		size = tess.numVertexes * vertexSize;

		tess.vertexBuffer = RB_AllocScratch( size );
		tess.vertexPtr = tess.vertexBuffer;
	} else if( tess.VtxBuf.id ) {
		size_t size = tess.numVertexes * vertexSize;

		if( tess.shader->anyGLAttr & GLA_FULL_dynamic ) {
			tess.vertexBuffer = RB_AllocScratch( size + numIndexes * sizeof(glIndex_t) );
			tess.vertexPtr = tess.vertexBuffer;
			tess.indexPtr = (glIndex_t *)(tess.vertexPtr + tess.numVertexes);
		} else {
			if( tess.shader->anyGLAttr & (GLA_VERTEX_dynamic |
						      GLA_NORMAL_dynamic |
						      GLA_TC2_dynamic    |
						      GLA_TC1_dynamic    |
						      GLA_COLOR_dynamic ) ) {
				tess.vertexBuffer = RB_AllocScratch( size );
				tess.vertexPtr = tess.vertexBuffer;
			} else {
				GL_VBO( tess.VtxBuf.id );
				tess.vertexPtr = RB_AllocateStreamBuffer( GL_ARRAY_BUFFER_ARB,
									  &tess.VtxBuf, size );
			}

			GL_IBO( tess.IdxBuf.id );
			tess.indexPtr = RB_AllocateStreamBuffer( GL_ELEMENT_ARRAY_BUFFER,
								 &tess.IdxBuf, numIndexes * sizeof(glIndex_t) );
		}
	} else {
		size = tess.numVertexes * vertexSize
			+ numIndexes * sizeof(glIndex_t);

		tess.vertexBuffer = RB_AllocScratch( size );
		tess.vertexPtr = tess.vertexBuffer;
		tess.indexPtr = (glIndex_t *)(tess.vertexPtr + tess.numVertexes);
	}

	tess.numVertexes = tess.indexRange = 0;
	tess.numIndexes[0] = tess.numIndexes[1] = tess.numIndexes[2] = 0;
	tess.minIndex[0] = tess.minIndex[1] = tess.minIndex[2] = 0x7fffffff;
	tess.maxIndex[0] = tess.maxIndex[1] = tess.maxIndex[2] = 0;

}

/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
static void DrawMultitextured( glRenderState_t *state,
			       shaderCommands_t *input,
			       int stage ) {
	shaderStage_t	*pStage;
	int		bundle;

	pStage = tess.xstages[stage];

	state->program = NULL;
	state->stateBits = pStage->stateBits;

	// this is an ugly hack to work around a GeForce driver
	// bug with multitexture and clip planes
	if ( backEnd.viewParms.portalLevel ) {
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	//
	// base
	//
	state->numImages = 1;
	R_GetAnimatedImage( &pStage->bundle[0], qfalse, &state->image[0] );

	//
	// lightmap/secondary passes
	//
	for ( bundle = 1; bundle < glConfig.numTextureUnits; bundle++ ) {
		if ( !pStage->bundle[bundle].multitextureEnv )
			break;
		
		if ( r_lightmap->integer ) {
			GL_TexEnv( bundle, GL_REPLACE );
		} else {
			GL_TexEnv( bundle, pStage->bundle[bundle].multitextureEnv );
		}

		R_GetAnimatedImage( &pStage->bundle[bundle], qfalse, &state->image[bundle] );
	}

	state->numImages = bundle;
	if( tess.IdxBuf.mapped ) {
		GL_DrawElements( state, input->numIndexes[1], tess.IdxBuf.id,
				 BufferOffset(tess.IdxBuf.current),
				 input->minIndex[1], input->maxIndex[1] );
	} else {
		GL_DrawElements( state, input->numIndexes[1], 0, input->indexPtr,
				 input->minIndex[1], input->maxIndex[1] );
	}
}



/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
#if idppc_altivec
static void ProjectDlightTexture_altivec( glRenderState_t *state ) {
	int		i, l;
	vec_t	origin0, origin1, origin2;
	float   texCoords0, texCoords1;
	vector float floatColorVec0, floatColorVec1;
	vector float modulateVec, colorVec, zero;
	vector short colorShort;
	vector signed int colorInt;
	vector unsigned char floatColorVecPerm, modulatePerm, colorChar;
	vector unsigned char vSel = VECCONST_UINT8(0x00, 0x00, 0x00, 0xff,
                                               0x00, 0x00, 0x00, 0xff,
                                               0x00, 0x00, 0x00, 0xff,
                                               0x00, 0x00, 0x00, 0xff);
	vec2_t	*texCoords;
	color4ub_t	*colors;
	byte	*clipBits;
	vec2_t	*texCoordsArray;
	color4ub_t	*colorArray;
	glIndex_t	*hitIndexes;
	int		numIndexes;
	float	scale;
	float	radius;
	vec3_t	floatColor;
	float	modulate = 0.0f;

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

	clipBits = RB_AllocScratch( sizeof( byte ) * tess.numVertexes );
	texCoordsArray = RB_AllocScratch( sizeof( vec2_t ) * tess.numVertexes );
	colorArray = RB_AllocScratch( sizeof( color4ub_t ) * tess.numVertexes );
	hitIndexes = RB_AllocScratch( sizeof( glIndex_t ) * tess.numIndexes[2] );

	// There has to be a better way to do this so that floatColor
	// and/or modulate are already 16-byte aligned.
	floatColorVecPerm = vec_lvsl(0,(float *)floatColor);
	modulatePerm = vec_lvsl(0,(float *)&modulate);
	modulatePerm = (vector unsigned char)vec_splat((vector unsigned int)modulatePerm,0);
	zero = (vector float)vec_splat_s8(0);

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray;
		colors = colorArray;

		dl = &backEnd.refdef.dlights[l];
		origin0 = dl->transformed[0];
		origin1 = dl->transformed[1];
		origin2 = dl->transformed[2];
		radius = dl->radius;
		scale = 1.0f / radius;

		if(r_greyscale->integer)
		{
			float luminance;
			
			luminance = LUMA(dl->color[0], dl->color[1], dl->color[2]) * 255.0f;
			floatColor[0] = floatColor[1] = floatColor[2] = luminance;
		}
		else if(r_greyscale->value)
		{
			float luminance;
			
			luminance = LUMA(dl->color[0], dl->color[1], dl->color[2]) * 255.0f;
			floatColor[0] = LERP(dl->color[0] * 255.0f, luminance, r_greyscale->value);
			floatColor[1] = LERP(dl->color[1] * 255.0f, luminance, r_greyscale->value);
			floatColor[2] = LERP(dl->color[2] * 255.0f, luminance, r_greyscale->value);
		}
		else
		{
			floatColor[0] = dl->color[0] * 255.0f;
			floatColor[1] = dl->color[1] * 255.0f;
			floatColor[2] = dl->color[2] * 255.0f;
		}
		floatColorVec0 = vec_ld(0, floatColor);
		floatColorVec1 = vec_ld(11, floatColor);
		floatColorVec0 = vec_perm(floatColorVec0,floatColorVec0,floatColorVecPerm);
		for ( i = tess.minIndex[2] ; i < tess.numVertexes ;
		      i++, texCoords++, colors++ ) {
			int		clip = 0;
			vec_t dist0, dist1, dist2;
			
			dist0 = origin0 - tess.vertexPtr2[i].xyz[0];
			dist1 = origin1 - tess.vertexPtr2[i].xyz[1];
			dist2 = origin2 - tess.vertexPtr2[i].xyz[2];

			backEnd.pc.c_dlightVertexes++;

			texCoords0 = 0.5f + dist0 * scale;
			texCoords1 = 0.5f + dist1 * scale;

			if( !r_dlightBacks->integer &&
			    // dist . tess.normal[i]
			    ( dist0 * tess.vertexPtr3[i].normal[0] +
			      dist1 * tess.vertexPtr3[i].normal[1] +
			      dist2 * tess.vertexPtr3[i].normal[2] ) < 0.0f ) {
				clip = 63;
			} else {
				if ( texCoords0 < 0.0f ) {
					clip |= 1;
				} else if ( texCoords0 > 1.0f ) {
					clip |= 2;
				}
				if ( texCoords1 < 0.0f ) {
					clip |= 4;
				} else if ( texCoords1 > 1.0f ) {
					clip |= 8;
				}
				(*texCoords)[0] = texCoords0;
				(*texCoords)[1] = texCoords1;

				// modulate the strength based on the height and color
				if ( dist2 > radius ) {
					clip |= 16;
					modulate = 0.0f;
				} else if ( dist2 < -radius ) {
					clip |= 32;
					modulate = 0.0f;
				} else {
					dist2 = Q_fabs(dist2);
					if ( dist2 < radius * 0.5f ) {
						modulate = 1.0f;
					} else {
						modulate = 2.0f * (radius - dist2) * scale;
					}
				}
			}
			clipBits[i] = clip;

			modulateVec = vec_ld(0,(float *)&modulate);
			modulateVec = vec_perm(modulateVec,modulateVec,modulatePerm);
			colorVec = vec_madd(floatColorVec0,modulateVec,zero);
			colorInt = vec_cts(colorVec,0);	// RGBx
			colorShort = vec_pack(colorInt,colorInt);		// RGBxRGBx
			colorChar = vec_packsu(colorShort,colorShort);	// RGBxRGBxRGBxRGBx
			colorChar = vec_sel(colorChar,vSel,vSel);		// RGBARGBARGBARGBA replace alpha with 255
			vec_ste((vector unsigned int)colorChar,0,(unsigned int *)colors);	// store color
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = tess.numIndexes[0];
		      i < tess.numIndexes[0] + tess.numIndexes[2] ; i += 3 ) {
			glIndex_t	a, b, c;

			a = tess.indexPtr[i];
			b = tess.indexPtr[i+1];
			c = tess.indexPtr[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}

		state->program = NULL;
		GL_VBO( 0, 0 );
		
		SetAttrPointer( state, AL_TEXCOORD, 0,
				2, GL_FLOAT, sizeof(vec2_t),
				texCoordsArray );
		SetAttrPointer( state, AL_COLOR, 0,
				4, GL_UNSIGNED_BYTE, sizeof(color4ub_t),
				colorArray );

		state->numImages = 1;
		state->image[0] = tr.dlightImage;
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		if ( dl->additive ) {
			state->stateBits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE |
				GLS_DEPTHFUNC_EQUAL;
		} else {
			state->stateBits = GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE |
				GLS_DEPTHFUNC_EQUAL;
		}
		GL_DrawElements( state, numIndexes, 0, hitIndexes,
				 0, tess.numVertexes-1 );

		if( r_showtris->integer ) {
			DrawTris( state, numIndexes, 
				  0, hitIndexes,
				  0, tess.numVertexes - 1,
				  1.0f, 1.0f, 1.0f );
		}

		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}

	RB_FreeScratch( hitIndexes );
	RB_FreeScratch( colorArray );
	RB_FreeScratch( texCoordsArray );
	RB_FreeScratch( clipBits );
}
#endif


static void ProjectDlightTexture_scalar( glRenderState_t *state ) {
	int		i, l;
	vec3_t	origin;
	vec2_t	*texCoords;
	color4ub_t	*colors;
	byte	*clipBits;
	vec2_t	*texCoordsArray;
	color4ub_t	*colorArray;
	glIndex_t	*hitIndexes;
	int		numIndexes;
	float	scale;
	float	radius;
	vec3_t	floatColor;
	float	modulate = 0.0f;

	if ( !backEnd.refdef.num_dlights ) {
		return;
	}

	clipBits = RB_AllocScratch( sizeof( byte ) * tess.numVertexes );
	texCoordsArray = RB_AllocScratch( sizeof( vec2_t ) * tess.numVertexes );
	colorArray = RB_AllocScratch( sizeof( color4ub_t ) * tess.numVertexes );
	hitIndexes = RB_AllocScratch( sizeof( glIndex_t ) * tess.numIndexes[2] );

	for ( l = 0 ; l < backEnd.refdef.num_dlights ; l++ ) {
		dlight_t	*dl;

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definately doesn't have any of this light
		}
		texCoords = texCoordsArray;
		colors = colorArray;

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		scale = 1.0f / radius;

		if(r_greyscale->integer)
		{
			float luminance;

			luminance = LUMA(dl->color[0], dl->color[1], dl->color[2]) * 255.0f;
			floatColor[0] = floatColor[1] = floatColor[2] = luminance;
		}
		else if(r_greyscale->value)
		{
			float luminance;
			
			luminance = LUMA(dl->color[0], dl->color[1], dl->color[2]) * 255.0f;
			floatColor[0] = LERP(dl->color[0] * 255.0f, luminance, r_greyscale->value);
			floatColor[1] = LERP(dl->color[1] * 255.0f, luminance, r_greyscale->value);
			floatColor[2] = LERP(dl->color[2] * 255.0f, luminance, r_greyscale->value);
		}
		else
		{
			floatColor[0] = dl->color[0] * 255.0f;
			floatColor[1] = dl->color[1] * 255.0f;
			floatColor[2] = dl->color[2] * 255.0f;
		}

		for ( i = tess.minIndex[2] ; i < tess.numVertexes ;
		      i++, texCoords++, colors++ ) {
			int	clip = 0;
			vec3_t	dist;
			
			VectorSubtract( origin, tess.vertexPtr[i].xyz, dist );

			backEnd.pc.c_dlightVertexes++;

			(*texCoords)[0] = 0.5f + dist[0] * scale;
			(*texCoords)[1] = 0.5f + dist[1] * scale;

			if( !r_dlightBacks->integer &&
			    // dist . tess.normal[i]
			    ( dist[0] * tess.vertexPtr[i].normal[0] +
			      dist[1] * tess.vertexPtr[i].normal[1] +
			      dist[2] * tess.vertexPtr[i].normal[2] ) < 0.0f ) {
				clip = 63;
			} else {
				if ( (*texCoords)[0] < 0.0f ) {
					clip |= 1;
				} else if ( (*texCoords)[0] > 1.0f ) {
					clip |= 2;
				}
				if ( (*texCoords)[1] < 0.0f ) {
					clip |= 4;
				} else if ( (*texCoords)[1] > 1.0f ) {
					clip |= 8;
				}

				// modulate the strength based on the height and color
				if ( dist[2] > radius ) {
					clip |= 16;
					modulate = 0.0f;
				} else if ( dist[2] < -radius ) {
					clip |= 32;
					modulate = 0.0f;
				} else {
					dist[2] = Q_fabs(dist[2]);
					if ( dist[2] < radius * 0.5f ) {
						modulate = 1.0f;
					} else {
						modulate = 2.0f * (radius - dist[2]) * scale;
					}
				}
			}
			clipBits[i] = clip;
			(*colors)[0] = ri.ftol(floatColor[0] * modulate);
			(*colors)[1] = ri.ftol(floatColor[1] * modulate);
			(*colors)[2] = ri.ftol(floatColor[2] * modulate);
			(*colors)[3] = 255;
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = tess.numIndexes[0] ;
		      i < tess.numIndexes[0] + tess.numIndexes[2] ; i += 3 ) {
			glIndex_t	a, b, c;

			a = tess.indexPtr[i];
			b = tess.indexPtr[i+1];
			c = tess.indexPtr[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( !numIndexes ) {
			continue;
		}
		
		state->program = tr.defaultShader->GLSLprogram;
		
		SetAttrPointer( state, AL_TEXCOORD, 0,
				2, GL_FLOAT, sizeof(vec2_t),
				texCoordsArray );
		SetAttrPointer( state, AL_COLOR, 0,
				4, GL_UNSIGNED_BYTE, sizeof(color4ub_t),
				colorArray );

		state->numImages = 1;
		state->image[0] = tr.dlightImage;
		state->stateBits = 0;
		if ( tess.xstages[0] )
			state->stateBits |= tess.xstages[0]->stateBits & GLS_POLYGON_OFFSET;
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered
		if ( dl->additive ) {
			state->stateBits |= GLS_DEPTHFUNC_EQUAL |
				GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		} else {
			state->stateBits |= GLS_DEPTHFUNC_EQUAL |
				GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE;
		}

		GL_DrawElements( state, numIndexes, 0, hitIndexes,
				 0, tess.numVertexes-1 );
		if( r_showtris->integer ) {
			DrawTris( state, numIndexes, 
				  0, hitIndexes,
				  0, tess.numVertexes - 1,
				  1.0f, 1.0f, 1.0f );
		}
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}

	RB_FreeScratch( hitIndexes );
	RB_FreeScratch( colorArray );
	RB_FreeScratch( texCoordsArray );
	RB_FreeScratch( clipBits );
}

static void ProjectDlightTexture( glRenderState_t *state ) {
#if idppc_altivec
	if (com_altivec->integer) {
		// must be in a seperate function or G3 systems will crash.
		ProjectDlightTexture_altivec( state );
		return;
	}
#endif
	ProjectDlightTexture_scalar( state );
}


/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( glRenderState_t *state ) {
	fog_t		*fog;

	tess.svars.texcoords[0] = RB_AllocScratch( tess.numVertexes * sizeof(vec2_t) );

	state->stateBits = GLS_DEFAULT;
	if ( tess.xstages[0] )
		state->stateBits |= tess.xstages[0]->stateBits & GLS_POLYGON_OFFSET;
	if ( tess.shader->fogPass == FP_EQUAL )
		state->stateBits |= GLS_DEPTHFUNC_EQUAL;

	if ( tr.fogShader->GLSLprogram ) {
		state->program = tr.fogShader->GLSLprogram;
		state->stateBits |= GLS_SRCBLEND_ONE | GLS_DSTBLEND_SRC_ALPHA;
	} else {
		state->program = NULL;
		state->stateBits |= GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}

	fog = tr.world->fogs + tess.fogNum;

	RB_CalcFogTexCoords( tess.svars.texcoords[0], tess.minIndex[2],
			     tess.numVertexes );

	SetAttrPointer( state, AL_TEXCOORD, 0,
			2, GL_FLOAT, sizeof(vec2_t),
			tess.svars.texcoords[0][0] );
	SetAttrVec4f( state, AL_COLOR,
		      fog->parms.color[0],
		      fog->parms.color[1],
		      fog->parms.color[2],
		      1.0f );

	state->numImages = 1;
	state->image[0] = tr.fogImage;

	GL_DrawElements( state, tess.numIndexes[2],
			 0, tess.indexPtr + tess.numIndexes[0],
			 0, tess.numVertexes-1 );

	RB_FreeScratch( tess.svars.texcoords[0] );
}

/*
===============
ComputeColors
===============
*/
static void ComputeColors( glRenderState_t *state,
			   vboInfo_t *VBO,
			   shaderStage_t *pStage,
			   qboolean skipFog )
{
	int		i;
	alphaGen_t	aGen = pStage->alphaGen;
	colorGen_t	rgbGen = pStage->rgbGen;
	color4ub_t	color;
	fog_t		*fog;
	qboolean	constRGB, constA;
	
	// get rid of AGEN_SKIP
	if ( aGen == AGEN_SKIP ) {
		if ( rgbGen == CGEN_EXACT_VERTEX ||
		     rgbGen == CGEN_VERTEX ) {
			aGen = AGEN_VERTEX;
		} else {
			aGen = AGEN_IDENTITY;
		}
	}
	
	// no need to multiply by 1
	if ( tr.identityLight == 1 && rgbGen == CGEN_VERTEX ) {
		rgbGen = CGEN_EXACT_VERTEX;
	}

	// check for constant RGB
	switch( rgbGen ) {
	case CGEN_IDENTITY_LIGHTING:
		color[0] = color[1] = color[2] = tr.identityLightByte;
		constRGB = qtrue;
		break;
	case CGEN_IDENTITY:
		color[0] = color[1] = color[2] = 255;
		constRGB = qtrue;
		break;
	case CGEN_ENTITY:
		RB_CalcColorFromEntity( &color, 1 );
		constRGB = qtrue;
		break;
	case CGEN_ONE_MINUS_ENTITY:
		RB_CalcColorFromOneMinusEntity( &color, 1 );
		constRGB = qtrue;
		break;
	case CGEN_WAVEFORM:
		RB_CalcWaveColor( &pStage->rgbWave, &color, 1 );
		constRGB = qtrue;
		break;
	case CGEN_FOG:
		fog = tr.world->fogs + tess.fogNum;
		*(int *)(&color) = fog->colorInt;
		constRGB = qtrue;
		break;
	case CGEN_CONST:
		*(int *)(&color) = *(int *)pStage->constantColor;		
		constRGB = qtrue;
		break;
	default:
		constRGB = qfalse;
		break;
	}

	// check for constant ALPHA
	switch( aGen ) {
	case AGEN_IDENTITY:
		color[3] = 255;
		constA = qtrue;
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( &color, 1 );
		constA = qtrue;
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( &color, 1 );
		constA = qtrue;
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->alphaWave, &color, 1 );
		constA = qtrue;
		break;
	case AGEN_CONST:
		color[3] = pStage->constantColor[3];
		constA = qtrue;
		break;
	default:
		constA = qfalse;
		break;
	}

	if ( !r_greyscale->integer &&
	     (skipFog || !tess.fogNum || pStage->adjustColorsForFog == ACFF_NONE) ) {
		// if RGB and ALPHA are constant, just set the GL color
		if ( constRGB && constA ) {
			SetAttrVec4f( state, AL_COLOR,
				      color[0] / 255.0f,
				      color[1] / 255.0f,
				      color[2] / 255.0f,
				      color[3] / 255.0f );
			tess.svars.colors = NULL;
			return;
		}
		
		// if RGB and ALPHA are identical to vertex data, bind that
		if ( aGen == AGEN_VERTEX && rgbGen == CGEN_EXACT_VERTEX ) {
			if( VBO && VBO->vbo ) {
				SetAttrPointer( state, AL_COLOR, VBO->vbo,
						4, GL_UNSIGNED_BYTE,
						sizeof(glVertex_t),
						&VBO->offset->color );
			} else if( tess.VtxBuf.mapped ) {
				SetAttrPointer( state, AL_COLOR, tess.VtxBuf.id,
						4, GL_UNSIGNED_BYTE, sizeof(glVertex_t),
						&((glVertex_t *)BufferOffset(tess.VtxBuf.current))->color );
			} else {
				SetAttrPointer( state, AL_COLOR, 0,
						4, GL_UNSIGNED_BYTE,
						sizeof(glVertex_t),
						&tess.vertexPtr->color );
			}
			tess.svars.colors = NULL;
			return;
		}
	}
	
	// we have to allocate a per-Vertex color value
	tess.svars.colors = RB_AllocScratch( tess.numVertexes * sizeof(color4ub_t) );

	//
	// rgbGen
	//
	switch ( rgbGen )
	{
	case CGEN_BAD:
	case CGEN_IDENTITY_LIGHTING:
	case CGEN_IDENTITY:
	case CGEN_ENTITY:
	case CGEN_ONE_MINUS_ENTITY:
	case CGEN_WAVEFORM:
	case CGEN_FOG:
	case CGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			*(int *)tess.svars.colors[i] = *(int *)(&color);
		}
		break;
	case CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor( tess.svars.colors, tess.numVertexes );
		break;
	case CGEN_EXACT_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i][0] = tess.vertexPtr[i].color[0];
			tess.svars.colors[i][1] = tess.vertexPtr[i].color[1];
			tess.svars.colors[i][2] = tess.vertexPtr[i].color[2];
			tess.svars.colors[i][3] = tess.vertexPtr[i].color[3];
		}
		break;
	case CGEN_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i][0] = tess.vertexPtr[i].color[0] * tr.identityLight;
			tess.svars.colors[i][1] = tess.vertexPtr[i].color[1] * tr.identityLight;
			tess.svars.colors[i][2] = tess.vertexPtr[i].color[2] * tr.identityLight;
			tess.svars.colors[i][3] = tess.vertexPtr[i].color[3];
		}
		break;
	case CGEN_ONE_MINUS_VERTEX:
		if ( tr.identityLight == 1 )
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i][0] = 255 - tess.vertexPtr[i].color[0];
				tess.svars.colors[i][1] = 255 - tess.vertexPtr[i].color[1];
				tess.svars.colors[i][2] = 255 - tess.vertexPtr[i].color[2];
			}
		}
		else
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				tess.svars.colors[i][0] = ( 255 - tess.vertexPtr[i].color[0] ) * tr.identityLight;
				tess.svars.colors[i][1] = ( 255 - tess.vertexPtr[i].color[1] ) * tr.identityLight;
				tess.svars.colors[i][2] = ( 255 - tess.vertexPtr[i].color[2] ) * tr.identityLight;
			}
		}
		break;
	}

	//
	// alphaGen
	//
	switch ( aGen )
	{
	case AGEN_SKIP:
	case AGEN_IDENTITY:
	case AGEN_ENTITY:
	case AGEN_ONE_MINUS_ENTITY:
	case AGEN_WAVEFORM:
	case AGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			tess.svars.colors[i][3] = color[3];
		}
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( tess.svars.colors, tess.numVertexes );
		break;
	case AGEN_VERTEX:
		if ( pStage->rgbGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				tess.svars.colors[i][3] = tess.vertexPtr[i].color[3];
			}
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			tess.svars.colors[i][3] = 255 - tess.vertexPtr[i].color[3];
		}
		break;
	case AGEN_PORTAL:
		{
			unsigned char alpha;

			for ( i = 0; i < tess.numVertexes; i++ )
			{
				float len;
				vec3_t v;

				VectorSubtract( tess.vertexPtr[i].xyz, backEnd.viewParms.or.origin, v );
				len = VectorLength( v );

				len /= tess.shader->portalRange;

				if ( len < 0 )
				{
					alpha = 0;
				}
				else if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				tess.svars.colors[i][3] = alpha;
			}
		}
		break;
	}

	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum )
	{
		switch ( pStage->adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( tess.svars.colors, tess.numVertexes );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( tess.svars.colors, tess.numVertexes );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( tess.svars.colors, tess.numVertexes );
			break;
		case ACFF_NONE:
			break;
		}
	}
	
	// if in greyscale rendering mode turn all color values into greyscale.
	if(r_greyscale->integer)
	{
		int scale;
		for(i = 0; i < tess.numVertexes; i++)
		{
			scale = LUMA(tess.svars.colors[i][0], tess.svars.colors[i][1], tess.svars.colors[i][2]);
 			tess.svars.colors[i][0] = tess.svars.colors[i][1] = tess.svars.colors[i][2] = scale;
		}
	}
	else if(r_greyscale->value)
	{
		float scale;
		
		for(i = 0; i < tess.numVertexes; i++)
		{
			scale = LUMA(tess.svars.colors[i][0], tess.svars.colors[i][1], tess.svars.colors[i][2]);
			tess.svars.colors[i][0] = LERP(tess.svars.colors[i][0], scale, r_greyscale->value);
			tess.svars.colors[i][1] = LERP(tess.svars.colors[i][1], scale, r_greyscale->value);
			tess.svars.colors[i][2] = LERP(tess.svars.colors[i][2], scale, r_greyscale->value);
		}
	}

	SetAttrPointer( state, AL_COLOR, 0,
			4, GL_UNSIGNED_BYTE, sizeof(color4ub_t),
			tess.svars.colors );
}

/*
===============
ComputeTexCoords
===============
*/
static void ComputeTexCoords( glRenderState_t *state,
			      vboInfo_t *VBO,
			      shaderStage_t *pStage ) {
	int		i;
	int		b;
	qboolean	noTexMods;
	
	for ( b = 0; b < NUM_TEXTURE_BUNDLES; b++ ) {
		int tm;

		noTexMods = (pStage->bundle[b].numTexMods == 0) ||
			(pStage->bundle[b].texMods[0].type == TMOD_NONE);
		
		if ( noTexMods && pStage->bundle[b].tcGen == TCGEN_BAD ) {
			SetAttrUnspec( state, AL_TEXCOORD + b );
			continue;
		}

		if ( noTexMods && pStage->bundle[b].tcGen == TCGEN_TEXTURE ) {
			if( VBO && VBO->vbo ) {
				SetAttrPointer( state, AL_TEXCOORD + b, VBO->vbo,
						2, GL_FLOAT, sizeof(glVertex_t),
						&VBO->offset->tc1);
			} else if( tess.VtxBuf.mapped ) {
				SetAttrPointer( state, AL_TEXCOORD + b, tess.VtxBuf.id,
						2, GL_FLOAT, sizeof(glVertex_t),
						&((glVertex_t *)BufferOffset(tess.VtxBuf.current))->tc1 );
			} else if( tess.vertexPtr == NULL ) {
				SetAttrPointer( state, AL_TEXCOORD + b, backEnd.worldVBO->vbo,
						2, GL_FLOAT, sizeof(glVertex_t),
						&backEnd.worldVBO->offset->tc1);
			} else {
				SetAttrPointer( state, AL_TEXCOORD + b, 0,
						2, GL_FLOAT, sizeof(glVertex_t),
						&tess.vertexPtr->tc1 );
			}
			tess.svars.texcoords[b] = NULL;
			continue;
		}

		if ( noTexMods && pStage->bundle[b].tcGen == TCGEN_LIGHTMAP ) {
			if( VBO && VBO->vbo ) {
				SetAttrPointer( state, AL_TEXCOORD + b, VBO->vbo,
						2, GL_FLOAT, sizeof(glVertex_t),
						&VBO->offset->tc2);
			} else if( tess.VtxBuf.mapped ) {
				SetAttrPointer( state, AL_TEXCOORD + b, tess.VtxBuf.id,
						2, GL_FLOAT, sizeof(glVertex_t),
						&((glVertex_t *)BufferOffset(tess.VtxBuf.current))->tc2 );
			} else if( tess.vertexPtr == NULL ) {
				SetAttrPointer( state, AL_TEXCOORD + b, backEnd.worldVBO->vbo,
						2, GL_FLOAT, sizeof(glVertex_t),
						&backEnd.worldVBO->offset->tc2);
			} else {
				SetAttrPointer( state, AL_TEXCOORD + b, 0,
						2, GL_FLOAT, sizeof(glVertex_t),
						&tess.vertexPtr->tc2 );
			}
			tess.svars.texcoords[b] = NULL;
			continue;
		}

		tess.svars.texcoords[b] = RB_AllocScratch( tess.numVertexes * sizeof(vec2_t) );
		//
		// generate the texture coordinates
		//
		switch ( pStage->bundle[b].tcGen )
		{
		case TCGEN_IDENTITY:
			Com_Memset( tess.svars.texcoords[b], 0, sizeof( vec2_t ) * tess.numVertexes );
			break;
		case TCGEN_TEXTURE:
			if( VBO ) ri.Error( ERR_DROP, "tcgen in VBO\n" );
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.vertexPtr[i].tc1[0];
				tess.svars.texcoords[b][i][1] = tess.vertexPtr[i].tc1[1];
			}
			break;
		case TCGEN_LIGHTMAP:
			if( VBO ) ri.Error( ERR_DROP, "tcgen in VBO\n" );
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = tess.vertexPtr[i].tc2[0];
				tess.svars.texcoords[b][i][1] = tess.vertexPtr[i].tc2[1];
			}
			break;
		case TCGEN_VECTOR:
			if( VBO ) ri.Error( ERR_DROP, "tcgen in VBO\n" );
			for ( i = 0 ; i < tess.numVertexes ; i++ ) {
				tess.svars.texcoords[b][i][0] = DotProduct( tess.vertexPtr[i].xyz, pStage->bundle[b].tcGenVectors[0] );
				tess.svars.texcoords[b][i][1] = DotProduct( tess.vertexPtr[i].xyz, pStage->bundle[b].tcGenVectors[1] );
			}
			break;
		case TCGEN_FOG:
			if( VBO ) ri.Error( ERR_DROP, "tcgen in VBO\n" );
			RB_CalcFogTexCoords( tess.svars.texcoords[b],
					     0, tess.numVertexes );
			break;
		case TCGEN_ENVIRONMENT_MAPPED:
			if( VBO ) ri.Error( ERR_DROP, "tcgen in VBO\n" );
			RB_CalcEnvironmentTexCoords( tess.svars.texcoords[b], tess.numVertexes );
			break;
		case TCGEN_BAD:
			return;
		}

		//
		// alter texture coordinates
		//
		for ( tm = 0; tm < pStage->bundle[b].numTexMods ; tm++ ) {
			switch ( pStage->bundle[b].texMods[tm].type )
			{
			case TMOD_NONE:
				tm = TR_MAX_TEXMODS;		// break out of for loop
				break;

			case TMOD_TURBULENT:
				RB_CalcTurbulentTexCoords( &pStage->bundle[b].texMods[tm].wave, 
							   tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_ENTITY_TRANSLATE:
				RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord,
							tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_SCROLL:
				RB_CalcScrollTexCoords( pStage->bundle[b].texMods[tm].scroll,
							tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_SCALE:
				RB_CalcScaleTexCoords( pStage->bundle[b].texMods[tm].scale,
						       tess.svars.texcoords[b], tess.numVertexes );
				break;
			
			case TMOD_STRETCH:
				RB_CalcStretchTexCoords( &pStage->bundle[b].texMods[tm].wave, 
							 tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_TRANSFORM:
				RB_CalcTransformTexCoords( &pStage->bundle[b].texMods[tm],
							   tess.svars.texcoords[b], tess.numVertexes );
				break;

			case TMOD_ROTATE:
				RB_CalcRotateTexCoords( pStage->bundle[b].texMods[tm].rotateSpeed,
							tess.svars.texcoords[b], tess.numVertexes );
				break;

			default:
				ri.Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'", pStage->bundle[b].texMods[tm].type, tess.shader->name );
				break;
			}
		}
		SetAttrPointer( state, AL_TEXCOORD + b, 0,
				2, GL_FLOAT, sizeof(vec2_t),
				tess.svars.texcoords[b] );
	}
}

/*
** RB_IterateStagesGeneric
*/
static void RB_IterateStagesGenericVBO( glRenderState_t *state,
					iboInfo_t *ibo,
					qboolean skipDetail )
{
	int stage;
	int bundle;

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t	*pStage = tess.xstages[stage];
		vboInfo_t	*vbo = ibo->vbo;

		if ( !pStage )
		{
			break;
		}

		// if the surface was invisible in the last frame, skip
		// all detail stages
		if ( skipDetail && pStage->isDetail )
			continue;

		state->stateBits = pStage->stateBits;

		ComputeColors( state, vbo, pStage, qtrue );
		ComputeTexCoords( state, vbo, pStage );

		//
		// set state
		//
		state->numImages = 1;
		if ( pStage->bundle[0].vertexLightmap && ( (r_vertexLight->integer && !r_uiFullScreen->integer) || glConfig.hardwareType == GLHW_PERMEDIA2 ) && r_lightmap->integer )
		{
			state->image[0] = tr.whiteImage;
		}
		else 
			R_GetAnimatedImage( &pStage->bundle[0], qfalse, &state->image[0] );
		
		//
		// do multitexture
		//
		for ( bundle = 1; bundle < glConfig.numTextureUnits; bundle++ ) {
			if ( !pStage->bundle[bundle].multitextureEnv )
				break;

			if ( r_lightmap->integer ) {
				GL_TexEnv( bundle, GL_REPLACE );
			} else {
				GL_TexEnv( bundle, pStage->bundle[bundle].multitextureEnv );
			}
			R_GetAnimatedImage( &pStage->bundle[bundle], qfalse, &state->image[bundle] );
		}
		state->numImages = bundle;

		GL_DrawElements( state, ibo->numIndexes, ibo->ibo, ibo->offset,
				 ibo->minIndex, ibo->maxIndex );
		
		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}
	}
}
static void RB_IterateStagesGeneric( glRenderState_t *state,
				     shaderCommands_t *input,
				     qboolean skipDetail )
{
	int stage, b;

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		shaderStage_t *pStage = tess.xstages[stage];

		if ( !pStage )
		{
			break;
		}

		if ( skipDetail && pStage->isDetail )
			continue;

		if( !tess.vertexPtr ) {
			ComputeColors( state, backEnd.worldVBO, pStage, qtrue );
			ComputeTexCoords( state, backEnd.worldVBO, pStage );
		} else {
			ComputeColors( state, NULL, pStage, qfalse );
			ComputeTexCoords( state, NULL, pStage );
		}

		//
		// do multitexture
		//
		if ( pStage->bundle[1].image[0] != 0 )
		{
			DrawMultitextured( state, input, stage );
		}
		else
		{
			//
			// set state
			//
			state->stateBits = pStage->stateBits;
			state->numImages = 1;
			if ( pStage->bundle[0].vertexLightmap && ( (r_vertexLight->integer && !r_uiFullScreen->integer) || glConfig.hardwareType == GLHW_PERMEDIA2 ) && r_lightmap->integer )
			{
				state->image[0] = tr.whiteImage;
			}
			else if( tess.imgOverride )
				state->image[0] = tess.imgOverride;
			else
				R_GetAnimatedImage( &pStage->bundle[0], qfalse,
						    &state->image[0] );

			//
			// draw
			//
			if( tess.IdxBuf.mapped ) {
				GL_DrawElements( state, input->numIndexes[1],
						 tess.IdxBuf.id,
						 BufferOffset(tess.IdxBuf.current),
						 input->minIndex[1],
						 input->maxIndex[1] );
			} else {
				GL_DrawElements( state, input->numIndexes[1],
						 0, input->indexPtr,
						 input->minIndex[1],
						 input->maxIndex[1] );
			}
		}

		for ( b = NUM_TEXTURE_BUNDLES - 1; b >= 0; b-- ) {
			if ( input->svars.texcoords[b] != NULL ) {
				RB_FreeScratch( input->svars.texcoords[b] );
				input->svars.texcoords[b] = NULL;
			}
		}
		if ( input->svars.colors != NULL ) {
			RB_FreeScratch( input->svars.colors );
			input->svars.colors = NULL;
		}

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].isLightmap || pStage->bundle[1].isLightmap || pStage->bundle[0].vertexLightmap ) )
		{
			break;
		}
	}
}


/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;
	shader_t		*shader;
	iboInfo_t        *ibo;
	glRenderState_t   state;
	GLuint            glQueryID = 0, *glQueryResult = NULL;
	qboolean          skipDetails = qfalse;

	input = &tess;
	shader = input->shader;

	RB_DeformTessGeometry();

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", shader->name) );
	}

	InitState( &state );

	//
	// set face culling appropriately
	//
	state.faceCulling = shader->cullType;

	if( qglBeginQueryARB &&
	    !backEnd.viewParms.portalLevel ) {
		if ( backEnd.currentEntity == &tr.worldEntity ) {
			glQueryID = shader->QueryID;
			glQueryResult = &shader->QueryResult;
		}
	}
	
	if( glQueryID ) {
		GL_GetQuery( glQueryID, glQueryResult );
		GL_StartQuery( glQueryID, glQueryResult );
		skipDetails = (QUERY_RESULT(glQueryResult) == 0);
	}

	for( ibo = tess.firstIBO; ibo; ibo = ibo->next ) {
		vboInfo_t	*vbo = ibo->vbo;
		
		SetAttrPointer( &state, AL_VERTEX, vbo->vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				&vbo->offset->xyz );

		RB_IterateStagesGenericVBO( &state, ibo, skipDetails );

		if ( r_showtris->integer && !shader->isDepth ) {
			DrawTris( &state, ibo->numIndexes, 
				  ibo->ibo, ibo->offset,
				  ibo->minIndex,
				  ibo->maxIndex,
				  1.0f, 1.0f, 0.0f );
		}
		if ( r_shownormals->integer ) {
			DrawNormals( &state );
		}
	}

	if ( tess.numIndexes[1] > 0 ) {
		glVertex_t	*vertexes;
		GLuint		vbo;
		
		if ( input->vertexPtr ) {
			if( tess.VtxBuf.mapped ) {
				vbo = tess.VtxBuf.id;
				vertexes = BufferOffset( tess.VtxBuf.current );
			} else {
				vbo = 0;
				vertexes = input->vertexPtr;
			}
		} else {
			vbo = backEnd.worldVBO->vbo;
			vertexes = backEnd.worldVBO->offset;
		}
		//
		// lock XYZ
		//
		SetAttrPointer( &state, AL_VERTEX, vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				&vertexes->xyz );
		if( qglLockArraysEXT && !vbo ) {
			if( tess.shader->allGLAttr & GLA_COLOR_vtxcolor )
				SetAttrPointer( &state, AL_COLOR, vbo,
						4, GL_UNSIGNED_BYTE,
						sizeof(glVertex_t),
						&vertexes->color );
			if( tess.shader->allGLAttr & GLA_TC1_texcoord )
				SetAttrPointer( &state, AL_TEXCOORD, vbo,
						2, GL_FLOAT, sizeof(glVertex_t),
						&vertexes->tc1 );
			if( tess.shader->allGLAttr & GLA_TC1_lmcoord )
				SetAttrPointer( &state, AL_TEXCOORD, vbo,
						2, GL_FLOAT, sizeof(glVertex_t),
						&vertexes->tc2 );
			if( tess.shader->allGLAttr & GLA_TC2_texcoord )
				SetAttrPointer( &state, AL_TEXCOORD2, vbo,
						2, GL_FLOAT, sizeof(glVertex_t),
						&vertexes->tc1 );
			if( tess.shader->allGLAttr & GLA_TC2_lmcoord )
				SetAttrPointer( &state, AL_TEXCOORD2, vbo,
						2, GL_FLOAT, sizeof(glVertex_t),
						&vertexes->tc2 );
			GL_LockArrays( &state );
		}
		//
		// call shader function
		//
		RB_IterateStagesGeneric( &state, input, skipDetails );
		
		if ( r_showtris->integer && !shader->isDepth ) {
			DrawTris( &state, tess.numIndexes[1],
				  0, tess.indexPtr,
				  tess.minIndex[1], tess.maxIndex[1],
				  1.0f, 0.0f, 0.0f );
		}
		if( qglLockArraysEXT && !vbo ) {
			GL_UnlockArrays( );
		}
		if ( r_shownormals->integer ) {
			DrawNormals( &state );
		}
	}

	if( glQueryID ) {
		GL_EndQuery( glQueryID, glQueryResult );
	}
}


/*
** RB_StageIteratorVertexLitTexture
*/
void RB_StageIteratorVertexLitTexture( void )
{
	shaderCommands_t *input;
	shader_t		*shader;
	iboInfo_t        *ibo;
	glRenderState_t  state;
	GLuint            glQueryID = 0, *glQueryResult = NULL;

	input = &tess;
	shader = input->shader;

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorVertexLitTexturedUnfogged( %s ) ---\n", shader->name) );
	}

	InitState( &state );

	//
	// set face culling appropriately
	//
	state.stateBits = tess.xstages[0]->stateBits;
	state.faceCulling = shader->cullType;

	state.numImages = 1;
	if( tess.imgOverride ) {
		state.image[0] = tess.imgOverride;
	} else {
		R_GetAnimatedImage( &tess.xstages[0]->bundle[0], qfalse,
				    &state.image[0] );
	}

	if( qglBeginQueryARB &&
	    !backEnd.viewParms.portalLevel ) {
		if ( backEnd.currentEntity == &tr.worldEntity ) {
			glQueryID = shader->QueryID;
			glQueryResult = &shader->QueryResult;
		}
	}
	
	if( glQueryID ) {
		GL_StartQuery( glQueryID, glQueryResult );
	}

	//
	// set arrays and lock
	//
	for( ibo = input->firstIBO; ibo; ibo = ibo->next ) {
		vboInfo_t	*vbo = ibo->vbo;
		
		SetAttrPointer( &state, AL_COLOR, vbo->vbo,
				4, GL_UNSIGNED_BYTE, sizeof(glVertex_t),
				&vbo->offset->color );
		SetAttrPointer( &state, AL_TEXCOORD, vbo->vbo,
				2, GL_FLOAT, sizeof(glVertex_t),
				&vbo->offset->tc1 );
		SetAttrPointer( &state, AL_VERTEX, vbo->vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				&vbo->offset->xyz );
		
		//
		// call special shade routine
		//
		GL_DrawElements( &state, ibo->numIndexes, ibo->ibo,
				 ibo->offset, ibo->minIndex, ibo->maxIndex );
		
		if ( r_showtris->integer && !shader->isDepth ) {
			DrawTris( &state, ibo->numIndexes,
				  ibo->ibo, ibo->offset,
				  ibo->minIndex, ibo->maxIndex,
				  1.0f, 1.0f, 0.0f );
		}
		if ( r_shownormals->integer ) {
			DrawNormals( &state );
		}
	}
	if ( input->numIndexes[1] > 0 ) {
		vec3_t		*vertexes;
		vec2_t		*tcs;
		GLuint		vbo;

		//
		// compute colors
		//
		tess.svars.colors = RB_AllocScratch( tess.numVertexes * sizeof(color4ub_t) );
		RB_CalcDiffuseColor( tess.svars.colors, tess.numVertexes );

		if ( input->vertexPtr ) {
			vbo = 0;
			vertexes = &input->vertexPtr[0].xyz;
			tcs = &input->vertexPtr[0].tc1;
		} else {
			vbo = backEnd.worldVBO->vbo;
			vertexes = &backEnd.worldVBO->offset->xyz;
			tcs = &backEnd.worldVBO->offset->tc1;
		}
		
		SetAttrPointer( &state, AL_COLOR, 0,
				4, GL_UNSIGNED_BYTE, sizeof(color4ub_t),
				tess.svars.colors );
		SetAttrPointer( &state, AL_TEXCOORD, vbo,
				2, GL_FLOAT, sizeof(glVertex_t),
				tcs );
		SetAttrPointer( &state, AL_VERTEX, vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				vertexes );
		
		//
		// call special shade routine
		//
		GL_DrawElements( &state, input->numIndexes[1],
				 0, input->indexPtr,
				 input->minIndex[1], input->maxIndex[1] );

		if ( r_showtris->integer && !shader->isDepth ) {
			DrawTris( &state, input->numIndexes[1],
				  0, input->indexPtr,
				  input->minIndex[1], input->maxIndex[1],
				  1.0f, 0.0f, 0.0f );
		}
		if ( r_shownormals->integer ) {
			DrawNormals( &state );
		}
		RB_FreeScratch( tess.svars.colors );
	}

	if( glQueryID ) {
		GL_EndQuery( glQueryID, glQueryResult );
	}
}

void RB_StageIteratorLightmappedMultitexture( void ) {
	shaderCommands_t *input;
	shader_t		*shader;
	iboInfo_t        *ibo;
	glRenderState_t   state;
	GLuint            glQueryID = 0, *glQueryResult = NULL;

	input = &tess;
	shader = input->shader;

	InitState( &state );

	//
	// log this call
	//
	if ( r_logFile->integer ) {
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorLightmappedMultitexture( %s ) ---\n", shader->name) );
	}

	//
	// set face culling appropriately
	//
	state.stateBits = GLS_DEFAULT;
	state.faceCulling = shader->cullType;

	SetAttrVec4f( &state, AL_COLOR, 1.0f, 1.0f, 1.0f, 1.0f );

	state.numImages = 2;
	if( tess.imgOverride ) {
		state.image[0] = tess.imgOverride;
	} else {
		R_GetAnimatedImage( &tess.xstages[0]->bundle[0], qfalse,
				    &state.image[0] );
	}
	R_GetAnimatedImage( &tess.xstages[0]->bundle[1], qfalse,
			    &state.image[1] );

	if ( r_lightmap->integer ) {
		GL_TexEnv( 1, GL_REPLACE );
	} else {
		GL_TexEnv( 1, GL_MODULATE );
	}

	if( qglBeginQueryARB &&
	    !backEnd.viewParms.portalLevel ) {
		if ( backEnd.currentEntity == &tr.worldEntity ) {
			glQueryID = shader->QueryID;
			glQueryResult = &shader->QueryResult;
		}
	}
	
	if( glQueryID ) {
		GL_StartQuery( glQueryID, glQueryResult );
	}

	for( ibo = tess.firstIBO; ibo; ibo = ibo->next ) {
		vboInfo_t	*vbo = ibo->vbo;
		
		//
		// set color, pointers, and lock
		//
		SetAttrPointer( &state, AL_VERTEX, vbo->vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				&vbo->offset->xyz );
		
		//
		// select base stage
		//
		SetAttrPointer( &state, AL_TEXCOORD, vbo->vbo,
				2, GL_FLOAT, sizeof(glVertex_t),
				&vbo->offset->tc1 );
		
		//
		// configure second stage
		//
		SetAttrPointer( &state, AL_TEXCOORD2, vbo->vbo,
				2, GL_FLOAT, sizeof(glVertex_t),
				&vbo->offset->tc2 );
		
		GL_DrawElements( &state, ibo->numIndexes, ibo->ibo,
				 ibo->offset, ibo->minIndex, ibo->maxIndex );
		
		if ( r_showtris->integer && !shader->isDepth ) {
			DrawTris( &state, ibo->numIndexes, ibo->ibo,
				  ibo->offset, ibo->minIndex, ibo->maxIndex,
				  1.0f, 1.0f, 0.0f );
		}
		if ( r_shownormals->integer ) {
			DrawNormals( &state );
		}
	}
	if ( tess.numIndexes[1] > 0 ) {
		vec3_t	*vertexes;
		vec2_t	*tcs1, *tcs2;
		GLuint   vbo;

		if ( tess.vertexPtr ) {
			vbo = 0;
			vertexes = &input->vertexPtr[0].xyz;
			tcs1 = &input->vertexPtr[0].tc1;
			tcs2 = &input->vertexPtr[0].tc2;
		} else {
			vbo = backEnd.worldVBO->vbo;
			vertexes = &backEnd.worldVBO->offset->xyz;
			tcs1 = &backEnd.worldVBO->offset->tc1;
			tcs2 = &backEnd.worldVBO->offset->tc2;
		}

		//
		// set color, pointers, and lock
		//
		SetAttrPointer( &state, AL_VERTEX, vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				vertexes );

		//
		// select base stage
		//
		SetAttrPointer( &state, AL_TEXCOORD, vbo,
				2, GL_FLOAT, sizeof(glVertex_t),
				tcs1 );
		
		//
		// configure second stage
		//
		SetAttrPointer( &state, AL_TEXCOORD2, vbo,
				2, GL_FLOAT, sizeof(glVertex_t),
				tcs2 );
		
		GL_DrawElements( &state, input->numIndexes[1],
				 0, input->indexPtr,
				 input->minIndex[1], input->maxIndex[1] );

		if ( r_showtris->integer && !shader->isDepth ) {
			DrawTris( &state, input->numIndexes[1],
				  0, input->indexPtr,
				  input->minIndex[1], input->maxIndex[1],
				  1.0f, 0.0f, 0.0f );
		}
		if ( r_shownormals->integer ) {
			DrawNormals( &state );
		}
	}

	if( glQueryID ) {
		GL_EndQuery( glQueryID, glQueryResult );
	}
}

/*
** RB_StageIteratorGLSL
*/
void RB_StageIteratorGLSL( void ) {
	shaderCommands_t	*input = &tess;
	shader_t		*shader = input->shader;
	int			i, n;
	iboInfo_t		*ibo;
	glRenderState_t		state;
	GLuint            	glQueryID = 0, *glQueryResult = NULL;
	qboolean		setAttrs;

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGLSL( %s ) ---\n", shader->name) );
	}

	InitState( &state );
	//
	// set face culling appropriately
	//
	state.stateBits = shader->stages[0]->stateBits;
	state.faceCulling = shader->cullType;
	state.program = shader->GLSLprogram;
	state.numImages = shader->numUnfoggedPasses;

	// bind all required textures
	if( tess.imgOverride ) {
		state.image[0] = tess.imgOverride;
		i = 1;
	} else {
		i = 0;
	}
	for( ; i < state.numImages; i++ ) {
		if( !shader->stages[i] )
			break;
		
		R_GetAnimatedImage( &shader->stages[i]->bundle[0], qtrue,
				    &state.image[i] );
	}
	if( tess.dataTexture ) {
		// bind data texture
		state.image[state.numImages++] = tess.dataTexture;
	}
	
	// bind attributes
	SetAttrVec4f( &state, AL_CAMERAPOS,
		      backEnd.viewParms.or.origin[0],
		      backEnd.viewParms.or.origin[1],
		      backEnd.viewParms.or.origin[2],
		      0.0f );

	if( tess.numInstances > 1 ) {
		if( qglDrawElementsInstancedARB ) {
			// bind instanced arrays
			n = 1; setAttrs = qfalse;
			SetAttrPointerInst( &state, AL_TIMES, 0, 4, GL_FLOAT,
					    sizeof( instanceVars_t ),
					    &tess.instances[0].times , 1 );
			SetAttrPointerInst( &state, AL_TRANSX, 0, 4, GL_FLOAT,
					    sizeof( instanceVars_t ),
					    &tess.instances[0].transX, 1 );
			SetAttrPointerInst( &state, AL_TRANSY, 0, 4, GL_FLOAT,
					    sizeof( instanceVars_t ),
					    &tess.instances[0].transY, 1 );
			SetAttrPointerInst( &state, AL_TRANSZ, 0, 4, GL_FLOAT,
					    sizeof( instanceVars_t ),
					    &tess.instances[0].transZ, 1 );
			SetAttrPointerInst( &state, AL_AMBIENTLIGHT,
					    0, 4, GL_FLOAT,
					    sizeof( instanceVars_t ),
					    &tess.instances[0].ambLight, 1 );
			SetAttrPointerInst( &state, AL_DIRECTEDLIGHT,
					    0, 3, GL_FLOAT,
					    sizeof( instanceVars_t ),
					    &tess.instances[0].dirLight, 1 );
			SetAttrPointerInst( &state, AL_LIGHTDIR,
					    0, 4, GL_FLOAT,
					    sizeof( instanceVars_t ),
					    &tess.instances[0].lightDir, 1 );
			SetAttrPointerInst( &state, AL_TEXCOORD2,
					    0, 4, GL_FLOAT,
					    sizeof( instanceVars_t ),
					    &tess.instances[0].texCoord, 1 );
			SetAttrPointerInst( &state, AL_COLOR2,
					    0, 4, GL_UNSIGNED_BYTE,
					    sizeof( instanceVars_t ),
					    &tess.instances[0].color, 1 );
		} else {
			// pseudo-instancing
			n = tess.numInstances;
			setAttrs = qtrue;
		}
	} else {
		// single instance
		n = 1;
		setAttrs = qtrue;
	}

	if( qglBeginQueryARB &&
	    !backEnd.viewParms.portalLevel ) {
		if ( backEnd.currentEntity == &tr.worldEntity ) {
			glQueryID = shader->QueryID;
			glQueryResult = &shader->QueryResult;
		}
	}
	
	if( glQueryID ) {
		GL_StartQuery( glQueryID, glQueryResult );
	}

	for( ibo = input->firstIBO; ibo; ibo = ibo->next ) {
		glIndex_t	minIndex, maxIndex;
		vboInfo_t	*vbo = ibo->vbo;

		if( shader->lightmapIndex == LIGHTMAP_MD3 ) {
			minIndex = ibo->minIndex;
			maxIndex = ibo->maxIndex;
			
			SetAttrPointer( &state, AL_VERTEX, vbo->vbo,
					4, GL_FLOAT, sizeof(vec4_t),
					vbo->offset );
			SetAttrVec4f( &state, AL_NORMAL,
				      0.0f, 0.0f, 0.0f, 0.0f );
			SetAttrVec4f( &state, AL_TEXCOORD,
				      0.0f, 0.0f, 0.0f, 0.0f );
			SetAttrVec4f( &state, AL_COLOR,
				      1.0f, 1.0f, 1.0f, 1.0f );
		} else {
			minIndex = ibo->minIndex;
			maxIndex = ibo->maxIndex;
			
			SetAttrPointer( &state, AL_VERTEX, vbo->vbo,
					4, GL_FLOAT, sizeof(glVertex_t),
					&vbo->offset->xyz );
			SetAttrPointer( &state, AL_NORMAL, vbo->vbo,
					3, GL_FLOAT, sizeof(glVertex_t),
					&vbo->offset->normal );
			SetAttrPointer( &state, AL_COLOR, vbo->vbo,
					4, GL_UNSIGNED_BYTE, sizeof(glVertex_t),
					&vbo->offset->color );
			SetAttrPointer( &state, AL_TEXCOORD, vbo->vbo,
					4, GL_FLOAT, sizeof(glVertex_t),
					&vbo->offset->tc1[0] );
		}

		if( setAttrs ) {
			for( i = 0; i < n; i++ ) {
				SetAttrVec4f( &state, AL_TIMES,
					      tess.instances[i].times[0],
					      tess.instances[i].times[1],
					      tess.instances[i].times[2],
					      tess.instances[i].times[3] );
				SetAttrVec4f( &state, AL_TRANSX,
					      tess.instances[i].transX[0],
					      tess.instances[i].transX[1],
					      tess.instances[i].transX[2],
					      tess.instances[i].transX[3] );
				SetAttrVec4f( &state, AL_TRANSY,
					      tess.instances[i].transY[0],
					      tess.instances[i].transY[1],
					      tess.instances[i].transY[2],
					      tess.instances[i].transY[3] );
				SetAttrVec4f( &state, AL_TRANSZ,
					      tess.instances[i].transZ[0],
					      tess.instances[i].transZ[1],
					      tess.instances[i].transZ[2],
					      tess.instances[i].transZ[3] );
				SetAttrVec4f( &state, AL_TEXCOORD2,
					      tess.instances[i].texCoord[0],
					      tess.instances[i].texCoord[1],
					      tess.instances[i].texCoord[2],
					      tess.instances[i].texCoord[3] );
				SetAttrVec4f( &state, AL_AMBIENTLIGHT,
					      tess.instances[i].ambLight[0],
					      tess.instances[i].ambLight[1],
					      tess.instances[i].ambLight[2],
					      tess.instances[i].ambLight[3] );
				SetAttrVec4f( &state, AL_DIRECTEDLIGHT,
					      tess.instances[i].dirLight[0],
					      tess.instances[i].dirLight[1],
					      tess.instances[i].dirLight[2],
					      0.0f );
				SetAttrVec4f( &state, AL_LIGHTDIR,
					      tess.instances[i].lightDir[0],
					      tess.instances[i].lightDir[1],
					      tess.instances[i].lightDir[2],
					      tess.instances[i].lightDir[3] );
				SetAttrVec4f( &state, AL_COLOR2,
					      tess.instances[i].color[0] / 255.0f,
					      tess.instances[i].color[1] / 255.0f,
					      tess.instances[i].color[2] / 255.0f,
					      tess.instances[i].color[3] / 255.0f );

				GL_DrawElements( &state, ibo->numIndexes,
						 ibo->ibo, ibo->offset,
						 minIndex, maxIndex );
			}
		} else {
			GL_DrawInstanced( &state, ibo->numIndexes,
					  ibo->ibo, ibo->offset,
					  minIndex, maxIndex,
					  tess.numInstances );
		}
	}

	if ( input->numIndexes[1] > 0 ) {
		glVertex_t	*ptr;
		GLuint		vbo;
		
		if ( tess.VtxBuf.mapped ) {
			vbo = tess.VtxBuf.id;
			ptr = BufferOffset( tess.VtxBuf.current );
		} else if ( input->vertexPtr ) {
			vbo = 0;
			ptr = input->vertexPtr;
		} else {
			vbo = backEnd.worldVBO->vbo;
			ptr = backEnd.worldVBO->offset;
		}

		SetAttrPointer( &state, AL_NORMAL, vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				&ptr->normal );
		
		SetAttrPointer( &state, AL_VERTEX, vbo,
				4, GL_FLOAT, sizeof(glVertex_t),
				&ptr->xyz );
		SetAttrPointer( &state, AL_COLOR, vbo,
				4, GL_UNSIGNED_BYTE, sizeof(glVertex_t),
				&ptr->color );
		SetAttrPointer( &state, AL_TEXCOORD, vbo,
				4, GL_FLOAT, sizeof(glVertex_t),
				&ptr->tc1[0] );

		//
		// call shader function
		//
		if( setAttrs ) {
			for( i = 0; i < n; i++ ) {
				SetAttrVec4f( &state, AL_TIMES,
					      tess.instances[i].times[0],
					      tess.instances[i].times[1],
					      tess.instances[i].times[2],
					      tess.instances[i].times[3] );
				SetAttrVec4f( &state, AL_TRANSX,
					      tess.instances[i].transX[0],
					      tess.instances[i].transX[1],
					      tess.instances[i].transX[2],
					      tess.instances[i].transX[3] );
				SetAttrVec4f( &state, AL_TRANSY,
					      tess.instances[i].transY[0],
					      tess.instances[i].transY[1],
					      tess.instances[i].transY[2],
					      tess.instances[i].transY[3] );
				SetAttrVec4f( &state, AL_TRANSZ,
					      tess.instances[i].transZ[0],
					      tess.instances[i].transZ[1],
					      tess.instances[i].transZ[2],
					      tess.instances[i].transZ[3] );
				SetAttrVec4f( &state, AL_TEXCOORD2,
					      tess.instances[i].texCoord[0],
					      tess.instances[i].texCoord[1],
					      tess.instances[i].texCoord[2],
					      tess.instances[i].texCoord[3] );
				SetAttrVec4f( &state, AL_AMBIENTLIGHT,
					      tess.instances[i].ambLight[0],
					      tess.instances[i].ambLight[1],
					      tess.instances[i].ambLight[2],
					      tess.instances[i].ambLight[3] );
				SetAttrVec4f( &state, AL_DIRECTEDLIGHT,
					      tess.instances[i].dirLight[0],
					      tess.instances[i].dirLight[1],
					      tess.instances[i].dirLight[2],
					      0.0f );
				SetAttrVec4f( &state, AL_LIGHTDIR,
					      tess.instances[i].lightDir[0],
					      tess.instances[i].lightDir[1],
					      tess.instances[i].lightDir[2],
					      tess.instances[i].lightDir[3] );
				SetAttrVec4f( &state, AL_COLOR2,
					      tess.instances[i].color[0] / 255.0f,
					      tess.instances[i].color[1] / 255.0f,
					      tess.instances[i].color[2] / 255.0f,
					      tess.instances[i].color[3] / 255.0f );

				if( tess.IdxBuf.mapped ) {
					GL_DrawElements( &state, input->numIndexes[1],
							 tess.IdxBuf.id,
							 BufferOffset(tess.IdxBuf.current),
							 input->minIndex[1],
							 input->maxIndex[1] );
				} else {
					GL_DrawElements( &state, input->numIndexes[1],
							 0, input->indexPtr,
							 input->minIndex[1],
							 input->maxIndex[1] );
				}
			}
		} else {
			if( tess.IdxBuf.mapped ) {
				GL_DrawInstanced( &state, input->numIndexes[1],
						  tess.IdxBuf.id,
						  BufferOffset(tess.IdxBuf.current),
						  input->minIndex[1],
						  input->maxIndex[1],
						  tess.numInstances );
			} else {
				GL_DrawInstanced( &state, input->numIndexes[1],
						  0, input->indexPtr,
						  input->minIndex[1],
						  input->maxIndex[1],
						  tess.numInstances );
			}
		}
	}
	
	if ( r_showtris->integer && !shader->isDepth ) {
		for( ibo = input->firstIBO; ibo; ibo = ibo->next ) {
			int		minIndex, maxIndex;
			vboInfo_t	*vbo = ibo->vbo;
			
			minIndex = ibo->minIndex;
			maxIndex = ibo->maxIndex;

			SetAttrPointer( &state, AL_VERTEX, vbo->vbo,
					4, GL_FLOAT, sizeof(glVertex_t),
					&vbo->offset->xyz );
			
			DrawTris( &state, ibo->numIndexes, ibo->ibo,
				  ibo->offset, ibo->minIndex, ibo->maxIndex,
				  0.0f, 0.0f, 1.0f );
		}
		
		if ( input->numIndexes[1] > 0 ) {
			glVertex_t	*ptr;
			GLuint		vbo;

			if ( input->vertexPtr ) {
				vbo = 0;
				ptr = input->vertexPtr;
			} else {
				vbo = backEnd.worldVBO->vbo;
				ptr = backEnd.worldVBO->offset;
			}

			SetAttrPointer( &state, AL_VERTEX, vbo,
					4, GL_FLOAT, sizeof(glVertex_t),
					&ptr->xyz );

			DrawTris( &state, input->numIndexes[1],
				  0, input->indexPtr,
				  input->minIndex[1], input->maxIndex[1],
				  0.0f, 1.0f, 0.0f );
		}
	}
	if ( r_shownormals->integer ) {
		DrawNormals( &state );
	}

	if( glQueryID ) {
		GL_EndQuery( glQueryID, glQueryResult );
	}
}

/*
** RB_StageIteratorPreparePortal
*/
void RB_StageIteratorPreparePortal( void ) {
	iboInfo_t	*ibo;
	int		level;
	GLuint		stencilVal, stencilMask;
	glRenderState_t state;
	
	InitState( &state );

	// render mirror area to stencil buffer (depth test enabled)
	state.stateBits = GLS_COLORMASK_FALSE;
	state.faceCulling = CT_FRONT_SIDED;
	level = backEnd.viewParms.portalLevel + 1;

	SetAttrVec4f( &state, AL_COLOR,
		      (level&4)?1.0f:0.0f,
		      (level&2)?1.0f:0.0f,
		      (level&1)?1.0f:0.0f,
		      1.0f );

	if( !backEnd.viewParms.portalLevel && !r_measureOverdraw->integer ) {
		qglEnable( GL_STENCIL_TEST );
	}
	
	stencilMask = (level - 1) << glGlobals.shadowBits;
	stencilVal = (level ^ (level >> 1)) << glGlobals.shadowBits;
	
	qglStencilMask( glGlobals.portalMask );
	qglStencilFunc( GL_EQUAL, stencilVal, stencilMask );
	qglStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	
	for( ibo = tess.firstIBO; ibo; ibo = ibo->next ) {
		vboInfo_t	*vbo = ibo->vbo;

		SetAttrPointer( &state, AL_VERTEX, vbo->vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				&vbo->offset->xyz );

		SetAttrVec4f( &state, AL_TEXCOORD, 0.0f, 0.0f, 0.0f, 0.0f );
		
		GL_DrawElements( &state, ibo->numIndexes, ibo->ibo,
				 ibo->offset, ibo->minIndex, ibo->maxIndex );
	}
	
	if ( tess.numIndexes[1] > 0 ) {
		if ( tess.vertexPtr ) {
			SetAttrPointer( &state, AL_VERTEX, 0,
					3, GL_FLOAT, sizeof(glVertex_t),
					&tess.vertexPtr[0].xyz );
		} else {
			SetAttrPointer( &state, AL_VERTEX,
					backEnd.worldVBO->vbo,
					3, GL_FLOAT, sizeof(glVertex_t),
					&backEnd.worldVBO->offset->xyz );
		}
		SetAttrVec4f( &state, AL_TEXCOORD, 0.0f, 0.0f, 0.0f, 0.0f );
		
		//
		// call shader function
		//
		GL_DrawElements( &state, tess.numIndexes[1], 0, tess.indexPtr,
				 tess.minIndex[1], tess.maxIndex[1] );
	}
	
	// set depth to max on mirror area (depth test disabled)
	state.stateBits = GLS_COLORMASK_FALSE | GLS_DEPTHMASK_TRUE |
		GLS_DEPTHRANGE_1_TO_1 | GLS_DEPTHFUNC_ALWAYS;

	if( r_measureOverdraw->integer ) {
		qglStencilMask( glGlobals.shadowMask );
		qglStencilFunc( GL_EQUAL, stencilVal, glGlobals.portalMask );
		qglStencilOp( GL_KEEP, GL_INCR, GL_INCR );
	} else {
		qglStencilFunc( GL_EQUAL, stencilVal, glGlobals.portalMask );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	}
	
	for( ibo = tess.firstIBO; ibo; ibo = ibo->next ) {
		vboInfo_t	*vbo = ibo->vbo;

		SetAttrPointer( &state, AL_VERTEX, vbo->vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				&vbo->offset->xyz );

		GL_DrawElements( &state, ibo->numIndexes, ibo->ibo,
				 ibo->offset, ibo->minIndex, ibo->maxIndex );
	}
	
	if ( tess.numIndexes[1] > 0 ) {
		if ( tess.vertexPtr ) {
			SetAttrPointer( &state, AL_VERTEX, 0,
					3, GL_FLOAT, sizeof(glVertex_t),
					&tess.vertexPtr[0].xyz );
		} else {
			SetAttrPointer( &state, AL_VERTEX,
					backEnd.worldVBO->vbo,
					3, GL_FLOAT, sizeof(glVertex_t),
					&backEnd.worldVBO->offset->xyz );
		}
		
		//
		// call shader function
		//
		GL_DrawElements( &state, tess.numIndexes[1], 0, tess.indexPtr,
				 tess.minIndex[1], tess.maxIndex[1] );
	}
	
	// keep stencil test enabled !
}

/*
** RB_StageIteratorFinalisePortal
*/
void RB_StageIteratorFinalisePortal( void ) {
	iboInfo_t	*ibo;
	int		level;
	GLuint		stencilVal, stencilMask;
	glRenderState_t state;
	
	InitState( &state );
	
	// clear stencil bits
	state.stateBits = GLS_COLORMASK_FALSE | GLS_DEPTHMASK_TRUE |
		GLS_DEPTHRANGE_1_TO_1 | GLS_DEPTHFUNC_ALWAYS;
	state.faceCulling = CT_FRONT_SIDED;
	SetAttrVec4f( &state, AL_COLOR, 0.0f, 0.0f, 0.0f, 0.0f );
	SetAttrVec4f( &state, AL_TEXCOORD, 0.0f, 0.0f, 0.0f, 0.0f );
	
	level = backEnd.viewParms.portalLevel;
	stencilMask = level << glGlobals.shadowBits;
	stencilVal = (level ^ (level >> 1)) << glGlobals.shadowBits;
	
	qglStencilMask( glGlobals.portalMask );
	qglStencilFunc( GL_EQUAL, stencilVal, stencilMask );
	qglStencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
	
	for( ibo = tess.firstIBO; ibo; ibo = ibo->next ) {
		vboInfo_t *vbo = ibo->vbo;

		SetAttrPointer( &state, AL_VERTEX, vbo->vbo,
				3, GL_FLOAT, sizeof(glVertex_t),
				&vbo->offset->xyz );

		GL_DrawElements( &state, ibo->numIndexes, ibo->ibo,
				 ibo->offset, ibo->minIndex, ibo->maxIndex );
	}
	
	if ( tess.numIndexes[1] > 0 ) {
		if ( tess.vertexPtr ) {
			SetAttrPointer( &state, AL_VERTEX, 0,
					3, GL_FLOAT, sizeof(glVertex_t),
					&tess.vertexPtr[0].xyz );
		} else {
			SetAttrPointer( &state, AL_VERTEX,
					backEnd.worldVBO->vbo,
					3, GL_FLOAT, sizeof(glVertex_t),
					&backEnd.worldVBO->offset->xyz );
		}
		
		//
		// call shader function
		//
		GL_DrawElements( &state, tess.numIndexes[1], 0, tess.indexPtr,
				 tess.minIndex[1], tess.maxIndex[1] );
	}

	if( !backEnd.viewParms.portalLevel ) {
		if( r_measureOverdraw->integer ) {
			qglStencilMask( glGlobals.shadowMask );
			qglStencilFunc( GL_ALWAYS, 0, 0 );
			qglStencilOp( GL_KEEP, GL_INCR, GL_INCR );
		} else {
			qglDisable( GL_STENCIL_TEST );
		}
	} else {
		qglStencilFunc( GL_EQUAL, stencilVal, glGlobals.portalMask );
		if( r_measureOverdraw->integer ) {
			qglStencilMask( glGlobals.shadowMask );
			qglStencilOp( GL_KEEP, GL_INCR, GL_INCR );
		} else {
			qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		}
	}
}

/*
** RB_StageIteratorCopyDepth
** Fake stage iterator to copy depth buffer into texture
*/
void RB_StageIteratorCopyDepth( void ) {
	GL_BindTexture( tr.depthImage->texnum );
	qglCopyTexSubImage2D( GL_TEXTURE_2D, 0,
			      backEnd.viewParms.viewportX,
			      backEnd.viewParms.viewportY, 
			      backEnd.viewParms.viewportX,
			      backEnd.viewParms.viewportY, 
			      backEnd.viewParms.viewportWidth,
			      backEnd.viewParms.viewportHeight );
}

/*
** RB_StageIteratorBuildWorldVBO
** Fake stage iterator used to fill a VBO
*/
void RB_StageIteratorBuildWorldVBO( void ) {
	vboInfo_t *vbo = ri.Malloc( sizeof( vboInfo_t ) );

	qglUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
	qglGetIntegerv( GL_ARRAY_BUFFER_BINDING_ARB, (GLint *)&vbo->vbo );
	vbo->offset = (glVertex_t *)NULL;

	backEnd.worldVBO = vbo;
}

/*
** RB_StageIteratorBuildIBO
** Fake stage iterator used to fill an IBO
*/
void RB_StageIteratorBuildIBO( void ) {
	qglUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );
}

/*
** RB_DiscardSurface
**
** Free allocated surface data
*/
void RB_DiscardSurface( void ) {
	if( tess.vertexBuffer ) {
		RB_FreeScratch( tess.vertexBuffer );
		tess.vertexBuffer = NULL;
	}
	if( tess.VtxBuf.mapped )
		qglUnmapBufferARB( GL_ARRAY_BUFFER_ARB );

	if( tess.IdxBuf.mapped )
		qglUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );
	tess.VtxBuf.mapped = tess.IdxBuf.mapped = qfalse;
}

/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	shaderCommands_t *input;
	glRenderState_t	state;

	input = &tess;

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		RB_DiscardSurface();
		return;
	}

	while( tess.indexRange < 2 ) {
		tess.indexRange++;
		tess.numIndexes[tess.indexRange] = tess.numIndexes[tess.indexRange - 1];
	}

	if( tess.VtxBuf.mapped ) {
		GL_VBO( tess.VtxBuf.id );
		qglUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
	}
	if( tess.IdxBuf.mapped ) {
		GL_IBO( tess.IdxBuf.id );
		qglUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );
	}

	// Merge ranges 0 and 1 into ranges 1 and 2.
	// Originally range 0 means surface only, 1 means surface+dlight and
	// 2 means dlight only. After the merge range 1 is all surface and
	// range 2 is all dlight, but they may overlap.
	tess.numIndexes[2] -= tess.numIndexes[0];
	if( tess.minIndex[2] > tess.minIndex[1] )
		tess.minIndex[2] = tess.minIndex[1];
	if( tess.maxIndex[2] < tess.maxIndex[1] )
		tess.maxIndex[2] = tess.maxIndex[1];

	if( tess.minIndex[1] > tess.minIndex[0] )
		tess.minIndex[1] = tess.minIndex[0];
	if( tess.maxIndex[1] < tess.maxIndex[0] )
		tess.maxIndex[1] = tess.maxIndex[0];

	if( tess.shader &&
	    ( input->numIndexes[1] > 0 || input->firstIBO ) ) {
		if ( tess.shader == tr.shadowShader ) {
			RB_ShadowTessEnd();
			RB_DiscardSurface();
			return;
		}

		//
		// update performance counters
		//
		backEnd.pc.c_shaders++;
		backEnd.pc.c_vertexes += tess.numVertexes;
		backEnd.pc.c_indexes += tess.numIndexes[1];
		backEnd.pc.c_totalIndexes += tess.numIndexes[1] * tess.numPasses;

		//
		// call off to shader specific tess end function
		//
		tess.currentStageIteratorFunc();
	}

	//
	// add dynamic lights and fog if necessary
	//
	if( tess.numIndexes[2] > 0 ) {
		InitState( &state );

		//
		// set face culling appropriately
		//
		state.faceCulling = input->shader->cullType;

		SetAttrPointer( &state, AL_VERTEX, 0, 3, GL_FLOAT, sizeof(glVertex_t),
				input->vertexPtr[0].xyz);

		// 
		// now do any dynamic lighting needed
		//
		if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE
		     && !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
			ProjectDlightTexture( &state );
		}
	
		//
		// now do fog
		//
		if ( tess.fogNum && tess.shader->fogPass ) {
			RB_FogPass( &state );
		}
	}
	tess.VtxBuf.mapped = tess.IdxBuf.mapped = qfalse;
	RB_DiscardSurface();

	if ( r_flush->integer ) {
		qglFlush();
	}

	GLimp_LogComment( "----------\n" );
}
