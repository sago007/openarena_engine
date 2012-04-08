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
/*
** QGL.H
*/

#ifndef __QGL_EXTRA_H__
#define __QGL_EXTRA_H__

#ifdef USE_LOCAL_HEADERS
#	include "SDL_opengl.h"
#else
#	include <SDL_opengl.h>
#endif

// MinGW headers may be missing some GL extensions
#ifndef GL_ARB_map_buffer_range
#define GL_ARB_map_buffer_range
#define GL_MAP_READ_BIT				0x0001
#define GL_MAP_WRITE_BIT			0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT		0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT		0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT		0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT		0x0020
#endif

#ifndef GL_EXT_timer_query
#define GL_EXT_timer_query
#define GL_TIME_ELAPSED_EXT 0x88BF
typedef int64_t GLint64EXT;
typedef uint64_t GLuint64EXT;
#endif

#ifndef GL_EXT_geometry_shader4
#define GL_EXT_geometry_shader4
#define GL_GEOMETRY_SHADER_EXT 0x8DD9
#define GL_GEOMETRY_VERTICES_OUT_EXT 0x8DDA
#define GL_GEOMETRY_INPUT_TYPE_EXT 0x8DDB
#define GL_GEOMETRY_OUTPUT_TYPE_EXT 0x8DDC
#endif

#ifndef GL_EXT_texture_buffer_object
#define GL_EXT_texture_buffer_object
#define GL_TEXTURE_BUFFER_EXT				0x8C2A
#define GL_MAX_TEXTURE_BUFFER_SIZE_EXT			0x8C2B
#define GL_TEXTURE_BINDING_BUFFER_EXT			0x8C2C
#define GL_TEXTURE_BUFFER_DATA_STORE_BINDING_EXT	0x8C2D
#define GL_TEXTURE_BUFFER_FORMAT_EXT			0x8C2E
#endif

#ifndef GL_ARB_debug_output
#define GL_ARB_debug_output
typedef void (APIENTRY *GLDEBUGPROCARB)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,GLvoid *userParam);
#define GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB 0x8242
#define GL_DEBUG_LOGGED_MESSAGES_ARB 0x9145
#define GL_DEBUG_SEVERITY_HIGH_ARB 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM_ARB 0x9147
#define GL_DEBUG_SEVERITY_LOW_ARB 0x9148
#define GL_DEBUG_SOURCE_APPLICATION_ARB 0x824A
#define GL_DEBUG_TYPE_OTHER_ARB 0x8251
#endif

#ifndef GL_AMD_debug_output
#define GL_AMD_debug_output
typedef void (APIENTRYP *GLDEBUGPROCAMD)(GLuint id,GLenum category,GLenum severity,GLsizei length,const GLchar *message,GLvoid *userParam);
#define GL_DEBUG_LOGGED_MESSAGES_AMD 0x9145
#define GL_DEBUG_SEVERITY_HIGH_AMD 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM_AMD 0x9147
#define GL_DEBUG_SEVERITY_LOW_AMD 0x9148
#define GL_DEBUG_CATEGORY_APPLICATION_AMD 0x914f
#endif

#ifndef WGL_ARB_create_context_profile
#define WGL_ARB_create_context_profile
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x0001
#endif

