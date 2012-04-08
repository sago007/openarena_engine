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
// tr_light.c

#include TR_CONFIG_H
#include TR_LOCAL_H

#define	DLIGHT_AT_RADIUS		16
// at the edge of a dlight's influence, this amount of light will be added

#define	DLIGHT_MINIMUM_RADIUS	16		
// never calculate a range less than this to prevent huge light numbers


/*
===============
R_TransformDlights

Transforms the origins of an array of dlights.
Used by both the front end (for DlightBmodel) and
the back end (before doing the lighting calculation)
===============
*/
void R_TransformDlights( int count, dlight_t *dl, orientationr_t *or) {
	int		i;
	vec3_t	temp;

	for ( i = 0 ; i < count ; i++, dl++ ) {
		VectorSubtract( dl->origin, or->origin, temp );
		dl->transformed[0] = DotProduct( temp, or->axis[0] );
		dl->transformed[1] = DotProduct( temp, or->axis[1] );
		dl->transformed[2] = DotProduct( temp, or->axis[2] );
	}
}

/*
=============
R_DlightBmodel

Determine which dynamic lights may effect this bmodel
=============
*/
void R_DlightBmodel( bmodel_t *bmodel ) {
	int			i, j;
	dlight_t	*dl;
	int			mask;
	msurface_t	*surf;

	// transform all the lights
	R_TransformDlights( tr.refdef.num_dlights, tr.refdef.dlights, &tr.or );

	mask = 0;
	for ( i=0 ; i<tr.refdef.num_dlights ; i++ ) {
		dl = &tr.refdef.dlights[i];

		// see if the point is close enough to the bounds to matter
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( dl->transformed[j] - bmodel->bounds[1][j] > dl->radius ) {
				break;
			}
			if ( bmodel->bounds[0][j] - dl->transformed[j] > dl->radius ) {
				break;
			}
		}
		if ( j < 3 ) {
			continue;
		}

		// we need to check this light
		mask |= 1 << i;
	}

	tr.currentEntity->needDlights = (mask != 0);

	// set the dlight bits in all the surfaces
	for ( i = 0 ; i < bmodel->numSurfaces ; i++ ) {
		surf = bmodel->firstSurface + i;

		if ( surf->type == SF_FACE ) {
			((srfSurfaceFace_t *)surf->data)->dlightBits[ tr.smpFrame ] = mask;
		} else if ( surf->type == SF_GRID ) {
			((srfGridMesh_t *)surf->data)->dlightBits[ tr.smpFrame ] = mask;
		} else if ( surf->type == SF_TRIANGLES ) {
			((srfTriangles_t *)surf->data)->dlightBits[ tr.smpFrame ] = mask;
		}
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

extern	cvar_t	*r_ambientScale;
extern	cvar_t	*r_directedScale;
extern	cvar_t	*r_debugLight;

/*
=================
R_SetupEntityLightingGrid

=================
*/
static void R_SetupEntityLightingGrid( trRefEntity_t *ent ) {
	vec3_t	lightOrigin;
	int		pos[3];
	int		i, j;
	byte	*gridData;
	float	frac[3];
	int		gridStep[3];
	vec3_t	direction;
	float	totalFactor;

	if ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) {
		// seperate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy( ent->e.lightingOrigin, lightOrigin );
	} else {
		VectorCopy( ent->e.origin, lightOrigin );
	}

	VectorSubtract( lightOrigin, tr.world->lightGridOrigin, lightOrigin );
	for ( i = 0 ; i < 3 ; i++ ) {
		float	v;

		v = lightOrigin[i]*tr.world->lightGridInverseSize[i];
		pos[i] = floor( v );
		frac[i] = v - pos[i];
		if ( pos[i] < 0 ) {
			pos[i] = 0;
		} else if ( pos[i] >= tr.world->lightGridBounds[i] - 1 ) {
			pos[i] = tr.world->lightGridBounds[i] - 1;
		}
	}

	VectorClear( ent->ambientLight );
	VectorClear( ent->directedLight );
	VectorClear( direction );

	assert( tr.world->lightGridData ); // NULL with -nolight maps

	// trilerp the light value
	gridStep[0] = 8;
	gridStep[1] = 8 * tr.world->lightGridBounds[0];
	gridStep[2] = 8 * tr.world->lightGridBounds[0] * tr.world->lightGridBounds[1];
	gridData = tr.world->lightGridData + pos[0] * gridStep[0]
		+ pos[1] * gridStep[1] + pos[2] * gridStep[2];

	totalFactor = 0;
	for ( i = 0 ; i < 8 ; i++ ) {
		float	factor;
		byte	*data;
		int		lat, lng;
		vec3_t	normal;
		#if idppc
		float d0, d1, d2, d3, d4, d5;
		#endif
		factor = 1.0;
		data = gridData;
		for ( j = 0 ; j < 3 ; j++ ) {
			if ( i & (1<<j) ) {
				factor *= frac[j];
				data += gridStep[j];
			} else {
				factor *= (1.0f - frac[j]);
			}
		}

		if ( !(data[0]+data[1]+data[2]) ) {
			continue;	// ignore samples in walls
		}
		totalFactor += factor;
		#if idppc
		d0 = data[0]; d1 = data[1]; d2 = data[2];
		d3 = data[3]; d4 = data[4]; d5 = data[5];

		ent->ambientLight[0] += factor * d0;
		ent->ambientLight[1] += factor * d1;
		ent->ambientLight[2] += factor * d2;

		ent->directedLight[0] += factor * d3;
		ent->directedLight[1] += factor * d4;
		ent->directedLight[2] += factor * d5;
		#else
		ent->ambientLight[0] += factor * data[0];
		ent->ambientLight[1] += factor * data[1];
		ent->ambientLight[2] += factor * data[2];

		ent->directedLight[0] += factor * data[3];
		ent->directedLight[1] += factor * data[4];
		ent->directedLight[2] += factor * data[5];
		#endif
		lat = data[7];
		lng = data[6];
		lat *= (FUNCTABLE_SIZE/256);
		lng *= (FUNCTABLE_SIZE/256);

		// decode X as cos( lat ) * sin( long )
		// decode Y as sin( lat ) * sin( long )
		// decode Z as cos( long )

		normal[0] = tr.sinTable[(lat+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK] * tr.sinTable[lng];
		normal[1] = tr.sinTable[lat] * tr.sinTable[lng];
		normal[2] = tr.sinTable[(lng+(FUNCTABLE_SIZE/4))&FUNCTABLE_MASK];

		VectorMA( direction, factor, normal, direction );
	}

	if ( totalFactor > 0 && totalFactor < 0.99 ) {
		totalFactor = 1.0f / totalFactor;
		VectorScale( ent->ambientLight, totalFactor, ent->ambientLight );
		VectorScale( ent->directedLight, totalFactor, ent->directedLight );
	}

	VectorScale( ent->ambientLight, r_ambientScale->value, ent->ambientLight );
	VectorScale( ent->directedLight, r_directedScale->value, ent->directedLight );

	VectorNormalize2( direction, ent->lightDir );
}


/*
===============
LogLight
===============
*/
static void LogLight( trRefEntity_t *ent ) {
	int	max1, max2;

	if ( !(ent->e.renderfx & RF_FIRST_PERSON ) ) {
		return;
	}

	max1 = ent->ambientLight[0];
	if ( ent->ambientLight[1] > max1 ) {
		max1 = ent->ambientLight[1];
	} else if ( ent->ambientLight[2] > max1 ) {
		max1 = ent->ambientLight[2];
	}

	max2 = ent->directedLight[0];
	if ( ent->directedLight[1] > max2 ) {
		max2 = ent->directedLight[1];
	} else if ( ent->directedLight[2] > max2 ) {
		max2 = ent->directedLight[2];
	}

	ri.Printf( PRINT_ALL, "amb:%i  dir:%i\n", max1, max2 );
}

/*
=================
R_SetupEntityLighting

Calculates all the lighting values that will be used
by the Calc_* functions
=================
*/
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent ) {
	int				i;
	dlight_t		*dl;
	float			power;
	vec3_t			dir;
	float			d;
	vec3_t			lightDir;
	vec3_t			lightOrigin;

	// lighting calculations 
	if ( ent->lightingCalculated ) {
		return;
	}
	ent->lightingCalculated = qtrue;

	//
	// trace a sample point down to find ambient light
	//
	if ( ent->e.renderfx & RF_LIGHTING_ORIGIN ) {
		// seperate lightOrigins are needed so an object that is
		// sinking into the ground can still be lit, and so
		// multi-part models can be lit identically
		VectorCopy( ent->e.lightingOrigin, lightOrigin );
	} else {
		VectorCopy( ent->e.origin, lightOrigin );
	}

	// if NOWORLDMODEL, only use dynamic lights (menu system, etc)
	if ( !(refdef->rdflags & RDF_NOWORLDMODEL ) 
		&& tr.world->lightGridData ) {
		R_SetupEntityLightingGrid( ent );
	} else {
		ent->ambientLight[0] = ent->ambientLight[1] = 
			ent->ambientLight[2] = tr.identityLight * 150;
		ent->directedLight[0] = ent->directedLight[1] = 
			ent->directedLight[2] = tr.identityLight * 150;
		VectorCopy( tr.sunDirection, ent->lightDir );
	}

	// bonus items and view weapons have a fixed minimum add
	if ( 1 /* ent->e.renderfx & RF_MINLIGHT */ ) {
		// give everything a minimum light add
		ent->ambientLight[0] += tr.identityLight * 32;
		ent->ambientLight[1] += tr.identityLight * 32;
		ent->ambientLight[2] += tr.identityLight * 32;
	}

	//
	// modify the light by dynamic lights
	//
	d = VectorLength( ent->directedLight );
	VectorScale( ent->lightDir, d, lightDir );

	if( !backEnd.dlightBuffer ) {
		for ( i = 0 ; i < refdef->num_dlights ; i++ ) {
			dl = &refdef->dlights[i];
			VectorSubtract( dl->origin, lightOrigin, dir );
			d = VectorNormalize( dir );

			power = DLIGHT_AT_RADIUS * ( dl->radius * dl->radius );
			if ( d < DLIGHT_MINIMUM_RADIUS ) {
				d = DLIGHT_MINIMUM_RADIUS;
			}
			d = power / ( d * d );

			VectorMA( ent->directedLight, d, dl->color, ent->directedLight );
			VectorMA( lightDir, d, dir, lightDir );
		}
	}

	// clamp ambient
	for ( i = 0 ; i < 3 ; i++ ) {
		if ( ent->ambientLight[i] > tr.identityLightByte ) {
			ent->ambientLight[i] = tr.identityLightByte;
		}
	}

	if ( r_debugLight->integer ) {
		LogLight( ent );
	}

	// save out the byte packet version
	((byte *)&ent->ambientLightInt)[0] = ri.ftol(ent->ambientLight[0]);
	((byte *)&ent->ambientLightInt)[1] = ri.ftol(ent->ambientLight[1]);
	((byte *)&ent->ambientLightInt)[2] = ri.ftol(ent->ambientLight[2]);
	((byte *)&ent->ambientLightInt)[3] = 0xff;
	
	// transform the direction to local space
	VectorNormalize( lightDir );
	ent->lightDir[0] = DotProduct( lightDir, ent->e.axis[0] );
	ent->lightDir[1] = DotProduct( lightDir, ent->e.axis[1] );
	ent->lightDir[2] = DotProduct( lightDir, ent->e.axis[2] );
}

