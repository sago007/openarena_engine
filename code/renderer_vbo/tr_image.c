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
// tr_image.c

#include TR_CONFIG_H
#include TR_LOCAL_H

static byte			 s_intensitytable[256];
static unsigned char s_gammatable[256];

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

#define FILE_HASH_SIZE		1024
static	image_t*		hashTable[FILE_HASH_SIZE];

/*
** R_GammaCorrect
*/
void R_GammaCorrect( byte *buffer, int bufSize ) {
	int i;

	for ( i = 0; i < bufSize; i++ ) {
		buffer[i] = s_gammatable[buffer[i]];
	}
}

typedef struct {
	char *name;
	int	minimize, maximize;
} textureMode_t;

textureMode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower(fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash &= (FILE_HASH_SIZE-1);
	return hash;
}

/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode( const char *string ) {
	int		i;
	image_t	*glt;

	for ( i=0 ; i< 6 ; i++ ) {
		if ( !Q_stricmp( modes[i].name, string ) ) {
			break;
		}
	}

	// hack to prevent trilinear from being set on voodoo,
	// because their driver freaks...
	if ( i == 5 && glConfig.hardwareType == GLHW_3DFX_2D3D ) {
		ri.Printf( PRINT_ALL, "Refusing to set trilinear on a voodoo.\n" );
		i = 3;
	}


	if ( i == 6 ) {
		ri.Printf (PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// bound texture anisotropy

	if(glGlobals.textureFilterAnisotropic)
	{
		if(r_ext_texture_filter_anisotropic->value > glGlobals.maxAnisotropy)
		{
			ri.Cvar_Set("r_ext_texture_filter_anisotropic", va("%d", glGlobals.maxAnisotropy));
		}
		else if(r_ext_texture_filter_anisotropic->value < 1.0)
		{
			ri.Cvar_Set("r_ext_texture_filter_anisotropic", "1.0");
		}
	}

	// change all the existing mipmap texture objects
	for ( i = 0 ; i < tr.numImages ; i++ ) {
		glt = tr.images[ i ];
		if ( glt->mipmap ) {
			GL_BindTexture ( glt->texnum );
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
            
			// set texture anisotropy
			if(glGlobals.textureFilterAnisotropic)
				qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, r_ext_texture_filter_anisotropic->value);
		}
	}
}

/*
===============
R_SumOfUsedImages
===============
*/
int R_SumOfUsedImages( void ) {
	int	total;
	int i;

	total = 0;
	for ( i = 0; i < tr.numImages; i++ ) {
		if ( tr.images[i]->frameUsed == tr.frameCount ) {
			total += tr.images[i]->uploadWidth * tr.images[i]->uploadHeight;
		}
	}

	return total;
}

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f( void ) {
	int		i;
	image_t	*image;
	int		texels;
	const char *yesno[] = {
		"no ", "yes"
	};

	ri.Printf (PRINT_ALL, "\n      -w-- -h-- -mm- -TMU- -if-- wrap --name-------\n");
	texels = 0;

	for ( i = 0 ; i < tr.numImages ; i++ ) {
		image = tr.images[ i ];

		texels += image->uploadWidth*image->uploadHeight;
		ri.Printf (PRINT_ALL,  "%4i: %4i %4i  %s   %d   ",
			i, image->uploadWidth, image->uploadHeight, yesno[image->mipmap], image->TMU );
		switch ( image->internalFormat ) {
		case 1:
			ri.Printf( PRINT_ALL, "I    " );
			break;
		case 2:
			ri.Printf( PRINT_ALL, "IA   " );
			break;
		case 3:
			ri.Printf( PRINT_ALL, "RGB  " );
			break;
		case 4:
			ri.Printf( PRINT_ALL, "RGBA " );
			break;
		case GL_RGBA8:
			ri.Printf( PRINT_ALL, "RGBA8" );
			break;
		case GL_RGB8:
			ri.Printf( PRINT_ALL, "RGB8" );
			break;
		case GL_RGB4_S3TC:
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
			ri.Printf( PRINT_ALL, "S3TC " );
			break;
		case GL_RGBA4:
			ri.Printf( PRINT_ALL, "RGBA4" );
			break;
		case GL_RGB5:
			ri.Printf( PRINT_ALL, "RGB5 " );
			break;
		default:
			ri.Printf( PRINT_ALL, "???? " );
		}

		switch ( image->wrapClampMode ) {
		case GL_REPEAT:
			ri.Printf( PRINT_ALL, "rept " );
			break;
		case GL_CLAMP_TO_EDGE:
			ri.Printf( PRINT_ALL, "clmp " );
			break;
		default:
			ri.Printf( PRINT_ALL, "%4i ", image->wrapClampMode );
			break;
		}
		
		ri.Printf( PRINT_ALL, " %s\n", image->imgName );
	}
	ri.Printf (PRINT_ALL, " ---------\n");
	ri.Printf (PRINT_ALL, " %i total texels (not including mipmaps)\n", texels);
	ri.Printf (PRINT_ALL, " %i total images\n\n", tr.numImages );
}

//=======================================================================

/*
================
ResampleTexture

Used to resample images in a more general than quartering fashion.

This will only be filtered properly if the resampled size
is greater than half the original size.

If a larger shrinking is needed, use the mipmap function 
before or after.
================
*/
static void ResampleTexture( const byte *in, int inwidth, int inheight,
			     byte *out, int outwidth, int outheight ) {
	int		i, j;
	const byte	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	p1[2048], p2[2048];
	const byte	*pix1, *pix2, *pix3, *pix4;

	if (outwidth>2048)
		ri.Error(ERR_DROP, "ResampleTexture: max width");
								
	fracstep = inwidth*0x10000/outwidth;

	frac = fracstep>>2;
	for ( i=0 ; i<outwidth ; i++ ) {
		p1[i] = 4*(frac>>16);
		frac += fracstep;
	}
	frac = 3*(fracstep>>2);
	for ( i=0 ; i<outwidth ; i++ ) {
		p2[i] = 4*(frac>>16);
		frac += fracstep;
	}

	for (i=0 ; i<outheight ; i++, out += 4*outwidth) {
		inrow = in + 4*inwidth*(int)((i+0.25)*inheight/outheight);
		inrow2 = in + 4*inwidth*(int)((i+0.75)*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j++) {
			pix1 = inrow + p1[j];
			pix2 = inrow + p2[j];
			pix3 = inrow2 + p1[j];
			pix4 = inrow2 + p2[j];
			out[4*j+0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0])>>2;
			out[4*j+1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1])>>2;
			out[4*j+2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2])>>2;
			out[4*j+3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3])>>2;
		}
	}
}