#ifndef GL_ARB_uniform_buffer_object
#define GL_ARB_uniform_buffer_object
#define GL_UNIFORM_BUFFER				0x8A11
#define GL_UNIFORM_BUFFER_BINDING			0x8A28
#define GL_UNIFORM_BUFFER_START				0x8A29
#define GL_UNIFORM_BUFFER_SIZE				0x8A2A
#define GL_MAX_VERTEX_UNIFORM_BLOCKS			0x8A2B
#define GL_MAX_GEOMETRY_UNIFORM_BLOCKS			0x8A2C
#define GL_MAX_FRAGMENT_UNIFORM_BLOCKS			0x8A2D
#define GL_MAX_COMBINED_UNIFORM_BLOCKS			0x8A2E
#define GL_MAX_UNIFORM_BUFFER_BINDINGS			0x8A2F
#define GL_MAX_UNIFORM_BLOCK_SIZE			0x8A30
#define GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS	0x8A31
#define GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS	0x8A32
#define GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS	0x8A33
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT		0x8A34
#define GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH		0x8A35
#define GL_ACTIVE_UNIFORM_BLOCKS			0x8A36
#define GL_UNIFORM_TYPE					0x8A37
#define GL_UNIFORM_SIZE					0x8A38
#define GL_UNIFORM_NAME_LENGTH				0x8A39
#define GL_UNIFORM_BLOCK_INDEX				0x8A3A
#define GL_UNIFORM_OFFSET				0x8A3B
#define GL_UNIFORM_ARRAY_STRIDE				0x8A3C
#define GL_UNIFORM_MATRIX_STRIDE			0x8A3D
#define GL_UNIFORM_IS_ROW_MAJOR				0x8A3E
#define GL_UNIFORM_BLOCK_BINDING			0x8A3F
#define GL_UNIFORM_BLOCK_DATA_SIZE			0x8A40
#define GL_UNIFORM_BLOCK_NAME_LENGTH			0x8A41
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS		0x8A42
#define GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES		0x8A43
#define GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER	0x8A44
#define GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER	0x8A45
#define GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER	0x8A46
#define GL_INVALID_INDEX				0xFFFFFFFFu
#endif

#ifndef GL_ARB_blend_func_extended
#define GL_ARB_blend_func_extended
#define GL_SRC1_COLOR					0x88F9
#define GL_ONE_MINUS_SRC1_COLOR				0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA				0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS			0x88FC
#endif

// GL_EXT_draw_range_elements
extern void (APIENTRYP qglDrawRangeElementsEXT) (GLenum mode, GLsizei count, GLuint start, GLuint end, GLenum type, const GLvoid *indices);

extern void (APIENTRYP qglMultiTexCoord4fvARB) (GLenum target, GLfloat *v);

// GL_ARB_vertex_buffer_object
extern void (APIENTRYP qglBindBufferARB) (GLenum target, GLuint buffer);
extern void (APIENTRYP qglDeleteBuffersARB) (GLsizei n, const GLuint *buffers);
extern void (APIENTRYP qglGenBuffersARB) (GLsizei n, GLuint *buffers);
extern GLboolean (APIENTRYP qglIsBufferARB) (GLuint buffer);
extern void (APIENTRYP qglBufferDataARB) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
extern void (APIENTRYP qglBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, const GLvoid *data);
extern void (APIENTRYP qglGetBufferSubDataARB) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, GLvoid *data);
extern GLvoid *(APIENTRYP qglMapBufferARB) (GLenum target, GLenum access);
extern GLboolean (APIENTRYP qglUnmapBufferARB) (GLenum target);
extern void (APIENTRYP qglGetBufferParameterivARB) (GLenum target, GLenum pname, GLint *params);
extern void (APIENTRYP qglGetBufferPointervARB) (GLenum target, GLenum pname, GLvoid **params);

// GL_ARB_map_buffer_range
extern GLvoid *(APIENTRYP qglMapBufferRange) (GLenum target, GLintptr offset,
					      GLsizeiptr length,
					      GLbitfield access);
extern GLvoid (APIENTRYP qglFlushMappedBufferRange) (GLenum target,
						     GLintptr offset,
						     GLsizeiptr length);

// GL_ARB_shader_objects
extern GLvoid (APIENTRYP qglDeleteShader) (GLuint shader);
extern GLvoid (APIENTRYP qglDeleteProgram) (GLuint program);
extern GLvoid (APIENTRYP qglDetachShader) (GLuint program, GLuint shader);
extern GLuint (APIENTRYP qglCreateShader) (GLenum type);
extern GLvoid (APIENTRYP qglShaderSource) (GLuint shader, GLsizei count, const char **string,
					   const GLint *length);
