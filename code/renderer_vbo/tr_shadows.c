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

/*

  for a projection shadow:

  point[x] += light vector * ( z - shadow plane )
  point[y] +=
  point[z] = shadow plane

  1 0 light[x] / light[z]

*/

typedef struct {
	int		i2;
	int		facing;
} edgeDef_t;

#define	MAX_EDGE_DEFS	32

static	edgeDef_t	*edgeDefs;
static	int		*numEdgeDefs;
static	int		*facing;

void R_AddEdgeDef( int i1, int i2, int facing ) {
	int		c;

	c = numEdgeDefs[ i1 ];
	if ( c == MAX_EDGE_DEFS ) {
		return;		// overflow
	}
	edgeDefs[ i1 * MAX_EDGE_DEFS + c ].i2 = i2;
	edgeDefs[ i1 * MAX_EDGE_DEFS + c ].facing = facing;

	numEdgeDefs[ i1 ]++;
}

void R_RenderShadowEdges( glRenderState_t *state ) {
	int		i, idx = 0;
	int		numTris = tess.numIndexes[1] / 3;
	int		numShadowTris = numTris * 6;

#if 0

	// dumb way -- render every triangle's edges
	state->program = tr.shadowShader->GLSLprogram;
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
	SetAttrPointer( state, AL_VERTEX, 0,
			3, GL_FLOAT, sizeof(glVertex_t),
			&tess.vertexPtr[0].xyz );

	if ( tess.indexInc == sizeof( GLuint ) ) {
		GLuint *indexPtr32 = tess.indexPtr.p32;
		GLuint *indexes;

		indexes = RB_AllocScratch( 3 * numShadowTris * sizeof( GLuint ) );

		for ( i = 0 ; i < numTris ; i++ ) {
			int		i1, i2, i3;

			if ( !facing[i] ) {
				continue;
			}

			i1 = indexPtr32[ i*3 + 0 ];
			i2 = indexPtr32[ i*3 + 1 ];
			i3 = indexPtr32[ i*3 + 2 ];
			
			indexes32[idx++] = i1;
			indexes32[idx++] = i1 + tess.numVertexes;
			indexes32[idx++] = i2;
			indexes32[idx++] = i2;
			indexes32[idx++] = i1 + tess.numVertexes;
			indexes32[idx++] = i2 + tess.numVertexes;

			indexes32[idx++] = i2;
			indexes32[idx++] = i2 + tess.numVertexes;
			indexes32[idx++] = i3;
			indexes32[idx++] = i3;
			indexes32[idx++] = i2 + tess.numVertexes;
			indexes32[idx++] = i3 + tess.numVertexes;

			indexes32[idx++] = i3;
			indexes32[idx++] = i3 + tess.numVertexes;
			indexes32[idx++] = i1;
			indexes32[idx++] = i1;
			indexes32[idx++] = i3 + tess.numVertexes;
			indexes32[idx++] = i1 + tess.numVertexes;
		}
		GL_DrawElements( state, idx, 0, indexes32, 0, idx-1, 65537 );
		RB_FreeScratch( indexes );
	} else {
		glIndex_t	*indexPtr = tess.indexPtr;
		glIndex_t	*indexes;
		
		indexes = RB_AllocScratch( 3 * numShadowTris * sizeof( glIndex_t ) );
	
		for ( i = 0 ; i < numTris ; i++ ) {
			glIndex_t	i1, i2, i3;
			
			if ( !facing[i] ) {
				continue;
			}

			i1 = indexPtr[ i*3 + 0 ];
			i2 = indexPtr[ i*3 + 1 ];
			i3 = indexPtr[ i*3 + 2 ];

			indexes[idx++] = i1;
			indexes[idx++] = i1 + tess.numVertexes;
			indexes[idx++] = i2;
			indexes[idx++] = i2;
			indexes[idx++] = i1 + tess.numVertexes;
			indexes[idx++] = i2 + tess.numVertexes;

			indexes[idx++] = i2;
			indexes[idx++] = i2 + tess.numVertexes;
			indexes[idx++] = i3;
			indexes[idx++] = i3;
			indexes[idx++] = i2 + tess.numVertexes;
			indexes[idx++] = i3 + tess.numVertexes;

			indexes[idx++] = i3;
			indexes[idx++] = i3 + tess.numVertexes;
			indexes[idx++] = i1;
			indexes[idx++] = i1;
			indexes[idx++] = i3 + tess.numVertexes;
			indexes[idx++] = i1 + tess.numVertexes;
		}
		GL_DrawElements( state, idx, 0, indexes, 0, idx-1 );
		RB_FreeScratch( indexes );
	}
#else
	int		c, c2;
	int		j, k;
	int		i2;
	int		c_edges, c_rejected;
	int		hit[2];
	glIndex_t	*indexes;

	// an edge is NOT a silhouette edge if its face doesn't face the light,
	// or if it has a reverse paired edge that also faces the light.
	// A well behaved polyhedron would have exactly two faces for each edge,
	// but lots of models have dangling edges or overfanned edges
	c_edges = 0;
	c_rejected = 0;
	state->program = tr.shadowShader->GLSLprogram;
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
	SetAttrPointer( state, AL_VERTEX, 0,
			3, GL_FLOAT, sizeof(glVertex_t),
			&tess.vertexPtr[0].xyz );

	indexes = RB_AllocScratch( 3 * numShadowTris * sizeof( glIndex_t ) );

	for ( i = 0 ; i < tess.numVertexes ; i++ ) {
		c = numEdgeDefs[ i ];
		for ( j = 0 ; j < c ; j++ ) {
			if ( !edgeDefs[ i * MAX_EDGE_DEFS + j ].facing ) {
				continue;
			}

			hit[0] = 0;
			hit[1] = 0;

			i2 = edgeDefs[ i * MAX_EDGE_DEFS + j ].i2;
			c2 = numEdgeDefs[ i2 ];
			for ( k = 0 ; k < c2 ; k++ ) {
				if ( edgeDefs[ i2 * MAX_EDGE_DEFS + k ].i2 == i ) {
					hit[ edgeDefs[ i2 * MAX_EDGE_DEFS + k ].facing ]++;
				}
			}

			// if it doesn't share the edge with another front facing
			// triangle, it is a sil edge
			if ( hit[ 1 ] == 0 ) {
				indexes[idx++] = i;
				indexes[idx++] = i + tess.numVertexes;
				indexes[idx++] = i2;
				indexes[idx++] = i2;
				indexes[idx++] = i + tess.numVertexes;
				indexes[idx++] = i2 + tess.numVertexes;
				c_edges++;
			} else {
				c_rejected++;
			}
		}
	}
	GL_DrawElements( state, idx, 0, indexes,
			 0, 2*tess.numVertexes - 1 );
	RB_FreeScratch( indexes );
#endif
}