/*
================
R_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
void R_LightScaleTexture (byte *in, int inwidth, int inheight, qboolean only_gamma )
{
	if ( only_gamma )
	{
		if ( !glConfig.deviceSupportsGamma )
		{
			int	i, c;
			byte	*p;

			p = in;

			c = inwidth*inheight;
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[p[0]];
				p[1] = s_gammatable[p[1]];
				p[2] = s_gammatable[p[2]];
			}
		}
	}
	else
	{
		int	i, c;
		byte	*p;

		p = in;

		c = inwidth*inheight;

		if ( glConfig.deviceSupportsGamma )
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_intensitytable[p[0]];
				p[1] = s_intensitytable[p[1]];
				p[2] = s_intensitytable[p[2]];
			}
		}
		else
		{
			for (i=0 ; i<c ; i++, p+=4)
			{
				p[0] = s_gammatable[s_intensitytable[p[0]]];
				p[1] = s_gammatable[s_intensitytable[p[1]]];
				p[2] = s_gammatable[s_intensitytable[p[2]]];
			}
		}
	}
}


/*
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter
================
*/
static void R_MipMap2( unsigned *in, int inWidth, int inHeight ) {
	int			i, j, k;
	byte		*outpix;
	int			inWidthMask, inHeightMask;
	int			total;
	int			outWidth, outHeight;
	unsigned	*temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;
	temp = ri.Hunk_AllocateTempMemory( outWidth * outHeight * 4 );

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for ( i = 0 ; i < outHeight ; i++ ) {
		for ( j = 0 ; j < outWidth ; j++ ) {
			outpix = (byte *) ( temp + i * outWidth + j );
			for ( k = 0 ; k < 4 ; k++ ) {
				total = 
					1 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((byte *)&in[ ((i*2-1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					2 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					4 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2+1)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k] +

					1 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2-1)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2)&inWidthMask) ])[k] +
					2 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+1)&inWidthMask) ])[k] +
					1 * ((byte *)&in[ ((i*2+2)&inHeightMask)*inWidth + ((j*2+2)&inWidthMask) ])[k];
				outpix[k] = total / 36;
			}
		}
	}

	Com_Memcpy( in, temp, outWidth * outHeight * 4 );
	ri.Hunk_FreeTempMemory( temp );
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap (byte *in, int width, int height) {
	int		i, j;
	byte	*out;
	int		row;
	int	w0, w1, w2, w3, w4, w5;

	if ( !r_simpleMipMaps->integer ) {
		R_MipMap2( (unsigned *)in, width, height );
		return;
	}

	if ( width == 1 && height == 1 ) {
		return;
	}

	row = width * 4;
	out = in;

	if( width == 1 ) {
		if( height == 1 ) {
			return;
		} else if( height & 1 ) {
			height >>= 1;
			for( i = 0; i < height; i++, out+=4, in+=8 ) {
				w0 = (height - i) * 0x10000 / (2*height+1);
				w1 = height       * 0x10000 / (2*height+1);
				w2 = 0x10000 - w0 - w1;
				out[0] = (w0 * in[0] + w1 * in[4] + w2 * in[ 8]) >> 16;
				out[1] = (w0 * in[1] + w1 * in[5] + w2 * in[ 9]) >> 16;
				out[2] = (w0 * in[2] + w1 * in[6] + w2 * in[10]) >> 16;
				out[3] = (w0 * in[3] + w1 * in[7] + w2 * in[11]) >> 16;
			}
		} else {
			height >>= 1;
			for( i = 0; i < height; i++, out+=4, in+=8 ) {
				out[0] = (in[0] + in[4]) >> 1;
				out[1] = (in[1] + in[5]) >> 1;
				out[2] = (in[2] + in[6]) >> 1;
				out[3] = (in[3] + in[7]) >> 1;
			}
		}
	} else if( width & 1 ) {
		width >>= 1;
		if( height == 1 ) {
			for( j = 0; j < width; j++, out+=4, in+=8 ) {
				w0 = (width - j) * 0x10000 / (2*width+1);
				w1 = width       * 0x10000 / (2*width+1);
				w2 = 0x10000 - w0 - w1;
				out[0] = (w0 * in[0] + w1 * in[4] + w2 * in[ 8]) >> 16;
				out[1] = (w0 * in[1] + w1 * in[5] + w2 * in[ 9]) >> 16;
				out[2] = (w0 * in[2] + w1 * in[6] + w2 * in[10]) >> 16;
				out[3] = (w0 * in[3] + w1 * in[7] + w2 * in[11]) >> 16;
			}
		} else if( height & 1 ) {
			height >>= 1;
			for (i=0 ; i<height ; i++, in+=row+4) {
				w0 = (height - i) * 0x100 / (2*height+1);
				w1 = height       * 0x100 / (2*height+1);
				w2 = 0x100 - w0 - w1;
				for (j=0 ; j<width ; j++, out+=4, in+=8) {
					w3 = (width - j) * 0x100 / (2*width+1);
					w4 = width       * 0x100 / (2*width+1);
					w5 = 0x100 - w3 - w4;
					out[0] = (w0 * w3 * in[      0] + w0 * w4 * in[      4] + w0 * w5 * in[       8] +
						  w1 * w3 * in[  row+0] + w1 * w4 * in[  row+4] + w1 * w5 * in[  row+ 8] +
						  w2 * w3 * in[2*row+0] + w2 * w4 * in[2*row+4] + w2 * w5 * in[2*row+ 8]) >> 16;
					out[1] = (w0 * w3 * in[      1] + w0 * w4 * in[      5] + w0 * w5 * in[       9] +
						  w1 * w3 * in[  row+1] + w1 * w4 * in[  row+5] + w1 * w5 * in[  row+ 9] +
						  w2 * w3 * in[2*row+1] + w2 * w4 * in[2*row+5] + w2 * w5 * in[2*row+ 9]) >> 16;
					out[2] = (w0 * w3 * in[      2] + w0 * w4 * in[      6] + w0 * w5 * in[      10] +
						  w1 * w3 * in[  row+2] + w1 * w4 * in[  row+6] + w1 * w5 * in[  row+10] +
						  w2 * w3 * in[2*row+2] + w2 * w4 * in[2*row+6] + w2 * w5 * in[2*row+10]) >> 16;
					out[3] = (w0 * w3 * in[      3] + w0 * w4 * in[      7] + w0 * w5 * in[      11] +
						  w1 * w3 * in[  row+3] + w1 * w4 * in[  row+7] + w1 * w5 * in[  row+11] +
						  w2 * w3 * in[2*row+3] + w2 * w4 * in[2*row+7] + w2 * w5 * in[2*row+11]) >> 16;
				}
			}
		} else {
			height >>= 1;
			for (i=0 ; i<height ; i++, in+=row) {
				for (j=0 ; j<width ; j++, out+=4, in+=8) {
					w0 = (width - j) * 0x10000 / (2*width+1);
					w1 = width       * 0x10000 / (2*width+1);
					w2 = 0x10000 - w0 - w1;
					out[0] = (w0 * (in[0] + in[row+0]) + w1 * (in[4] + in[row+4]) + w2 * (in[ 8] + in[row+ 8]))>>17;
					out[1] = (w0 * (in[1] + in[row+1]) + w1 * (in[5] + in[row+5]) + w2 * (in[ 9] + in[row+ 9]))>>17;
					out[2] = (w0 * (in[2] + in[row+2]) + w1 * (in[6] + in[row+6]) + w2 * (in[10] + in[row+10]))>>17;
					out[3] = (w0 * (in[3] + in[row+3]) + w1 * (in[7] + in[row+7]) + w2 * (in[11] + in[row+11]))>>17;
				}
			}
		}
	} else {
		width >>= 1;
		if( height == 1 ) {
			for( j = 0; j < width; j++, out+=4, in+=8 ) {
				out[0] = (in[0] + in[4]) >> 1;
				out[1] = (in[1] + in[5]) >> 1;
				out[2] = (in[2] + in[6]) >> 1;
				out[3] = (in[3] + in[7]) >> 1;
			}
		} else if( height & 1 ) {
			height >>= 1;
			for (i=0 ; i<height ; i++, in+=row) {
				w0 = (height - i) * 0x10000 / (2*height+1);
				w1 = height       * 0x10000 / (2*height+1);
				w2 = 0x10000 - w0 - w1;
				for (j=0 ; j<width ; j++, out+=4, in+=8) {
					out[0] = (w0 * (in[0] + in[4]) + w1 * (in[row+0] + in[row+4]) + w2 * (in[2*row+0] + in[2*row+4]))>>17;
					out[1] = (w0 * (in[1] + in[5]) + w1 * (in[row+1] + in[row+5]) + w2 * (in[2*row+1] + in[2*row+5]))>>17;
					out[2] = (w0 * (in[2] + in[6]) + w1 * (in[row+2] + in[row+6]) + w2 * (in[2*row+2] + in[2*row+6]))>>17;
					out[3] = (w0 * (in[3] + in[7]) + w1 * (in[row+3] + in[row+7]) + w2 * (in[2*row+3] + in[2*row+7]))>>17;
				}
			}
		} else {
			height >>= 1;
			for (i=0 ; i<height ; i++, in+=row) {
				for (j=0 ; j<width ; j++, out+=4, in+=8) {
					out[0] = (in[0] + in[4] + in[row+0] + in[row+4])>>2;
					out[1] = (in[1] + in[5] + in[row+1] + in[row+5])>>2;
					out[2] = (in[2] + in[6] + in[row+2] + in[row+6])>>2;
					out[3] = (in[3] + in[7] + in[row+3] + in[row+7])>>2;
				}
			}
		}
	}
}