/*
=================
R_LightForPoint
=================
*/
int R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir )
{
	trRefEntity_t ent;
	
	if ( tr.world->lightGridData == NULL )
	  return qfalse;

	Com_Memset(&ent, 0, sizeof(ent));
	VectorCopy( point, ent.e.origin );
	R_SetupEntityLightingGrid( &ent );
	VectorCopy(ent.ambientLight, ambientLight);
	VectorCopy(ent.directedLight, directedLight);
	VectorCopy(ent.lightDir, lightDir);

	return qtrue;
}

/*
=================
BoundingSphere

helper function to generate bounding sphere of two spheres
returns the radius
=================
*/
static float
BoundingSphere(vec3_t o1, float r1, vec3_t o2, float r2, vec3_t oOut) {
	float d = Distance( o1, o2 );
	float factor;

	if( r1 >= r2 + d ) {
		VectorCopy( o1, oOut );
		return r1;
	}
	if( r2 >= r1 + d ) {
		VectorCopy( o2, oOut );
		return r2;
	}
	factor = 0.5f * (r2 + d - r1) / d;
	oOut[0] = o1[0] + factor * (o2[0] - o1[0]);
	oOut[1] = o1[1] + factor * (o2[1] - o1[1]);
	oOut[2] = o1[2] + factor * (o2[2] - o1[2]);
	return 0.5f * (r1 + d + r2);
}