/*
=================
RB_ShadowTessEnd

triangleFromEdge[ v1 ][ v2 ]


  set triangle from edge( v1, v2, tri )
  if ( facing[ triangleFromEdge[ v1 ][ v2 ] ] && !facing[ triangleFromEdge[ v2 ][ v1 ] ) {
  }
=================
*/
void RB_ShadowTessEnd( void ) {
	int		i;
	int		numTris;
	vec3_t	lightDir;
	glRenderState_t state;

	if ( glConfig.stencilBits < 4 ) {
		return;
	}

	VectorCopy( backEnd.currentEntity->lightDir, lightDir );

	// project vertexes away from light direction
	for ( i = 0 ; i < tess.numVertexes ; i++ ) {
		VectorMA( tess.vertexPtr[i].xyz, -512,
			  lightDir, tess.vertexPtr[i+tess.numVertexes].xyz );
	}

	// decide which triangles face the light
	numEdgeDefs = RB_AllocScratch( sizeof( int ) * tess.numVertexes );
	edgeDefs = RB_AllocScratch( sizeof( edgeDef_t ) * tess.numVertexes * MAX_EDGE_DEFS );
	facing = RB_AllocScratch( sizeof( int ) * tess.numIndexes[1] / 3 );
	Com_Memset( numEdgeDefs, 0, sizeof( int ) * tess.numVertexes );

	numTris = tess.numIndexes[1] / 3;
	for ( i = 0 ; i < numTris ; i++ ) {
		glIndex_t	i1, i2, i3;
		vec3_t	d1, d2, normal;
		vec3_t	*v1, *v2, *v3;
		float	d;

		glIndex_t *indexPtr = tess.indexPtr;
		i1 = indexPtr[ i*3 + 0 ];
		i2 = indexPtr[ i*3 + 1 ];
		i3 = indexPtr[ i*3 + 2 ];

		v1 = &tess.vertexPtr[i1].xyz;
		v2 = &tess.vertexPtr[i2].xyz;
		v3 = &tess.vertexPtr[i3].xyz;

		VectorSubtract( *v2, *v1, d1 );
		VectorSubtract( *v3, *v1, d2 );
		CrossProduct( d1, d2, normal );

		d = DotProduct( normal, lightDir );
		if ( d > 0 ) {
			facing[ i ] = 1;
		} else {
			facing[ i ] = 0;
		}

		// create the edges
		R_AddEdgeDef( i1, i2, facing[ i ] );
		R_AddEdgeDef( i2, i3, facing[ i ] );
		R_AddEdgeDef( i3, i1, facing[ i ] );
	}

	// draw the silhouette edges
	InitState( &state );

	state.numImages = 1;
	state.image[0] = tr.whiteImage;
	state.stateBits = GLS_COLORMASK_FALSE;
	SetAttrVec4f( &state, AL_COLOR, 1.0f, 0.2f, 1.0f, 1.0f );

	if( !backEnd.viewParms.portalLevel ) {
		qglEnable( GL_STENCIL_TEST );
		qglStencilFunc( GL_ALWAYS, 1, glGlobals.shadowMask );
	}
	qglStencilMask( glGlobals.shadowMask );

	if( qglStencilOpSeparate ) {
		// single pass, doesn't matter if we incr or decr as we check
		// for != 0...
		state.faceCulling = CT_TWO_SIDED;
		qglStencilOpSeparate( GL_BACK, GL_KEEP, GL_KEEP, GL_INCR_WRAP );
		qglStencilOpSeparate( GL_FRONT, GL_KEEP, GL_KEEP, GL_DECR_WRAP );
	} else {
		// two passes for front/back faces
		state.faceCulling = CT_BACK_SIDED;
		qglStencilOp( GL_KEEP, GL_KEEP, GL_INCR );
		
		R_RenderShadowEdges( &state );
		
		state.faceCulling = CT_FRONT_SIDED;
		qglStencilOp( GL_KEEP, GL_KEEP, GL_DECR );
	}
	R_RenderShadowEdges( &state );

	qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

	RB_FreeScratch( facing );
	RB_FreeScratch( edgeDefs );
	RB_FreeScratch( numEdgeDefs );
}