/*
================
R_MipMapHeightMap

Operates in place, quartering the size of the texture
The red & green and blue channels are set the the x, y and z of the
average normal and the alpha channel is the max or average height.
================
*/
static ID_INLINE byte max2(byte a, byte b) { return (a >= b) ? a : b; }
static ID_INLINE byte max4(byte a, byte b, byte c, byte d) { return max2(max2(a, b), max2(c, d)); }
static void R_MipMapHeightMap (byte *in, int width, int height) {
	int	i, j;
	byte	*out;
	int	row;
	vec3_t  normal, normalSum;

	if ( width == 1 && height == 1 ) {
		return;
	}

	row = width * 4;
	out = in;
	width >>= 1;
	height >>= 1;

	if ( width == 0 || height == 0 ) {
		width += height;	// get largest
		for (i=0 ; i<width ; i++, out+=4, in+=8 ) {
			VectorClear( normalSum );

			normal[0] = in[0] * 1.0f/127.5f - 1.0f;
			normal[1] = in[1] * 1.0f/127.5f - 1.0f;
			normal[2] = in[2] * 1.0f/127.5f - 1.0f;
			VectorAdd( normal, normalSum, normalSum );

			normal[0] = in[4] * 1.0f/127.5f - 1.0f;
			normal[1] = in[5] * 1.0f/127.5f - 1.0f;
			normal[2] = in[6] * 1.0f/127.5f - 1.0f;
			VectorAdd( normal, normalSum, normalSum );

			VectorNormalizeFast( normalSum );
			out[0] = (normalSum[0] + 1.0f) * 127.5f;
			out[1] = (normalSum[1] + 1.0f) * 127.5f;
			out[2] = (normalSum[2] + 1.0f) * 127.5f;
			
			out[3] = max2( in[3], in[7] );
		}
		return;
	}

	for (i=0 ; i<height ; i++, in+=row) {
		for (j=0 ; j<width ; j++, out+=4, in+=8) {
			VectorClear( normalSum );

			normal[0] = in[0] * 1.0f/127.5f - 1.0f;
			normal[1] = in[1] * 1.0f/127.5f - 1.0f;
			normal[2] = in[2] * 1.0f/127.5f - 1.0f;
			VectorAdd( normal, normalSum, normalSum );

			normal[0] = in[4] * 1.0f/127.5f - 1.0f;
			normal[1] = in[5] * 1.0f/127.5f - 1.0f;
			normal[2] = in[6] * 1.0f/127.5f - 1.0f;
			VectorAdd( normal, normalSum, normalSum );

			normal[0] = in[row+0] * 1.0f/127.5f - 1.0f;
			normal[1] = in[row+1] * 1.0f/127.5f - 1.0f;
			normal[2] = in[row+2] * 1.0f/127.5f - 1.0f;
			VectorAdd( normal, normalSum, normalSum );

			normal[0] = in[row+4] * 1.0f/127.5f - 1.0f;
			normal[1] = in[row+5] * 1.0f/127.5f - 1.0f;
			normal[2] = in[row+6] * 1.0f/127.5f - 1.0f;
			VectorAdd( normal, normalSum, normalSum );

			VectorNormalizeFast( normalSum );
			out[0] = (normalSum[0] + 1.0f) * 127.5f;
			out[1] = (normalSum[1] + 1.0f) * 127.5f;
			out[2] = (normalSum[2] + 1.0f) * 127.5f;

			out[3] = max4( in[3], in[7],
				       in[row+3], in[row+7] );
		}
	}
	return;
}


/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================
*/
static void R_BlendOverTexture( byte *data, int pixelCount, byte blend[4] ) {
	int		i;
	int		inverseAlpha;
	int		premult[3];

	inverseAlpha = 255 - blend[3];
	premult[0] = blend[0] * blend[3];
	premult[1] = blend[1] * blend[3];
	premult[2] = blend[2] * blend[3];

	for ( i = 0 ; i < pixelCount ; i++, data+=4 ) {
		data[0] = ( data[0] * inverseAlpha + premult[0] ) >> 9;
		data[1] = ( data[1] * inverseAlpha + premult[1] ) >> 9;
		data[2] = ( data[2] * inverseAlpha + premult[2] ) >> 9;
	}
}

byte	mipBlendColors[16][4] = {
	{0,0,0,0},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
	{255,0,0,128},
	{0,255,0,128},
	{0,0,255,128},
};

static qboolean TexFormatSupported( GLenum internalFormat, int width, int height ) {
	qglTexImage2D (GL_PROXY_TEXTURE_2D, 0, internalFormat,
		       width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	qglGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0,
				  GL_TEXTURE_WIDTH, &width);
	return (width != 0);
}