// node of dlight bounding volume tree
struct node {
  vec3_t	origin;
  float		radius;
  int		leaves, left, right;
};

static void
UploadDLights( const trRefdef_t *refdef, struct node *nodes, int root, byte *buffer,
	       int *ofsSpheres, int *ofsColors, int *ofsLinks ) {
	if( root < refdef->num_dlights ) {
		VectorCopy( refdef->dlights[root].origin, (float *)(buffer + *ofsSpheres) );
		((float *)(buffer + *ofsSpheres))[3] = refdef->dlights[root].radius;
		*ofsSpheres += backEnd.uDLSpheres.stride;

		VectorCopy( refdef->dlights[root].color, (float *)(buffer + *ofsColors) );
		*ofsColors += backEnd.uDLColors.stride;

		*(GLint *)(buffer + *ofsLinks) = 1;
		*ofsLinks += backEnd.uDLLinks.stride;
	} else {
		root -= refdef->num_dlights;

		VectorCopy( nodes[root].origin, (float *)(buffer + *ofsSpheres) );
		((float *)(buffer + *ofsSpheres))[3] = nodes[root].radius;
		*ofsSpheres += backEnd.uDLSpheres.stride;

		*(GLint *)(buffer + *ofsLinks) = nodes[root].leaves;
		*ofsLinks += backEnd.uDLLinks.stride;

		UploadDLights( refdef, nodes, nodes[root].left, buffer,
			       ofsSpheres, ofsColors, ofsLinks );
		UploadDLights( refdef, nodes, nodes[root].right, buffer,
			       ofsSpheres, ofsColors, ofsLinks );
	}
}
/*
=================
RB_PrepareDLights
=================
*/
void RB_PrepareDLights( const trRefdef_t *refdef )
{
	struct node nodes[ MAX_SHADER_DLIGHTS - 1 ];
	int  numNodes = 0;
	int  activeNodes[ MAX_SHADER_DLIGHTS ];
	int  numActiveNodes = refdef->num_dlights;
	int  i, j;
	byte *buffer;
	int  ofsSpheres, ofsColors, ofsLinks;
	float *ptrDebug;

	if( !backEnd.dlightBuffer )
		return;

	// bottom-up tree generator
	for( i = 0; i < numActiveNodes; i++ ) {
		activeNodes[i] = i;
	}

	while( numActiveNodes > 1 ) {
		float best = 999999.f;
		int   besti = 0, bestj = 0;

		for( i = 0; i < numActiveNodes; i++) {
			for( j = i + 1; j < numActiveNodes; j++ ) {
				float	r, r1, r2;
				vec3_t	o;
				vec_t	*o1, *o2;
				int	leaves1, leaves2;

				if( activeNodes[i] < refdef->num_dlights ) {
					r1 = 2.0f * refdef->dlights[activeNodes[i]].radius;
					o1 = refdef->dlights[activeNodes[i]].origin;
					leaves1 = 1;
				} else {
					int idx = activeNodes[i] - refdef->num_dlights;
					r1 = nodes[idx].radius;
					o1 = nodes[idx].origin;
					leaves1 = nodes[idx].leaves;
				}
				if( activeNodes[j] < refdef->num_dlights ) {
					r2 = 2.0f * refdef->dlights[activeNodes[j]].radius;
					o2 = refdef->dlights[activeNodes[j]].origin;
					leaves2 = 1;
				} else {
					int idx = activeNodes[j] - refdef->num_dlights;
					r2 = nodes[idx].radius;
					o2 = nodes[idx].origin;
					leaves2 = nodes[idx].leaves;
				}

				r = BoundingSphere(o1, r1, o2, r2, o);
				if( r < best ) {
					besti = i;
					bestj = j;
					VectorCopy(o, nodes[numNodes].origin);
					nodes[numNodes].radius = r;
					nodes[numNodes].left = activeNodes[i];
					nodes[numNodes].right = activeNodes[j];
					nodes[numNodes].leaves = leaves1 + leaves2;
					best = r;
				}
			}
		}

		activeNodes[besti] = refdef->num_dlights + numNodes++;
		for( j = bestj + 1; j < numActiveNodes; j++ ) {
			activeNodes[j - 1] = activeNodes[j];
		}
		numActiveNodes--;
	}

	buffer = RB_AllocScratch( backEnd.dlightBufferSize );

	ofsSpheres = backEnd.uDLSpheres.offset;
	ofsColors = backEnd.uDLColors.offset;
	ofsLinks = backEnd.uDLLinks.offset;
	ptrDebug = (float *)(buffer + backEnd.uDLDebug.offset);
	if( refdef->num_dlights > 0 ) {
		*(int *)(buffer + backEnd.uDLNum.offset) = 2 * refdef->num_dlights - 1;
		UploadDLights( refdef, nodes, activeNodes[0], buffer,
			       &ofsSpheres, &ofsColors, &ofsLinks );
	} else {
		*(int *)(buffer + backEnd.uDLNum.offset) = 0;
	}
	switch( r_lightmap->integer ) {
	case 1:
		ptrDebug[0] = 1.0f / backEnd.viewParms.viewportWidth;
		ptrDebug[1] = 1.0f / backEnd.viewParms.viewportHeight;
		break;
	case 2:
		ptrDebug[0] = 4.0f / backEnd.viewParms.viewportWidth;
		ptrDebug[1] = 3.0f / backEnd.viewParms.viewportHeight;
		break;
	default:
		ptrDebug[0] = 0.0f;
		ptrDebug[1] = 0.0f;
		break;
	}
	GL_UBO( backEnd.dlightBuffer );
	qglBufferDataARB( GL_UNIFORM_BUFFER, backEnd.dlightBufferSize,
			  buffer, GL_DYNAMIC_DRAW_ARB );
	GL_UBO( 0 );
	RB_FreeScratch( buffer );
}