/*
=================
RB_ShadowFinish

Darken everything that is is a shadow volume.
We have to delay this until everything has been shadowed,
because otherwise shadows from different body parts would
overlap and double darken.
=================
*/
void RB_ShadowFinish( void ) {
	static vec3_t	vertexes[4] = {
		{ -100,  100, -10 },
		{  100,  100, -10 },
		{  100, -100, -10 },
		{ -100, -100, -10 }
	};
	glRenderState_t state;
	
	if ( r_shadows->integer != 2 ) {
		return;
	}
	if ( glConfig.stencilBits < 4 ) {
		return;
	}
	qglEnable( GL_STENCIL_TEST );
	qglStencilFunc( GL_NOTEQUAL, 0, glGlobals.shadowMask );
	qglStencilMask( glGlobals.shadowMask );
	qglStencilOp( GL_ZERO, GL_ZERO, GL_ZERO );

	InitState( &state );
	state.faceCulling = CT_TWO_SIDED;

	state.numImages = 1;
	state.image[0] = tr.whiteImage;

	qglLoadIdentity ();

	state.program = NULL;
	SetAttrVec4f( &state, AL_COLOR, 0.6f, 0.6f, 0.6f, 1.0f );
	state.stateBits = GLS_DEPTHMASK_TRUE |
		GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;

	SetAttrPointer( &state, AL_VERTEX, 0,
			3, GL_FLOAT, sizeof(vec3_t),
			vertexes );
	GL_DrawArrays( &state, GL_QUADS, 0, 4 );

	if( !backEnd.viewParms.portalLevel ) {
		qglDisable( GL_STENCIL_TEST );
	} else {
		int		level;
		GLuint		stencilVal;
		level = backEnd.viewParms.portalLevel;
		stencilVal = (level ^ (level >> 1)) << glGlobals.shadowBits;
		
		qglStencilFunc( GL_EQUAL, stencilVal, glGlobals.portalMask );
		qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
	}
}


/*
=================
RB_ProjectionShadowDeform

=================
*/
void RB_ProjectionShadowDeform( void ) {
	int		i;
	float	h;
	vec3_t	ground;
	vec3_t	light;
	float	groundDist;
	float	d;
	vec3_t	lightDir;

	ground[0] = backEnd.or.axis[0][2];
	ground[1] = backEnd.or.axis[1][2];
	ground[2] = backEnd.or.axis[2][2];

	groundDist = backEnd.or.origin[2] - backEnd.currentEntity->e.shadowPlane;

	VectorCopy( backEnd.currentEntity->lightDir, lightDir );
	d = DotProduct( lightDir, ground );
	// don't let the shadows get too long or go negative
	if ( d < 0.5 ) {
		VectorMA( lightDir, (0.5 - d), ground, lightDir );
		d = DotProduct( lightDir, ground );
	}
	d = 1.0 / d;

	light[0] = lightDir[0] * d;
	light[1] = lightDir[1] * d;
	light[2] = lightDir[2] * d;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		h = DotProduct( tess.vertexPtr[i].xyz, ground ) + groundDist;

		tess.vertexPtr[i].xyz[0] -= light[0] * h;
		tess.vertexPtr[i].xyz[1] -= light[1] * h;
		tess.vertexPtr[i].xyz[2] -= light[2] * h;
	}
}