/*
===============
Upload32

Upload a color texture with all required mipmaps
===============
*/
static void Upload32( byte *data,
		      int width, int height, 
		      qboolean mipmap, 
		      qboolean picmip, 
		      qboolean lightMap,
		      int *format, 
		      int *pUploadWidth, int *pUploadHeight,
		      qboolean *hasAlpha, int *maxMipLevel )
{
	int		base_level, max_level;
	int		pot_width, pot_height, w, h, w2, h2;
	qboolean	rescale = qfalse;
	qboolean	hasColor = qfalse;
	int		samples;
	byte		*resampledBuffer = NULL;
	int		i, c;
	byte		*scan;
	GLenum		internalFormat = GL_RGB;

	//
	// convert to exact power of 2 sizes
	//
	for( pot_width = 1 ; pot_width < width ; pot_width<<=1)
		;
	for( pot_height = 1 ; pot_height < height ; pot_height<<=1)
		;

	if ( r_roundImagesDown->integer && pot_width > width )
		pot_width >>= 1;
	if ( r_roundImagesDown->integer && pot_height > height )
		pot_height >>= 1;

	//
	// compute mip levels
	//
	base_level = 0;
	if( picmip && r_picmip->integer >= 0 )
		base_level = r_picmip->integer;

	w = width;
	h = height;
	max_level = 0;
	while( w > 1 || h > 1 ) {
		// OpenGL rounds down non-pot sizes
		w >>= 1;
		h >>= 1;
		max_level++;
	}

	// find best texture format supported
	w = width; h = height; w2 = pot_width; h2 = pot_height;
	for( i = 0; i <= max_level; i++ ) {
		if( i < base_level )
			continue;
		if( TexFormatSupported( GL_RGBA8, w, h ) ) {
			break;
		}
		if( TexFormatSupported( GL_RGBA8, w2, h2 ) ) {
			rescale = qtrue;
			break;
		}
	}
	base_level = i;
	if( !mipmap ) {
		max_level = base_level;
	}

	if ( rescale ) {
		resampledBuffer = ri.Hunk_AllocateTempMemory( pot_width * pot_height * 4 );
		ResampleTexture (data, width, height, resampledBuffer, pot_width, pot_height);
		data = resampledBuffer;

		width = pot_width;
		height = pot_height;
	}

	//
	// scan the texture for each channel's max values
	// and verify if the alpha channel is being used or not
	//
	c = width*height;
	scan = data;
	samples = 3;

	if(lightMap) {
		if(r_greyscale->integer)
			internalFormat = GL_LUMINANCE;
		else
			internalFormat = GL_RGB;
	} else {
		for ( i = 0; i < c; i++ ) {
			if( scan[i*4 + 0] != scan[i*4 + 1] || scan[i*4 + 0] != scan[i*4 + 2] ) {
				hasColor = qtrue;
				if( samples == 4 )
					break;
			}
			if( scan[i*4 + 3] != 255 ) {
				samples = 4;
				if( hasColor )
					break;
			}
		}
		// select proper internal format
		if ( samples == 3 ) {
			*hasAlpha = qfalse;
			if( r_greyscale->integer || !hasColor ) {
				internalFormat = GL_LUMINANCE8;
			} else {
				if ( glConfig.textureCompression == TC_S3TC_ARB ) {
					internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				} else if ( glConfig.textureCompression == TC_S3TC ) {
					internalFormat = GL_RGB4_S3TC;
				} else if ( r_texturebits->integer == 16 ) {
					internalFormat = GL_RGB5;
				} else if ( r_texturebits->integer == 32 ) {
					internalFormat = GL_RGB8;
				} else {
					internalFormat = GL_RGB;
				}
			}
		} else if ( samples == 4 ) {
			*hasAlpha = qtrue;
			if( r_greyscale->integer || !hasColor ) {
				internalFormat = GL_LUMINANCE8_ALPHA8;
			} else {
				if ( r_texturebits->integer == 16 ) {
					internalFormat = GL_RGBA4;
				} else if ( r_texturebits->integer == 32 ) {
					internalFormat = GL_RGBA8;
				} else {
					internalFormat = GL_RGBA;
				}
			}
		}
	}

	w = width; h = height;

	// there may be base_level unused levels, but we need the
	// data for mipmapping
	for( i = 0; i <= max_level; i++ ) {
		if( i == 0 ) {
			// data pointer is already set up
		} else {
			// compute mipmaps inplace
			R_MipMap( data, w, h );

			if( w > 1 ) w >>= 1;
			if( h > 1 ) h >>= 1;
		}

		if( i >= base_level ) {
			R_LightScaleTexture (data, w, h, !mipmap );
			if ( r_colorMipLevels->integer ) {
				R_BlendOverTexture( data, w * h,
						    mipBlendColors[i - base_level] );
			}
			qglTexImage2D (GL_TEXTURE_2D, i - base_level,
				       internalFormat, w, h, 0,
				       GL_RGBA, GL_UNSIGNED_BYTE, data);
		}

		if( i == base_level ) {
			*pUploadWidth = w;
			*pUploadHeight = h;
			*format = internalFormat;
			*maxMipLevel = max_level - base_level;

			if( qglGenerateMipmap && mipmap &&
			    !r_colorMipLevels->integer ) {
				// use automatic mipmap generation
				qglGenerateMipmap( GL_TEXTURE_2D );
				break;
			}
		}
	}

	if (mipmap)
	{
		if ( glGlobals.textureFilterAnisotropic )
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
					(GLint)Com_Clamp( 1, glGlobals.maxAnisotropy, r_ext_max_anisotropy->integer ) );

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		if ( glGlobals.textureFilterAnisotropic )
			qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );

		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	}

	GL_CheckErrors();

	if ( resampledBuffer )
		ri.Hunk_FreeTempMemory( resampledBuffer );
}


/*
===============
UploadHeightMap

Upload a normal/height texture with all required mipmaps
===============
*/
static void UploadHeightMap( const byte *data,
			     int width, int height, 
			     qboolean picmip, 
			     int *format, 
			     int *pUploadWidth, int *pUploadHeight,
			     int *maxMipLevel )
{
	byte		*scaledBuffer = NULL;
	byte		*resampledBuffer = NULL;
	int		scaled_width, scaled_height;
	int		i, c;
	byte		*scan;
	GLenum		internalFormat = GL_RGB;
	int		miplevel;
	float		hMax = 0;
	qboolean	skip;

	//
	// convert to exact power of 2 sizes
	//
	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;
	if ( r_roundImagesDown->integer && scaled_width > width )
		scaled_width >>= 1;
	if ( r_roundImagesDown->integer && scaled_height > height )
		scaled_height >>= 1;

	if ( scaled_width != width || scaled_height != height ) {
		resampledBuffer = ri.Hunk_AllocateTempMemory( scaled_width * scaled_height * 4 );
		ResampleTexture (data, width, height, resampledBuffer, scaled_width, scaled_height);
		data = resampledBuffer;

		width = scaled_width;
		height = scaled_height;
	}

	//
	// perform optional picmip operation
	//
	if ( picmip ) {
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
	}

	//
	// clamp to minimum size
	//
	if (scaled_width < 1) {
		scaled_width = 1;
	}
	if (scaled_height < 1) {
		scaled_height = 1;
	}

	//
	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	while ( scaled_width > glConfig.maxTextureSize
		|| scaled_height > glConfig.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}
	// calculate maxMipLevel for this texture
	for( i = 1, *maxMipLevel = 0; i < scaled_width || i < scaled_height;
	     i <<= 1, (*maxMipLevel)++);

	scaledBuffer = ri.Hunk_AllocateTempMemory( sizeof( unsigned ) * scaled_width * scaled_height );

	// select proper internal format
	internalFormat = GL_RGBA8;
	skip = qfalse;

	// copy or resample data as appropriate for first MIP level
	// use the normal mip-mapping function to go down from here
	while ( width > scaled_width || height > scaled_height ) {
		R_MipMapHeightMap( (byte *)data, width, height );
		width >>= 1;
		height >>= 1;
		if ( width < 1 ) {
			width = 1;
		}
		if ( height < 1 ) {
			height = 1;
		}
	}
	Com_Memcpy( scaledBuffer, data, width * height * 4 );

	//
	// scan the texture for maximum height values and increase
	// height values so that the maximum is 255.
	//
	c = scaled_width * scaled_height;
	scan = ((byte *)scaledBuffer);

	for ( i = 0; i < c; i++ )
	{
		if ( scan[i*4+3] > hMax ) 
		{
			hMax = scan[i*4+3];
		}
	}
	if( hMax < 255 ) {
		for ( i = 0; i < c; i++ )
		{
			scan[i*4+3] += 255 - hMax;
		}
	}

	*pUploadWidth = scaled_width;
	*pUploadHeight = scaled_height;
	*format = internalFormat;

	for( miplevel = 0; miplevel <= *maxMipLevel; miplevel++ ) {
		qglTexImage2D (GL_TEXTURE_2D, miplevel, internalFormat, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaledBuffer );
		
		R_MipMapHeightMap( (byte *)scaledBuffer, scaled_width, scaled_height );
		scaled_width >>= 1;
		scaled_height >>= 1;
		if (scaled_width < 1)
			scaled_width = 1;
		if (scaled_height < 1)
			scaled_height = 1;
	}
	
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	qglTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	GL_CheckErrors();
	
	if ( scaledBuffer != 0 )
		ri.Hunk_FreeTempMemory( scaledBuffer );
	if ( resampledBuffer != 0 )
		ri.Hunk_FreeTempMemory( resampledBuffer );
}