extern GLvoid (APIENTRYP qglCompileShader) (GLuint shader);
extern GLuint (APIENTRYP qglCreateProgram) (void);
extern GLvoid (APIENTRYP qglAttachShader) (GLuint program, GLuint shader);
extern GLvoid (APIENTRYP qglLinkProgram) (GLuint program);
extern GLvoid (APIENTRYP qglUseProgram) (GLuint program);
extern GLvoid (APIENTRYP qglValidateProgram) (GLuint program);
extern GLvoid (APIENTRYP qglUniform1f) (GLint location, GLfloat v0);
extern GLvoid (APIENTRYP qglUniform2f) (GLint location, GLfloat v0, GLfloat v1);
extern GLvoid (APIENTRYP qglUniform3f) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
extern GLvoid (APIENTRYP qglUniform4f) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern GLvoid (APIENTRYP qglUniform1i) (GLint location, GLint v0);
extern GLvoid (APIENTRYP qglUniform2i) (GLint location, GLint v0, GLint v1);
extern GLvoid (APIENTRYP qglUniform3i) (GLint location, GLint v0, GLint v1, GLint v2);
extern GLvoid (APIENTRYP qglUniform4i) (GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
extern GLvoid (APIENTRYP qglUniform1fv) (GLint location, GLsizei count, const GLfloat *value);
extern GLvoid (APIENTRYP qglUniform2fv) (GLint location, GLsizei count, const GLfloat *value);
extern GLvoid (APIENTRYP qglUniform3fv) (GLint location, GLsizei count, const GLfloat *value);
extern GLvoid (APIENTRYP qglUniform4fv) (GLint location, GLsizei count, const GLfloat *value);
extern GLvoid (APIENTRYP qglUniform1iv) (GLint location, GLsizei count, const GLint *value);
extern GLvoid (APIENTRYP qglUniform2iv) (GLint location, GLsizei count, const GLint *value);
extern GLvoid (APIENTRYP qglUniform3iv) (GLint location, GLsizei count, const GLint *value);
extern GLvoid (APIENTRYP qglUniform4iv) (GLint location, GLsizei count, const GLint *value);
extern GLvoid (APIENTRYP qglUniformMatrix2fv) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern GLvoid (APIENTRYP qglUniformMatrix3fv) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern GLvoid (APIENTRYP qglUniformMatrix4fv) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
extern GLvoid (APIENTRYP qglGetShaderiv) (GLuint shader, GLenum pname, GLint *params);
extern GLvoid (APIENTRYP qglGetProgramiv) (GLuint program, GLenum pname, GLint *params);
extern GLvoid (APIENTRYP qglGetShaderInfoLog) (GLuint shader, GLsizei maxLength, GLsizei *length, char *infoLog);
extern GLvoid (APIENTRYP qglGetProgramInfoLog) (GLuint program, GLsizei maxLength, GLsizei *length, char *infoLog);
extern GLvoid (APIENTRYP qglGetAttachedShaders) (GLuint program, GLsizei maxCount, GLsizei *count,
						 GLuint *shaders);
extern GLint (APIENTRYP qglGetUniformLocation) (GLuint program, const char *name);
extern GLvoid (APIENTRYP qglGetActiveUniform) (GLuint program, GLuint index, GLsizei maxLength,
					       GLsizei *length, GLint *size, GLenum *type, char *name);
extern GLvoid (APIENTRYP qglGetUniformfv) (GLuint program, GLint location, GLfloat *params);
extern GLvoid (APIENTRYP qglGetUniformiv) (GLuint program, GLint location, GLint *params);
extern GLvoid (APIENTRYP qglGetShaderSource) (GLuint shader, GLsizei maxLength, GLsizei *length,
					      char *source);

// GL_ARB_vertex_shader
extern GLvoid (APIENTRYP qglVertexAttrib1fARB) (GLuint index, GLfloat v0);
extern GLvoid (APIENTRYP qglVertexAttrib1sARB) (GLuint index, GLshort v0);
extern GLvoid (APIENTRYP qglVertexAttrib1dARB) (GLuint index, GLdouble v0);
extern GLvoid (APIENTRYP qglVertexAttrib2fARB) (GLuint index, GLfloat v0, GLfloat v1);
extern GLvoid (APIENTRYP qglVertexAttrib2sARB) (GLuint index, GLshort v0, GLshort v1);
extern GLvoid (APIENTRYP qglVertexAttrib2dARB) (GLuint index, GLdouble v0, GLdouble v1);
extern GLvoid (APIENTRYP qglVertexAttrib3fARB) (GLuint index, GLfloat v0, GLfloat v1, GLfloat v2);
extern GLvoid (APIENTRYP qglVertexAttrib3sARB) (GLuint index, GLshort v0, GLshort v1, GLshort v2);
extern GLvoid (APIENTRYP qglVertexAttrib3dARB) (GLuint index, GLdouble v0, GLdouble v1, GLdouble v2);
extern GLvoid (APIENTRYP qglVertexAttrib4fARB) (GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
extern GLvoid (APIENTRYP qglVertexAttrib4sARB) (GLuint index, GLshort v0, GLshort v1, GLshort v2, GLshort v3);
extern GLvoid (APIENTRYP qglVertexAttrib4dARB) (GLuint index, GLdouble v0, GLdouble v1, GLdouble v2, GLdouble v3);
extern GLvoid (APIENTRYP qglVertexAttrib4NubARB) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
extern GLvoid (APIENTRYP qglVertexAttrib1fvARB) (GLuint index, GLfloat *v);
extern GLvoid (APIENTRYP qglVertexAttrib1svARB) (GLuint index, GLshort *v);
extern GLvoid (APIENTRYP qglVertexAttrib1dvARB) (GLuint index, GLdouble *v);
extern GLvoid (APIENTRYP qglVertexAttrib2fvARB) (GLuint index, GLfloat *v);
extern GLvoid (APIENTRYP qglVertexAttrib2svARB) (GLuint index, GLshort *v);
extern GLvoid (APIENTRYP qglVertexAttrib2dvARB) (GLuint index, GLdouble *v);
extern GLvoid (APIENTRYP qglVertexAttrib3fvARB) (GLuint index, GLfloat *v);
extern GLvoid (APIENTRYP qglVertexAttrib3svARB) (GLuint index, GLshort *v);
extern GLvoid (APIENTRYP qglVertexAttrib3dvARB) (GLuint index, GLdouble *v);
extern GLvoid (APIENTRYP qglVertexAttrib4fvARB) (GLuint index, GLfloat *v);
extern GLvoid (APIENTRYP qglVertexAttrib4svARB) (GLuint index, GLshort *v);
extern GLvoid (APIENTRYP qglVertexAttrib4dvARB) (GLuint index, GLdouble *v);
extern GLvoid (APIENTRYP qglVertexAttrib4ivARB) (GLuint index, GLint *v);
extern GLvoid (APIENTRYP qglVertexAttrib4bvARB) (GLuint index, GLbyte *v);
extern GLvoid (APIENTRYP qglVertexAttrib4ubvARB) (GLuint index, GLubyte *v);
extern GLvoid (APIENTRYP qglVertexAttrib4usvARB) (GLuint index, GLushort *v);
extern GLvoid (APIENTRYP qglVertexAttrib4uivARB) (GLuint index, GLuint *v);
extern GLvoid (APIENTRYP qglVertexAttrib4NbvARB) (GLuint index, const GLbyte *v);
extern GLvoid (APIENTRYP qglVertexAttrib4NsvARB) (GLuint index, const GLshort *v);
extern GLvoid (APIENTRYP qglVertexAttrib4NivARB) (GLuint index, const GLint *v);
extern GLvoid (APIENTRYP qglVertexAttrib4NubvARB) (GLuint index, const GLubyte *v);
extern GLvoid (APIENTRYP qglVertexAttrib4NusvARB) (GLuint index, const GLushort *v);
extern GLvoid (APIENTRYP qglVertexAttrib4NuivARB) (GLuint index, const GLuint *v);
extern GLvoid (APIENTRYP qglVertexAttribPointerARB) (GLuint index, GLint size, GLenum type, GLboolean normalized,
						     GLsizei stride, const GLvoid *pointer);
extern GLvoid (APIENTRYP qglEnableVertexAttribArrayARB) (GLuint index);
extern GLvoid (APIENTRYP qglDisableVertexAttribArrayARB) (GLuint index);
extern GLvoid (APIENTRYP qglBindAttribLocationARB) (GLhandleARB programObj, GLuint index, const GLcharARB *name);
extern GLvoid (APIENTRYP qglGetActiveAttribARB) (GLhandleARB programObj, GLuint index, GLsizei maxLength,
						 GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
extern GLint (APIENTRYP qglGetAttribLocationARB) (GLhandleARB programObj, const GLcharARB *name);
extern GLvoid (APIENTRYP qglGetVertexAttribdvARB) (GLuint index, GLenum pname, GLdouble *params);
extern GLvoid (APIENTRYP qglGetVertexAttribfvARB) (GLuint index, GLenum pname, GLfloat *params);
extern GLvoid (APIENTRYP qglGetVertexAttribivARB) (GLuint index, GLenum pname, GLint *params);
extern GLvoid (APIENTRYP qglGetVertexAttribPointervARB) (GLuint index, GLenum pname, GLvoid **pointer);

// GL_EXT_geometry_shader4
extern GLvoid (APIENTRYP qglProgramParameteriEXT) (GLuint program, GLenum pname, GLint value);
extern GLvoid (APIENTRYP qglFramebufferTextureEXT) (GLenum target, GLenum attachment,
						    GLuint texture, GLint level);
extern GLvoid (APIENTRYP qglFramebufferTextureLayerEXT) (GLenum target, GLenum attachment,
							 GLuint texture, GLint level, GLint layer);
extern GLvoid (APIENTRYP qglFramebufferTextureFaceEXT) (GLenum target, GLenum attachment,
							GLuint texture, GLint level, GLenum face);

// GL_EXT_texture3D
extern GLvoid (APIENTRYP qglTexImage3DEXT) (GLenum target, GLint level,
					    GLenum internalformat,
					    GLsizei width, GLsizei height,
					    GLsizei depth, GLint border,
					    GLenum format, GLenum type,
					    const GLvoid *pixels);

// GL_EXT_texture_buffer_object
extern GLvoid (APIENTRYP qglTexBufferEXT) (GLenum target, GLenum internalFormat,
					   GLuint buffer);

// GL_ARB_uniform_buffer_object
extern GLvoid (APIENTRYP qglGetUniformIndices) (GLuint program,
						GLsizei uniformCount, 
						const GLchar** uniformNames, 
						GLuint* uniformIndices);
extern GLvoid (APIENTRYP qglGetActiveUniformsiv) (GLuint program,
						  GLsizei uniformCount, 
						  const GLuint* uniformIndices, 
						  GLenum pname, 
						  GLint* params);
extern GLvoid (APIENTRYP qglGetActiveUniformName) (GLuint program,
						   GLuint uniformIndex, 
						   GLsizei bufSize, 
						   GLsizei* length, 
						   GLchar* uniformName);
extern GLuint (APIENTRYP qglGetUniformBlockIndex) (GLuint program,
						   const GLchar* uniformBlockName);
extern GLvoid (APIENTRYP qglGetActiveUniformBlockiv) (GLuint program,
						      GLuint uniformBlockIndex, 
						      GLenum pname, 
						      GLint* params);
extern GLvoid (APIENTRYP qglGetActiveUniformBlockName) (GLuint program,
							GLuint uniformBlockIndex, 
							GLsizei bufSize, 
							GLsizei* length, 
							GLchar* uniformBlockName);
extern GLvoid (APIENTRYP qglBindBufferRange) (GLenum target, 
					      GLuint index, 
					      GLuint buffer, 
					      GLintptr offset,
					      GLsizeiptr size);
extern GLvoid (APIENTRYP qglBindBufferBase) (GLenum target, 
					     GLuint index, 
					     GLuint buffer);
extern GLvoid (APIENTRYP qglGetIntegeri_v) (GLenum target,
					    GLuint index,
					    GLint* data);
extern GLvoid (APIENTRYP qglUniformBlockBinding) (GLuint program,
						  GLuint uniformBlockIndex, 
						  GLuint uniformBlockBinding);

// GL_ARB_framebuffer_object
extern GLboolean (APIENTRYP qglIsRenderbuffer) (GLuint renderbuffer);
extern GLvoid (APIENTRYP qglBindRenderbuffer) (GLenum target, GLuint renderbuffer);
extern GLvoid (APIENTRYP qglDeleteRenderbuffers) (GLsizei n, const GLuint *renderbuffers);
extern GLvoid (APIENTRYP qglGenRenderbuffers) (GLsizei n, GLuint *renderbuffers);
extern GLvoid (APIENTRYP qglRenderbufferStorage) (GLenum target, GLenum internalformat,
						  GLsizei width, GLsizei height);
extern GLvoid (APIENTRYP qglRenderbufferStorageMultisample) (GLenum target, GLsizei samples,
							     GLenum internalformat,
							     GLsizei width, GLsizei height);
extern GLvoid (APIENTRYP qglGetRenderbufferParameteriv) (GLenum target, GLenum pname, GLint *params);
extern GLboolean (APIENTRYP qglIsFramebuffer) (GLuint framebuffer);
extern GLvoid (APIENTRYP qglBindFramebuffer) (GLenum target, GLuint framebuffer);
extern GLvoid (APIENTRYP qglDeleteFramebuffers) (GLsizei n, const GLuint *framebuffers);
extern GLvoid (APIENTRYP qglGenFramebuffers) (GLsizei n, GLuint *framebuffers);
extern GLenum (APIENTRYP qglCheckFramebufferStatus) (GLenum target);
extern GLvoid (APIENTRYP qglFramebufferTexture1D) (GLenum target, GLenum attachment,
						   GLenum textarget, GLuint texture, GLint level);
extern GLvoid (APIENTRYP qglFramebufferTexture2D) (GLenum target, GLenum attachment,
						   GLenum textarget, GLuint texture, GLint level);
extern GLvoid (APIENTRYP qglFramebufferTexture3D) (GLenum target, GLenum attachment,
						   GLenum textarget, GLuint texture,
						   GLint level, GLint layer);
extern GLvoid (APIENTRYP qglFramebufferTextureLayer) (GLenum target, GLenum attachment,
						      GLuint texture, GLint level, GLint layer);
extern GLvoid (APIENTRYP qglFramebufferRenderbuffer) (GLenum target, GLenum attachment,
						      GLenum renderbuffertarget, GLuint renderbuffer);
extern GLvoid (APIENTRYP qglGetFramebufferAttachmentParameteriv) (GLenum target, GLenum attachment,
								  GLenum pname, GLint *params);
extern GLvoid (APIENTRYP qglBlitFramebuffer) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1,
					      GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
					      GLbitfield mask, GLenum filter);
extern GLvoid (APIENTRYP qglGenerateMipmap) (GLenum target);

// GL_EXT_timer_query
extern GLvoid (APIENTRYP qglGenQueriesARB) (GLsizei n, GLuint *ids);
extern GLvoid (APIENTRYP qglDeleteQueriesARB) (GLsizei n, const GLuint *ids);
extern GLboolean (APIENTRYP qglIsQueryARB) (GLuint id);
extern GLvoid (APIENTRYP qglBeginQueryARB) (GLenum target, GLuint id);
extern GLvoid (APIENTRYP qglEndQueryARB) (GLenum target);
extern GLvoid (APIENTRYP qglGetQueryivARB) (GLenum target, GLenum pname, GLint *params);
extern GLvoid (APIENTRYP qglGetQueryObjectivARB) (GLuint id, GLenum pname, GLint *params);
extern GLvoid (APIENTRYP qglGetQueryObjectuivARB) (GLuint id, GLenum pname, GLuint *params);
extern GLvoid (APIENTRYP qglGetQueryObjecti64vEXT) (GLuint id, GLenum pname, GLint64EXT *params);
extern GLvoid (APIENTRYP qglGetQueryObjectui64vEXT) (GLuint id, GLenum pname, GLuint64EXT *params);

// GL_ARB_instanced_arrays
extern GLvoid (APIENTRYP qglVertexAttribDivisorARB) (GLuint index, GLuint divisor);
extern GLvoid (APIENTRYP qglDrawArraysInstancedARB) (GLenum mode, GLint first, GLsizei count,
						     GLsizei primcount);
extern GLvoid (APIENTRYP qglDrawElementsInstancedARB) (GLenum mode, GLsizei count, GLenum type,
						       const GLvoid *indices, GLsizei primcount);

// GL_ARB_separate_stencil, does not really exists, part of 2.0
extern GLvoid (APIENTRYP qglStencilOpSeparate) (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
extern GLvoid (APIENTRYP qglStencilFuncSeparate) (GLenum face, GLenum func, GLint ref, GLuint mask);
extern GLvoid (APIENTRYP qglStencilMaskSeparate) (GLenum face, GLuint mask);

// GL_ARB_debug_output, not part of core
extern GLvoid (APIENTRYP qglDebugMessageControlARB) (GLenum source,
						     GLenum type,
						     GLenum severity,
						     GLsizei count,
						     const GLuint* ids,
						     GLboolean enabled);
extern GLvoid (APIENTRYP qglDebugMessageInsertARB) (GLenum source,
						    GLenum type,
						    GLuint id,
						    GLenum severity,
						    GLsizei length, 
						    const GLchar* buf);
extern GLvoid (APIENTRYP qglDebugMessageCallbackARB) (GLDEBUGPROCARB callback,
						      GLvoid *userParam);
extern GLuint (APIENTRYP qglGetDebugMessageLogARB) (GLuint count,
						    GLsizei bufsize,
						    GLenum *sources,
						    GLenum *types,
						    GLuint *ids,
						    GLenum *severities,
						    GLsizei *lengths, 
						    GLchar *messageLog);
// GL_AMD_debug_output, predecessor to GL_ARB_debug_output, but has only
// a category parameter instead of source and type
extern GLvoid (APIENTRYP qglDebugMessageEnableAMD) (GLenum category,
						    GLenum severity,
						    GLsizei count,
						    const GLuint* ids,
						    GLboolean enabled);
extern GLvoid (APIENTRYP qglDebugMessageInsertAMD) (GLenum category,
						    GLuint id,
						    GLenum severity,
						    GLsizei length, 
						    const GLchar* buf);
extern GLvoid (APIENTRYP qglDebugMessageCallbackAMD) (GLDEBUGPROCAMD callback,
						      GLvoid *userParam);
extern GLuint (APIENTRYP qglGetDebugMessageLogAMD) (GLuint count,
						    GLsizei bufsize,
						    GLenum *categories,
						    GLuint *ids,
						    GLenum *severities,
						    GLsizei *lengths, 
						    GLchar *messageLog);

// GL_ARB_blend_func_extended
extern GLvoid (APIENTRYP qglBindFragDataLocationIndexed) (GLuint program,
							  GLuint colorNumber,
							  GLuint index,
							  const GLchar *name);
extern GLint (APIENTRYP qglGetFragDataIndex) (GLuint program,
					      const GLchar *name);


#endif