/*
================
R_CreateImage

This is the only way any image_t are created
================
*/
image_t *R_CreateImage( const char *name, byte *pic, int width, int height, 
			qboolean mipmap, qboolean allowPicmip, int glWrapClampMode ) {
	image_t		*image;
	qboolean	isLightmap = qfalse;
	long		hash;
	GLenum		target = GL_TEXTURE_2D;

	if (strlen(name) >= MAX_QPATH ) {
		ri.Error (ERR_DROP, "R_CreateImage: \"%s\" is too long", name);
	}
	if ( !strncmp( name, "*lightmap", 9 ) ) {
		isLightmap = qtrue;
	}

	if ( tr.numImages == MAX_DRAWIMAGES ) {
		ri.Error( ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit");
	}

	image = tr.images[tr.numImages] = ri.Hunk_Alloc( sizeof( image_t ), h_low );
	qglGenTextures(1, &image->texnum);
	//image->texnum = 1024 + tr.numImages;
	tr.numImages++;

	if( !height ) {
		target = GL_TEXTURE_BUFFER_EXT;
		qglGenBuffersARB( 1, &image->buffer );
	}

	image->target = target;
	image->mipmap = mipmap;
	image->allowPicmip = allowPicmip;

	strcpy (image->imgName, name);

	image->width = width;
	image->height = height;
	image->wrapClampMode = glWrapClampMode;

	// lightmaps are always allocated on TMU 1
	if ( qglActiveTextureARB && isLightmap ) {
		image->TMU = 1;
	} else {
		image->TMU = 0;
	}

	if( target == GL_TEXTURE_2D ) {
		GL_BindTexture( image->texnum );

		if( pic ) {
			if( *name == '^' ) {
				// height map
				UploadHeightMap( pic,
						 image->width, image->height, 
						 allowPicmip,
						 &image->internalFormat,
						 &image->uploadWidth,
						 &image->uploadHeight,
						 &image->maxMipLevel
					);
				image->hasAlpha = qtrue;
			} else {
				Upload32( pic,
					  image->width, image->height, 
					  image->mipmap,
					  allowPicmip,
					  isLightmap,
					  &image->internalFormat,
					  &image->uploadWidth,
					  &image->uploadHeight,
					  &image->hasAlpha,
					  &image->maxMipLevel
					);
			}
		}

		qglTexParameterf( target, GL_TEXTURE_WRAP_S, glWrapClampMode );
		qglTexParameterf( target, GL_TEXTURE_WRAP_T, glWrapClampMode );
	}

	hash = generateHashValue(name);
	image->next = hashTable[hash];
	hashTable[hash] = image;

	return image;
}

//===================================================================

typedef struct
{
	char *ext;
	void (*ImageLoader)( const char *, unsigned char **, int *, int * );
} imageExtToLoaderMap_t;

// Note that the ordering indicates the order of preference used
// when there are multiple images of different formats available
static imageExtToLoaderMap_t imageLoaders[ ] =
{
	{ "tga",  R_LoadTGA },
	{ "jpg",  R_LoadJPG },
	{ "jpeg", R_LoadJPG },
	{ "png",  R_LoadPNG },
	{ "pcx",  R_LoadPCX },
	{ "bmp",  R_LoadBMP }
};

static int numImageLoaders = ARRAY_LEN( imageLoaders );

/*
=================
R_LoadImage

Loads any of the supported image types into a cannonical
32 bit format.
=================
*/
void R_LoadImage( const char *name, byte **pic, int *width, int *height )
{
	qboolean orgNameFailed = qfalse;
	int orgLoader = -1;
	int i;
	char localName[ MAX_QPATH ];
	const char *ext;
	char *altName;

	*pic = NULL;
	*width = 0;
	*height = 0;

	Q_strncpyz( localName, name, MAX_QPATH );

	ext = COM_GetExtension( localName );

	if( *ext )
	{
		// Look for the correct loader and use it
		for( i = 0; i < numImageLoaders; i++ )
		{
			if( !Q_stricmp( ext, imageLoaders[ i ].ext ) )
			{
				// Load
				imageLoaders[ i ].ImageLoader( localName, pic, width, height );
				break;
			}
		}

		// A loader was found
		if( i < numImageLoaders )
		{
			if( *pic == NULL )
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				orgNameFailed = qtrue;
				orgLoader = i;
				COM_StripExtension( name, localName, MAX_QPATH );
			}
			else
			{
				// Something loaded
				return;
			}
		}
	}

	// Try and find a suitable match using all
	// the image formats supported
	for( i = 0; i < numImageLoaders; i++ )
	{
		if (i == orgLoader)
			continue;

		altName = va( "%s.%s", localName, imageLoaders[ i ].ext );

		// Load
		imageLoaders[ i ].ImageLoader( altName, pic, width, height );

		if( *pic )
		{
			if( orgNameFailed )
			{
				ri.Printf( PRINT_DEVELOPER, "WARNING: %s not present, using %s instead\n",
						name, altName );
			}

			break;
		}
	}
}


/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
image_t	*R_FindImageFile( const char *name, qboolean mipmap, qboolean allowPicmip, int glWrapClampMode ) {
	image_t	*image;
	int		width, height;
	byte	*pic;
	long	hash;

	if (!name) {
		return NULL;
	}

	hash = generateHashValue(name);

	//
	// see if the image is already loaded
	//
	for (image=hashTable[hash]; image; image=image->next) {
		if ( !strcmp( name, image->imgName ) ) {
			// the white image can be used with any set of parms, but other mismatches are errors
			if ( strcmp( name, "*white" ) ) {
				if ( image->mipmap != mipmap ) {
					ri.Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed mipmap parm\n", name );
				}
				if ( image->allowPicmip != allowPicmip ) {
					ri.Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed allowPicmip parm\n", name );
				}
				if ( image->wrapClampMode != glWrapClampMode ) {
					ri.Printf( PRINT_ALL, "WARNING: reused image %s with mixed glWrapClampMode parm\n", name );
				}
			}
			return image;
		}
	}

	//
	// load the pic from disk
	//
	R_LoadImage( name, &pic, &width, &height );
	if ( pic == NULL ) {
		return NULL;
	}

	image = R_CreateImage( ( char * ) name, pic, width, height, mipmap, allowPicmip, glWrapClampMode );
	ri.Free( pic );
	return image;
}

/*
===============
R_FindHeightMapFile

Finds or loads the given height map file.
Returns NULL if it fails, not a default image.
==============
*/
image_t	*R_FindHeightMapFile( const char *name, qboolean mipmap, int glWrapClampMode ) {
	image_t	*image;
	int		width, height;
	byte	*pic;
	long	hash;
	char localName[ MAX_QPATH ];

	if (!name) {
		return NULL;
	}

	localName[0] = '^';
	strcpy( localName+1, name );

	hash = generateHashValue( localName );

	//
	// see if the image is already loaded
	//
	for (image=hashTable[hash]; image; image=image->next) {
		if ( !strcmp( name, image->imgName ) ) {
			// the white image can be used with any set of parms, but other mismatches are errors
			if ( strcmp( name, "*white" ) ) {
				if ( image->mipmap != mipmap ) {
					ri.Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed mipmap parm\n", name );
				}
				if ( image->allowPicmip ) {
					ri.Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed allowPicmip parm\n", name );
				}
				if ( image->wrapClampMode != glWrapClampMode ) {
					ri.Printf( PRINT_ALL, "WARNING: reused image %s with mixed glWrapClampMode parm\n", name );
				}
			}
			return image;
		}
	}

	//
	// load the pic from disk
	//
	R_LoadImage( localName+1, &pic, &width, &height );
	if ( pic == NULL ) {
		return NULL;
	}

	image = R_CreateImage( localName, pic, width, height, mipmap, qfalse, glWrapClampMode );
	ri.Free( pic );
	return image;
}


image_t *R_CombineImages( int num, image_t **images ) {
	int	i, cols, width, height, lod, maxLod, xoffs;
	byte	*data;
	image_t	*result;
	
	if( num <= 1 )
		return NULL;
	
	/* check that all images are compatible */
	for( i = 1; i < num; i++ ) {
		if( images[i]->uploadWidth    != images[0]->uploadWidth ||
		    images[i]->uploadHeight   != images[0]->uploadHeight ||
		    images[i]->internalFormat != images[0]->internalFormat )
			return NULL;
	}
	
	width = images[0]->uploadWidth;
	height = images[0]->uploadHeight;
	
	/* Check that they fit into one texture */
	for( cols = 1; cols < num; cols *= 2 ) {
		if( cols * width >= glConfig.maxTextureSize )
			break;
	}
	
	/* TODO: avoid the GPU->CPU->GPU roundtrip with some render-to-texture
	 *       magic */
	data = ri.Hunk_AllocateTempMemory( width * height < 16 ?
					   16 * sizeof(color4ub_t) :
					   width * height * sizeof(color4ub_t) );

	result = R_CreateImage( "*combined", NULL, cols * width,
				height, images[0]->mipmap,
				images[0]->allowPicmip, GL_REPEAT );
	result->uploadWidth = cols * width;
	result->uploadHeight = height;
	result->internalFormat = images[0]->internalFormat;

	if( !images[0]->mipmap ) {
		maxLod = 0;
	} else {
		for( maxLod = 0; (1<<maxLod) < width &&
			     (1<<maxLod) < height; maxLod++ );
	}
	for( lod = 0; lod <= maxLod; lod++ ) {
		qglTexImage2D( GL_TEXTURE_2D, lod, result->internalFormat,
			       cols * width, height,
			       0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );
		xoffs = 0;
		if( width < 4 )
			qglPixelStorei( GL_PACK_ROW_LENGTH, 4 );
		for( i = 0; i < num; i++ ) {
			GL_BindTexture( images[i]->texnum );
			qglGetTexImage( GL_TEXTURE_2D, lod, GL_RGBA, GL_UNSIGNED_BYTE,
					data + (xoffs & 3) * sizeof(color4ub_t) );
			if( ((xoffs + width) & 3) == 0 ) {
				GL_BindTexture( result->texnum );
				qglTexSubImage2D( GL_TEXTURE_2D, lod, 
						  xoffs & -4, 0,
						  width < 4 ? 4 : width, height,
						  GL_RGBA, GL_UNSIGNED_BYTE, data );
				GL_CheckErrors();
			}
			
			xoffs += width;
		}
		for( ; i < cols; i++ ) {
			if( ((xoffs + width) & 3) == 0 ) {
				GL_BindTexture( result->texnum );
				qglTexSubImage2D( GL_TEXTURE_2D, lod, 
						  xoffs & -4, 0,
						  width < 4 ? 4 : width, height,
						  GL_RGBA, GL_UNSIGNED_BYTE, data );
				GL_CheckErrors();
				xoffs += width;
				break;
			}
			
			xoffs += width;
		}
		if( xoffs & 3 ) {
			GL_BindTexture( result->texnum );
			qglTexSubImage2D( GL_TEXTURE_2D, lod, 
					  xoffs & -4, 0,
					  xoffs & 3, height,
					  GL_RGBA, GL_UNSIGNED_BYTE, data );
			GL_CheckErrors();
		}
		if( width < 4 )
			qglPixelStorei( GL_PACK_ROW_LENGTH, 0 );
		width >>= 1; height >>= 1;
	}
	qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxLod );
	
	ri.Hunk_FreeTempMemory( data );
	
	return result;
}

/*
================
R_CreateDlightImage
================
*/
#define	DLIGHT_SIZE	16
static void R_CreateDlightImage( void ) {
	int		x,y;
	byte	data[DLIGHT_SIZE][DLIGHT_SIZE][4];
	int		b;

	// make a centered inverse-square falloff blob for dynamic lighting
	for (x=0 ; x<DLIGHT_SIZE ; x++) {
		for (y=0 ; y<DLIGHT_SIZE ; y++) {
			float	d;

			d = ( DLIGHT_SIZE/2 - 0.5f - x ) * ( DLIGHT_SIZE/2 - 0.5f - x ) +
				( DLIGHT_SIZE/2 - 0.5f - y ) * ( DLIGHT_SIZE/2 - 0.5f - y );
			b = 4000 / d;
			if (b > 255) {
				b = 255;
			} else if ( b < 75 ) {
				b = 0;
			}
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = b;
			data[y][x][3] = 255;			
		}
	}
	tr.dlightImage = R_CreateImage("*dlight", (byte *)data, DLIGHT_SIZE, DLIGHT_SIZE, qfalse, qfalse, GL_CLAMP_TO_EDGE );
}


/*
=================
R_InitFogTable
=================
*/
void R_InitFogTable( void ) {
	int		i;
	float	d;
	float	exp;
	
	exp = 0.5;

	for ( i = 0 ; i < FOG_TABLE_SIZE ; i++ ) {
		d = pow ( (float)i/(FOG_TABLE_SIZE-1), exp );

		tr.fogTable[i] = d;
	}
}

/*
================
R_FogFactor

Returns a 0.0 to 1.0 fog density value
This is called for each texel of the fog texture on startup
and for each vertex of transparent shaders in fog dynamically
================
*/
float	R_FogFactor( float s, float t ) {
	float	d;

	s -= 1.0/512;
	if ( s < 0 ) {
		return 0;
	}
	if ( t < 1.0/32 ) {
		return 0;
	}
	if ( t < 31.0/32 ) {
		s *= (t - 1.0f/32.0f) / (30.0f/32.0f);
	}

	// we need to leave a lot of clamp range
	s *= 8;

	if ( s > 1.0 ) {
		s = 1.0;
	}

	d = tr.fogTable[ (int)(s * (FOG_TABLE_SIZE-1)) ];

	return d;
}

/*
================
R_CreateFogImage
================
*/
#define	FOG_S	256
#define	FOG_T	32
static void R_CreateFogImage( void ) {
	int		x,y;
	byte	*data;
	float	d;
	float	borderColor[4];

	data = ri.Hunk_AllocateTempMemory( FOG_S * FOG_T * 4 );

	// S is distance, T is depth
	for (x=0 ; x<FOG_S ; x++) {
		for (y=0 ; y<FOG_T ; y++) {
			d = R_FogFactor( ( x + 0.5f ) / FOG_S, ( y + 0.5f ) / FOG_T );

			data[(y*FOG_S+x)*4+0] = 
			data[(y*FOG_S+x)*4+1] = 
			data[(y*FOG_S+x)*4+2] = 255;
			data[(y*FOG_S+x)*4+3] = 255*d;
		}
	}
	// standard openGL clamping doesn't really do what we want -- it includes
	// the border color at the edges.  OpenGL 1.2 has clamp-to-edge, which does
	// what we want.
	tr.fogImage = R_CreateImage("*fog", (byte *)data, FOG_S, FOG_T, qfalse, qfalse, GL_CLAMP_TO_EDGE );
	ri.Hunk_FreeTempMemory( data );

	borderColor[0] = 1.0;
	borderColor[1] = 1.0;
	borderColor[2] = 1.0;
	borderColor[3] = 1;

	qglTexParameterfv( GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor );
}

/*
==================
R_CreateDefaultImage
==================
*/
#define	DEFAULT_SIZE	16
static void R_CreateDefaultImage( void ) {
	int		x;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// the default image will be a box, to allow you to see the mapping coordinates
	Com_Memset( data, 32, sizeof( data ) );
	for ( x = 0 ; x < DEFAULT_SIZE ; x++ ) {
		data[0][x][0] =
		data[0][x][1] =
		data[0][x][2] =
		data[0][x][3] = 255;

		data[x][0][0] =
		data[x][0][1] =
		data[x][0][2] =
		data[x][0][3] = 255;

		data[DEFAULT_SIZE-1][x][0] =
		data[DEFAULT_SIZE-1][x][1] =
		data[DEFAULT_SIZE-1][x][2] =
		data[DEFAULT_SIZE-1][x][3] = 255;

		data[x][DEFAULT_SIZE-1][0] =
		data[x][DEFAULT_SIZE-1][1] =
		data[x][DEFAULT_SIZE-1][2] =
		data[x][DEFAULT_SIZE-1][3] = 255;
	}
	tr.defaultImage = R_CreateImage("*default", (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, qtrue, qfalse, GL_REPEAT );
}

/*
==================
R_CreateBuiltinImages
==================
*/
void R_CreateBuiltinImages( void ) {
	int		x,y;
	byte	data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	R_CreateDefaultImage();

	// we use a solid white image instead of disabling texturing
	Com_Memset( data, 255, sizeof( data ) );
	tr.whiteImage = R_CreateImage("*white", (byte *)data, 8, 8, qfalse, qfalse, GL_REPEAT );

	// with overbright bits active, we need an image which is some fraction of full color,
	// for default lightmaps, etc
	for (x=0 ; x<DEFAULT_SIZE ; x++) {
		for (y=0 ; y<DEFAULT_SIZE ; y++) {
			data[y][x][0] = 
			data[y][x][1] = 
			data[y][x][2] = tr.identityLightByte;
			data[y][x][3] = 255;			
		}
	}

	tr.identityLightImage = R_CreateImage("*identityLight", (byte *)data, 8, 8, qfalse, qfalse, GL_REPEAT );


	for(x=0;x<32;x++) {
		// scratchimage is usually used for cinematic drawing
		tr.scratchImage[x] = R_CreateImage("*scratch", (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, qfalse, qtrue, GL_CLAMP_TO_EDGE );
	}

	R_CreateDlightImage();
	R_CreateFogImage();

	if( r_depthPass->integer && qglCreateShader ) {
		tr.depthImage = R_CreateImage("*depth", NULL,
					      glConfig.vidWidth,
					      glConfig.vidHeight,
					      qfalse, qfalse,
					      GL_CLAMP_TO_EDGE );
		qglTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
			       glConfig.vidWidth, glConfig.vidHeight,
			       0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	} else {
		tr.depthImage = NULL;
	}
}


/*
===============
R_SetColorMappings
===============
*/
void R_SetColorMappings( void ) {
	int		i, j;
	float	g;
	int		inf;
	int		shift;

	// setup the overbright lighting
	tr.overbrightBits = r_overBrightBits->integer;
	if ( !glConfig.deviceSupportsGamma ) {
		tr.overbrightBits = 0;		// need hardware gamma for overbright
	}

	// never overbright in windowed mode
	if ( !glConfig.isFullscreen ) 
	{
		tr.overbrightBits = 0;
	}

	// allow 2 overbright bits in 24 bit, but only 1 in 16 bit
	if ( glConfig.colorBits > 16 ) {
		if ( tr.overbrightBits > 2 ) {
			tr.overbrightBits = 2;
		}
	} else {
		if ( tr.overbrightBits > 1 ) {
			tr.overbrightBits = 1;
		}
	}
	if ( tr.overbrightBits < 0 ) {
		tr.overbrightBits = 0;
	}

	tr.identityLight = 1.0f / ( 1 << tr.overbrightBits );
	tr.identityLightByte = 255 * tr.identityLight;


	if ( r_intensity->value <= 1 ) {
		ri.Cvar_Set( "r_intensity", "1" );
	}

	if ( r_gamma->value < 0.5f ) {
		ri.Cvar_Set( "r_gamma", "0.5" );
	} else if ( r_gamma->value > 3.0f ) {
		ri.Cvar_Set( "r_gamma", "3.0" );
	}

	g = r_gamma->value;

	shift = tr.overbrightBits;

	for ( i = 0; i < 256; i++ ) {
		if ( g == 1 ) {
			inf = i;
		} else {
			inf = 255 * pow ( i/255.0f, 1.0f / g ) + 0.5f;
		}
		inf <<= shift;
		if (inf < 0) {
			inf = 0;
		}
		if (inf > 255) {
			inf = 255;
		}
		s_gammatable[i] = inf;
	}

	for (i=0 ; i<256 ; i++) {
		j = i * r_intensity->value;
		if (j > 255) {
			j = 255;
		}
		s_intensitytable[i] = j;
	}

	if ( glConfig.deviceSupportsGamma )
	{
		GLimp_SetGamma( s_gammatable, s_gammatable, s_gammatable );
	}
}

/*
===============
R_InitImages
===============
*/
void	R_InitImages( void ) {
	Com_Memset(hashTable, 0, sizeof(hashTable));
	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();
}

/*
===============
R_DeleteTextures
===============
*/
void R_DeleteTextures( void ) {
	int		i;

	for ( i=0; i<tr.numImages ; i++ ) {
		qglDeleteTextures( 1, &tr.images[i]->texnum );
		if( tr.images[i]->target == GL_TEXTURE_BUFFER_EXT )
			qglDeleteBuffersARB( 1, &tr.images[i]->buffer );
	}
	Com_Memset( tr.images, 0, sizeof( tr.images ) );

	tr.numImages = 0;

	Com_Memset( glState.currenttextures, 0, sizeof( glState.currenttextures ) );
	GL_UnbindAllTextures( );
}

/*
============================================================================

SKINS

============================================================================
*/

/*
==================
CommaParse

This is unfortunate, but the skin files aren't
compatable with our normal parsing rules.
==================
*/
static char *CommaParse( char **data_p ) {
	int c = 0, len;
	char *data;
	static	char	com_token[MAX_TOKEN_CHARS];

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	// make sure incoming data is valid
	if ( !data ) {
		*data_p = NULL;
		return com_token;
	}

	while ( 1 ) {
		// skip whitespace
		while( (c = *data) <= ' ') {
			if( !c ) {
				break;
			}
			data++;
		}


		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' )
		{
			while (*data && *data != '\n')
				data++;
		}
		// skip /* */ comments
		else if ( c=='/' && data[1] == '*' ) 
		{
			while ( *data && ( *data != '*' || data[1] != '/' ) ) 
			{
				data++;
			}
			if ( *data ) 
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	if ( c == 0 ) {
		return "";
	}

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data++;
			if (c=='\"' || !c)
			{
				com_token[len] = 0;
				*data_p = ( char * ) data;
				return com_token;
			}
			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c>32 && c != ',' );

	if (len == MAX_TOKEN_CHARS)
	{
//		ri.Printf (PRINT_DEVELOPER, "Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = ( char * ) data;
	return com_token;
}


/*
===============
RE_RegisterSkin

===============
*/
qhandle_t RE_RegisterSkin( const char *name ) {
	qhandle_t	hSkin;
	skin_t		*skin;
	skinSurface_t	*surf;
	union {
		char *c;
		void *v;
	} text;
	char		*text_p;
	char		*token;
	char		surfName[MAX_QPATH];

	if ( !name || !name[0] ) {
		ri.Printf( PRINT_DEVELOPER, "Empty name passed to RE_RegisterSkin\n" );
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		ri.Printf( PRINT_DEVELOPER, "Skin name exceeds MAX_QPATH\n" );
		return 0;
	}


	// see if the skin is already loaded
	for ( hSkin = 1; hSkin < tr.numSkins ; hSkin++ ) {
		skin = tr.skins[hSkin];
		if ( !Q_stricmp( skin->name, name ) ) {
			if( skin->numSurfaces == 0 ) {
				return 0;		// default skin
			}
			return hSkin;
		}
	}

	// allocate a new skin
	if ( tr.numSkins == MAX_SKINS ) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name );
		return 0;
	}
	tr.numSkins++;
	skin = ri.Hunk_Alloc( sizeof( skin_t ), h_low );
	tr.skins[hSkin] = skin;
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );
	skin->numSurfaces = 0;

	// make sure the render thread is stopped
	R_SyncRenderThread();

	// If not a .skin file, load as a single shader
	if ( strcmp( name + strlen( name ) - 5, ".skin" ) ) {
		skin->numSurfaces = 1;
		skin->surfaces[0] = ri.Hunk_Alloc( sizeof(skin->surfaces[0]), h_low );
		skin->surfaces[0]->shader = R_FindShader( name, tr.defaultMD3Shader->lightmapIndex, qtrue );
		return hSkin;
	}

	// load and parse the skin file
	ri.FS_ReadFile( name, &text.v );
	if ( !text.c ) {
		return 0;
	}

	text_p = text.c;
	while ( text_p && *text_p ) {
		// get surface name
		token = CommaParse( &text_p );
		Q_strncpyz( surfName, token, sizeof( surfName ) );

		if ( !token[0] ) {
			break;
		}
		// lowercase the surface name so skin compares are faster
		Q_strlwr( surfName );

		if ( *text_p == ',' ) {
			text_p++;
		}

		if ( strstr( token, "tag_" ) ) {
			continue;
		}
		
		// parse the shader name
		token = CommaParse( &text_p );

		surf = skin->surfaces[ skin->numSurfaces ] = ri.Hunk_Alloc( sizeof( *skin->surfaces[0] ), h_low );
		Q_strncpyz( surf->name, surfName, sizeof( surf->name ) );
		surf->shader = R_FindShader( token, tr.defaultMD3Shader->lightmapIndex, qtrue );
		skin->numSurfaces++;
	}

	ri.FS_FreeFile( text.v );


	// never let a skin have 0 shaders
	if ( skin->numSurfaces == 0 ) {
		return 0;		// use default skin
	}

	return hSkin;
}


/*
===============
R_InitSkins
===============
*/
void	R_InitSkins( void ) {
	skin_t		*skin;

	tr.numSkins = 1;

	// make the default skin have all default shaders
	skin = tr.skins[0] = ri.Hunk_Alloc( sizeof( skin_t ), h_low );
	Q_strncpyz( skin->name, "<default skin>", sizeof( skin->name )  );
	skin->numSurfaces = 1;
	skin->surfaces[0] = ri.Hunk_Alloc( sizeof( skinSurface_t ), h_low );
	skin->surfaces[0]->shader = tr.defaultShader;
}

/*
===============
R_GetSkinByHandle
===============
*/
skin_t	*R_GetSkinByHandle( qhandle_t hSkin ) {
	if ( hSkin < 1 || hSkin >= tr.numSkins ) {
		return tr.skins[0];
	}
	return tr.skins[ hSkin ];
}

/*
===============
R_SkinList_f
===============
*/
void	R_SkinList_f( void ) {
	int			i, j;
	skin_t		*skin;

	ri.Printf (PRINT_ALL, "------------------\n");

	for ( i = 0 ; i < tr.numSkins ; i++ ) {
		skin = tr.skins[i];

		ri.Printf( PRINT_ALL, "%3i:%s\n", i, skin->name );
		for ( j = 0 ; j < skin->numSurfaces ; j++ ) {
			ri.Printf( PRINT_ALL, "       %s = %s\n", 
				skin->surfaces[j]->name, skin->surfaces[j]->shader->name );
		}
	}
	ri.Printf (PRINT_ALL, "------------------\n");
}

