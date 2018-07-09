/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_surf.c: surface-related refresh code

#include "quakedef.h"

int			skytexturenum;

#ifndef GL_RGBA4
#define	GL_RGBA4	0
#endif


int		lightmap_bytes;		// 1, 2, or 4

GLuint	lightmap_textures;

unsigned		blocklights[18*18];

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define	MAX_LIGHTMAPS	64
int			active_lightmaps;

typedef struct glRect_s {
	unsigned char l,t,w,h;
} glRect_t;

glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
qboolean	lightmap_modified[MAX_LIGHTMAPS];
glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];

int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		lightmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];

// For gl_texsort 0
msurface_t  *skychain = NULL;
msurface_t  *waterchain = NULL;

int         gl_batchmark;

typedef struct glpolygon2texbatch_s
{
    int segmentcount;
    int segmenttop;
    int segmenttotal;
    
    void** segmentpointers;

    qboolean differ;
    
    GLsizei* segments;
    GLuint* tex0names;
    GLuint* tex1names;
    
    int vertexcount;
    int vertextop;
    int vertextotal;
    
    GLfloat* vertices;
    
    qboolean usevertexbuffer;
    
    GLuint vertexbuffer;
    
} glpolygon2texbatch_t;

glpolygon2texbatch_t gl_worldnormal2texbatch;
glpolygon2texbatch_t gl_worldwarped2texbatch;
glpolygon2texbatch_t gl_brushnormal2texbatch;
glpolygon2texbatch_t gl_brushwarped2texbatch;

glpolygon2texbatch_t* gl_normal2texbatch;
glpolygon2texbatch_t* gl_warped2texbatch;

void R_RenderDynamicLightmaps (msurface_t *fa);

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].origin, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabs(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];
		
		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
					blocklights[t*smax + s] += (rad - dist)*256;
			}
		}
	}
}


/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
//    int            lightadj[4];
	unsigned	*bl;

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

// set to full bright if no light data
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		for (i=0 ; i<size ; i++)
			blocklights[i] = 255*256;
		goto store;
	}

// clear to no light
	for (i=0 ; i<size ; i++)
		blocklights[i] = 0;

// add all the lightmaps
	if (lightmap)
		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			for (i=0 ; i<size ; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size;	// skip to next lightmap
		}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

// bound, invert, and shift
store:
	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		stride -= (smax<<2);
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				t = *bl++;
				t >>= 7;
				if (t > 255)
					t = 255;
				dest[3] = 255-t;
				dest += 4;
			}
		}
		break;
	case GL_ALPHA:
	case GL_LUMINANCE:
	//case GL_INTENSITY:  // Since we're defining GL_INTENSITY as GL_LUMINANCE, this no longer needs to be evaluated
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				t = *bl++;
				t >>= 7;
				if (t > 255)
					t = 255;
				dest[j] = 255-t;
			}
		}
		break;
	default:
		Sys_Error ("Bad lightmap format");
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
	int		reletive;
	int		count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	reletive = (int)(cl.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


extern	int		solidskytexture;
extern	int		alphaskytexture;
extern	float	speedscale;		// for top sky and bottom sky

void DrawGLWaterPoly (glpoly_t *p);
void DrawGLWaterPolyLightmap (glpoly_t *p);

lpMTexFUNC qglMTexCoord2fSGIS = NULL;

qboolean mtexenabled = false;

void GL_SelectTexture (GLenum target);

void GL_DisableMultitexture(void) 
{
	if (mtexenabled) {
		GL_SelectTexture(GL_TEXTURE0);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture(void) 
{
	if (gl_mtexable) {
		GL_SelectTexture(GL_TEXTURE1);
		mtexenabled = true;
	}
}

void R_BeginPolygon2Tex (glpolygon2texbatch_t* batch)
{
    if (batch->segmentcount < batch->segmenttotal)
    {
        batch->segmentcount = batch->segmenttotal;
        
        if (batch->usevertexbuffer && batch->segmentpointers != NULL)
        {
            free (batch->segmentpointers);
            batch->segmentpointers = NULL;
        }
    }

    if (batch->segmentcount > 0)
    {
        batch->segments = Hunk_AllocName(batch->segmentcount * sizeof(GLsizei), "sequential_2tex_segments");
        batch->tex0names = Hunk_AllocName(batch->segmentcount * sizeof(GLuint), "sequential_2tex_tex0names");
        batch->tex1names = Hunk_AllocName(batch->segmentcount * sizeof(GLuint), "sequential_2tex_tex1names");

        if (batch->usevertexbuffer && batch->segmentpointers == NULL)
        {
            batch->segmentpointers = calloc (batch->segmentcount, sizeof(void*));
        }
    }
    
    batch->segmenttotal = 0;
    batch->segmenttop = 0;
    
    if (batch->vertexcount < batch->vertextotal)
    {
        batch->vertexcount = batch->vertextotal;
    }
    
    if (batch->vertexcount > 0)
    {
        batch->vertices = Hunk_AllocName(batch->vertexcount * VERTEXSIZE * sizeof(GLfloat), "sequential_2tex_vertices");
    }
    
    batch->vertextotal = 0;
    batch->vertextop = 0;
    batch->differ = false;
}

void R_ConfigurePolygon2TexBuffers (void)
{
    glEnableVertexAttribArray(gl_polygon2texturesprogram_position);
    glVertexAttribPointer(gl_polygon2texturesprogram_position, 3, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(gl_polygon2texturesprogram_texcoords0);
    glVertexAttribPointer(gl_polygon2texturesprogram_texcoords0, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(gl_polygon2texturesprogram_texcoords1);
    glVertexAttribPointer(gl_polygon2texturesprogram_texcoords1, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)(5 * sizeof(GLfloat)));
}

GLuint R_CreatePolygon2TexBuffers (GLsizeiptr count, const GLvoid* vertices)
{
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, count * VERTEXSIZE * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

    R_ConfigurePolygon2TexBuffers();
    
    return vertexbuffer;
}

void R_UnbindPolygon2TexBuffers (void)
{
    glDisableVertexAttribArray(gl_polygon2texturesprogram_texcoords1);
    glDisableVertexAttribArray(gl_polygon2texturesprogram_texcoords0);
    glDisableVertexAttribArray(gl_polygon2texturesprogram_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void R_DisposePolygon2TexBuffers (GLuint vertexbuffer)
{
    R_UnbindPolygon2TexBuffers ();
    
    glDeleteBuffers(1, &vertexbuffer);
}

void R_DrawSegmentsInPolygon2Tex (glpolygon2texbatch_t* batch)
{
    GLsizei offset = 0;
    
    for (int i = 0; i < batch->segmenttop; i++)
    {
        GLsizei count = batch->segments[i];
        GLuint tex0 = batch->tex0names[i];
        GLuint tex1 = batch->tex1names[i];
        
        // Binds world to texture env 0
        GL_SelectTexture(GL_TEXTURE0);
        GL_Bind (tex0);
        
        // Binds lightmap to texenv 1
        GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)
        GL_Bind (tex1);
        
        glDrawArrays(GL_TRIANGLE_FAN, offset, count);
        
        offset += count;
    }
}

void R_DrawPolygon2Tex (glpolygon2texbatch_t* batch)
{
    GLuint vertexbuffer = R_CreatePolygon2TexBuffers(batch->vertextop, batch->vertices);
    
    R_DrawSegmentsInPolygon2Tex (batch);
    
    R_DisposePolygon2TexBuffers (vertexbuffer);
}

void R_DrawPendingPolygon2Tex (glpolygon2texbatch_t* batch)
{
    if (batch->usevertexbuffer)
    {
        if (batch->differ && batch->vertexbuffer > 0)
        {
            glDeleteBuffers(1, &batch->vertexbuffer);
            
            batch->vertexbuffer = 0;
        }
        
        if (batch->vertexbuffer == 0)
        {
            batch->vertexbuffer = R_CreatePolygon2TexBuffers(batch->vertextop, batch->vertices);
        }
        else
        {
            glBindBuffer(GL_ARRAY_BUFFER, batch->vertexbuffer);
            
            R_ConfigurePolygon2TexBuffers ();
        }
        
        R_DrawSegmentsInPolygon2Tex (batch);
        
        R_UnbindPolygon2TexBuffers ();
    }
    else
    {
        R_DrawPolygon2Tex (batch);
    }
}

void R_ResetPolygon2Tex (glpolygon2texbatch_t* batch)
{
    if (batch->usevertexbuffer && batch->segmentpointers != NULL)
    {
        memset(batch->segmentpointers, 0, batch->segmentcount * sizeof(void*));
    }
    
    batch->segmenttop = 0;
    batch->vertextop = 0;
}

void R_EndPolygon2Tex (glpolygon2texbatch_t* batch)
{
    if (batch->segmenttop > 0 && batch->vertextop > 0)
    {
        GL_Use (gl_polygon2texturesprogram);
        
        R_DrawPendingPolygon2Tex (batch);
    }
}

void R_CopySequentialWarped2Tex (GLfloat* source, int size, GLfloat* destination)
{
    GLfloat* v = source;
    int j = 0;
    for (int i=0 ; i<size ; i++, v+= VERTEXSIZE)
    {
        destination[j++] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
        destination[j++] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
        destination[j++] = v[2];
        
        destination[j++] = v[3];
        destination[j++] = v[4];
        
        destination[j++] = v[5];
        destination[j++] = v[6];
    }
}

#if 0
/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly (msurface_t *s)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	texture_t	*t;

	//
	// normal lightmaped poly
	//
	if (! (s->flags & (SURF_DRAWSKY|SURF_DRAWTURB|SURF_UNDERWATER) ) )
	{
		p = s->polys;

		t = R_TextureAnimation (s->texinfo->texture);
		GL_Bind (t->gl_texturenum);
		glBegin (GL_POLYGON);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			glTexCoord2f (v[3], v[4]);
			glVertex3fv (v);
		}
		glEnd ();

		GL_Bind (lightmap_textures + s->lightmaptexturenum);
		glEnable (GL_BLEND);
		glBegin (GL_POLYGON);
		v = p->verts[0];
		for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
		{
			glTexCoord2f (v[5], v[6]);
			glVertex3fv (v);
		}
		glEnd ();

		glDisable (GL_BLEND);

		return;
	}

	//
	// subdivided water surface warp
	//
	if (s->flags & SURF_DRAWTURB)
	{
		GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys (s);
		return;
	}

	//
	// subdivided sky warp
	//
	if (s->flags & SURF_DRAWSKY)
	{
		GL_Bind (solidskytexture);
		speedscale = realtime*8;
		speedscale -= (int)speedscale;

		EmitSkyPolys (s);

		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_Bind (alphaskytexture);
		speedscale = realtime*16;
		speedscale -= (int)speedscale;
		EmitSkyPolys (s);
		if (gl_lightmap_format == GL_LUMINANCE)
			glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);

		glDisable (GL_BLEND);
	}

	//
	// underwater warped with lightmap
	//
	p = s->polys;

	t = R_TextureAnimation (s->texinfo->texture);
	GL_Bind (t->gl_texturenum);
	DrawGLWaterPoly (p);

	GL_Bind (lightmap_textures + s->lightmaptexturenum);
	glEnable (GL_BLEND);
	DrawGLWaterPolyLightmap (p);
	glDisable (GL_BLEND);
}
#else
/*
================
R_DrawSequentialPoly

Systems that have fast state and texture changes can
just do everything as it passes with no need to sort
================
*/
void R_DrawSequentialPoly (msurface_t *s)
{
	glpoly_t	*p;
//    float        *v;
	int			i;
	texture_t	*t;
//    vec3_t        nv, dir;
//    float        ss, ss2, length;
//    float        s1, t1;
	glRect_t	*theRect;

	//
	// normal lightmaped poly
	//

	if (! (s->flags & (SURF_DRAWSKY|SURF_DRAWTURB|SURF_UNDERWATER) ) )
	{
        R_RenderDynamicLightmaps (s);
		if (gl_mtexable) {
            if (gl_warped2texbatch->segmenttop > 0 && gl_warped2texbatch->vertextop > 0)
            {
                GL_Use (gl_polygon2texturesprogram);
                
                R_DrawPendingPolygon2Tex (gl_warped2texbatch);
                
                R_ResetPolygon2Tex (gl_warped2texbatch);
            }
            
			p = s->polys;

            GL_Use (gl_polygon2texturesprogram);
            
			t = R_TextureAnimation (s->texinfo->texture);
			i = s->lightmaptexturenum;
			if (lightmap_modified[i])
			{
				lightmap_modified[i] = false;

                // Binds world to texture env 0
                GL_SelectTexture(GL_TEXTURE0);
                GL_Bind (t->gl_texturenum);
                // Binds lightmap to texenv 1
                GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)
                GL_Bind (lightmap_textures + s->lightmaptexturenum);
                
                theRect = &lightmap_rectchange[i];
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
					BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
					lightmaps+(i* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);
				theRect->l = BLOCK_WIDTH;
				theRect->t = BLOCK_HEIGHT;
				theRect->h = 0;
				theRect->w = 0;
			}
            
            qboolean copytobatch = false;
            
            if (gl_normal2texbatch->segmentcount >= gl_normal2texbatch->segmenttop + 1 && gl_normal2texbatch->vertexcount >= gl_normal2texbatch->vertextop + p->numverts)
            {
                copytobatch = true;
            }
            else
            {
                if (gl_normal2texbatch->segmenttop > 0 && gl_normal2texbatch->vertextop > 0)
                {
                    R_DrawPolygon2Tex (gl_normal2texbatch);
                    
                    R_ResetPolygon2Tex (gl_normal2texbatch);
                }
                
                if (gl_normal2texbatch->segmentcount >= 1 && gl_normal2texbatch->vertexcount >= p->numverts)
                {
                    copytobatch = true;
                }
            }
            
            if (copytobatch)
            {
                if (gl_normal2texbatch->usevertexbuffer && gl_normal2texbatch->segmentpointers != NULL)
                {
                    if (gl_normal2texbatch->segmentpointers[gl_normal2texbatch->segmenttop] != p->verts && !gl_normal2texbatch->differ)
                    {
                        gl_normal2texbatch->differ = true;
                    }
                
                    gl_normal2texbatch->segmentpointers[gl_normal2texbatch->segmenttop] = p->verts;
                }

                gl_normal2texbatch->segments[gl_normal2texbatch->segmenttop] = p->numverts;
                gl_normal2texbatch->tex0names[gl_normal2texbatch->segmenttop] = t->gl_texturenum;
                gl_normal2texbatch->tex1names[gl_normal2texbatch->segmenttop] = lightmap_textures + s->lightmaptexturenum;
                
                gl_normal2texbatch->segmenttop++;

                memcpy(gl_normal2texbatch->vertices + gl_normal2texbatch->vertextop * VERTEXSIZE, p->verts, p->numverts * VERTEXSIZE * sizeof(GLfloat));
                
                gl_normal2texbatch->vertextop += p->numverts;
            }
            else
            {
                GLuint vertexbuffer = R_CreatePolygon2TexBuffers(p->numverts, p->verts);
                
                // Binds world to texture env 0
                GL_SelectTexture(GL_TEXTURE0);
                GL_Bind (t->gl_texturenum);
                
                // Binds lightmap to texenv 1
                GL_EnableMultitexture(); // Same as SelectTexture (TEXTURE1)
                GL_Bind (lightmap_textures + s->lightmaptexturenum);
                
                glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
                
                R_DisposePolygon2TexBuffers (vertexbuffer);
            }

            gl_normal2texbatch->segmenttotal++;
            gl_normal2texbatch->vertextotal += p->numverts;
            
            return;
		} else {
			p = s->polys;

            GL_Use (gl_polygon1textureprogram);
            
            glUniformMatrix4fv(gl_polygon1textureprogram_transform, 1, 0, gl_polygon_matrix);

            t = R_TextureAnimation (s->texinfo->texture);
			GL_Bind (t->gl_texturenum);
            
            int mark = Hunk_LowMark ();
            
            GLuint* indices;
            int indexcount;
            
            GL_Triangulate ((GLfloat*)p->verts, p->numverts, VERTEXSIZE, &indices, &indexcount);
            
            GLuint vertexbuffer;
            glGenBuffers(1, &vertexbuffer);
            
            glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
            glBufferData(GL_ARRAY_BUFFER, p->numverts * VERTEXSIZE * sizeof(GLfloat), (GLfloat*)p->verts, GL_STATIC_DRAW);
            
            glEnableVertexAttribArray(gl_polygon1textureprogram_position);
            glVertexAttribPointer(gl_polygon1textureprogram_position, 3, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)0);
            glEnableVertexAttribArray(gl_polygon1textureprogram_texcoords);
            glVertexAttribPointer(gl_polygon1textureprogram_texcoords, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)(3 * sizeof(GLfloat)));
            
            GLuint elementbuffer;
            glGenBuffers(1, &elementbuffer);
            
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexcount * sizeof(GLuint), indices, GL_STATIC_DRAW);
            
            glDrawElements(GL_TRIANGLES, indexcount, GL_UNSIGNED_INT, 0);
            
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            
            glDeleteBuffers(1, &elementbuffer);
            
            glDisableVertexAttribArray(gl_polygon1textureprogram_texcoords);
            glDisableVertexAttribArray(gl_polygon1textureprogram_position);
            
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            
            glDeleteBuffers(1, &vertexbuffer);
            
            GL_Bind (lightmap_textures + s->lightmaptexturenum);
            
            glEnable (GL_BLEND);

            glGenBuffers(1, &vertexbuffer);
            
            glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
            glBufferData(GL_ARRAY_BUFFER, p->numverts * VERTEXSIZE * sizeof(GLfloat), (GLfloat*)p->verts, GL_STATIC_DRAW);
            
            glEnableVertexAttribArray(gl_polygon1textureprogram_position);
            glVertexAttribPointer(gl_polygon1textureprogram_position, 3, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)0);
            glEnableVertexAttribArray(gl_polygon1textureprogram_texcoords);
            glVertexAttribPointer(gl_polygon1textureprogram_texcoords, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)(5 * sizeof(GLfloat)));
            
            glGenBuffers(1, &elementbuffer);
            
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexcount * sizeof(GLuint), indices, GL_STATIC_DRAW);
            
            glDrawElements(GL_TRIANGLES, indexcount, GL_UNSIGNED_INT, 0);
            
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            
            glDeleteBuffers(1, &elementbuffer);
            
            glDisableVertexAttribArray(gl_polygon1textureprogram_texcoords);
            glDisableVertexAttribArray(gl_polygon1textureprogram_position);
            
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            
            glDeleteBuffers(1, &vertexbuffer);

            Hunk_FreeToLowMark (mark);
            
            glDisable (GL_BLEND);
		}

		return;
	}

	//
	// subdivided water surface warp
	//

	if (s->flags & SURF_DRAWTURB)
	{
        if (gl_normal2texbatch->segmenttop > 0 && gl_normal2texbatch->vertextop > 0)
        {
            GL_Use (gl_polygon2texturesprogram);
            
            R_DrawPendingPolygon2Tex (gl_normal2texbatch);
            
            R_ResetPolygon2Tex (gl_normal2texbatch);
        }
        
        if (gl_warped2texbatch->segmenttop > 0 && gl_warped2texbatch->vertextop > 0)
        {
            GL_Use (gl_polygon2texturesprogram);
            
            R_DrawPendingPolygon2Tex (gl_warped2texbatch);
            
            R_ResetPolygon2Tex (gl_warped2texbatch);
        }
        
        GL_Use (gl_polygon1textureprogram);

        glUniformMatrix4fv(gl_polygon1textureprogram_transform, 1, 0, gl_polygon_matrix);

        gl_waterpolygon_position = gl_polygon1textureprogram_position;
        gl_waterpolygon_texcoords = gl_polygon1textureprogram_texcoords;
        
        GL_DisableMultitexture();
		GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys (s);
		return;
	}

	//
	// subdivided sky warp
	//
	if (s->flags & SURF_DRAWSKY)
	{
        if (gl_normal2texbatch->segmenttop > 0 && gl_normal2texbatch->vertextop > 0)
        {
            GL_Use (gl_polygon2texturesprogram);
            
            R_DrawPendingPolygon2Tex (gl_normal2texbatch);
            
            R_ResetPolygon2Tex (gl_normal2texbatch);
        }
        
        if (gl_warped2texbatch->segmenttop > 0 && gl_warped2texbatch->vertextop > 0)
        {
            GL_Use (gl_polygon2texturesprogram);
            
            R_DrawPendingPolygon2Tex (gl_warped2texbatch);
            
            R_ResetPolygon2Tex (gl_warped2texbatch);
        }
        
        GL_Use (gl_polygon1textureprogram);
        
        glUniformMatrix4fv(gl_polygon1textureprogram_transform, 1, 0, gl_polygon_matrix);

        GL_DisableMultitexture();
		GL_Bind (solidskytexture);
		speedscale = realtime*8;
		speedscale -= (int)speedscale & ~127;

		EmitSkyPolys (s);

		glEnable (GL_BLEND);
		GL_Bind (alphaskytexture);
		speedscale = realtime*16;
		speedscale -= (int)speedscale & ~127;
		EmitSkyPolys (s);

		glDisable (GL_BLEND);
		return;
	}

	//
	// underwater warped with lightmap
	//
    R_RenderDynamicLightmaps (s);
	if (gl_mtexable) {
        if (gl_normal2texbatch->segmenttop > 0 && gl_normal2texbatch->vertextop > 0)
        {
            GL_Use (gl_polygon2texturesprogram);
            
            R_DrawPendingPolygon2Tex (gl_normal2texbatch);
            
            R_ResetPolygon2Tex (gl_normal2texbatch);
        }
        
		p = s->polys;

        GL_Use (gl_polygon2texturesprogram);
        
		t = R_TextureAnimation (s->texinfo->texture);
		i = s->lightmaptexturenum;
		if (lightmap_modified[i])
		{
			lightmap_modified[i] = false;

            GL_SelectTexture(GL_TEXTURE0);
            GL_Bind (t->gl_texturenum);
            GL_EnableMultitexture();
            GL_Bind (lightmap_textures + s->lightmaptexturenum);
            
            theRect = &lightmap_rectchange[i];
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
				BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
				lightmaps+(i* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);
			theRect->l = BLOCK_WIDTH;
			theRect->t = BLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}
        
        qboolean copytobatch = false;
        
        if (gl_warped2texbatch->segmentcount >= gl_warped2texbatch->segmenttop + 1 && gl_warped2texbatch->vertexcount >= gl_warped2texbatch->vertextop + p->numverts)
        {
            copytobatch = true;
        }
        else
        {
            if (gl_warped2texbatch->segmenttop > 0 && gl_warped2texbatch->vertextop > 0)
            {
                R_DrawPolygon2Tex (gl_warped2texbatch);
                
                R_ResetPolygon2Tex (gl_warped2texbatch);
            }
            
            if (gl_warped2texbatch->segmentcount >= 1 && gl_warped2texbatch->vertexcount >= p->numverts)
            {
                copytobatch = true;
            }
        }
        
        if (copytobatch)
        {
            if (gl_normal2texbatch->usevertexbuffer && gl_warped2texbatch->segmentpointers != NULL)
            {
                if (gl_warped2texbatch->segmentpointers[gl_warped2texbatch->segmenttop] != p->verts && !gl_warped2texbatch->differ)
                {
                    gl_warped2texbatch->differ = true;
                }
            
                gl_warped2texbatch->segmentpointers[gl_warped2texbatch->segmenttop] = p->verts;
            }
            
            gl_warped2texbatch->segments[gl_warped2texbatch->segmenttop] = p->numverts;
            gl_warped2texbatch->tex0names[gl_warped2texbatch->segmenttop] = t->gl_texturenum;
            gl_warped2texbatch->tex1names[gl_warped2texbatch->segmenttop] = lightmap_textures + s->lightmaptexturenum;
            
            gl_warped2texbatch->segmenttop++;
            
            R_CopySequentialWarped2Tex((GLfloat*)p->verts, p->numverts, gl_warped2texbatch->vertices + gl_warped2texbatch->vertextop * VERTEXSIZE);
            
            gl_warped2texbatch->vertextop += p->numverts;
        }
        else
        {
            int mark = Hunk_LowMark();
            
            GLfloat* vertices = Hunk_AllocName (p->numverts * VERTEXSIZE * sizeof(GLfloat), "vertex_buffer");
            
            R_CopySequentialWarped2Tex((GLfloat*)p->verts, p->numverts, vertices);

            GLuint vertexbuffer = R_CreatePolygon2TexBuffers(p->numverts, vertices);
            
            GL_SelectTexture(GL_TEXTURE0);
            GL_Bind (t->gl_texturenum);
            GL_EnableMultitexture();
            GL_Bind (lightmap_textures + s->lightmaptexturenum);
            
            glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
            
            R_DisposePolygon2TexBuffers (vertexbuffer);
            
            Hunk_FreeToLowMark (mark);
        }
        
        gl_warped2texbatch->segmenttotal++;
        gl_warped2texbatch->vertextotal += p->numverts;
        
    } else {
		p = s->polys;

        GL_Use (gl_polygon1textureprogram);
        
        glUniformMatrix4fv(gl_polygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
        
        gl_waterpolygon_position = gl_polygon1textureprogram_position;
        gl_waterpolygon_texcoords = gl_polygon1textureprogram_texcoords;

        t = R_TextureAnimation (s->texinfo->texture);
		GL_Bind (t->gl_texturenum);
		DrawGLWaterPoly (p);

		GL_Bind (lightmap_textures + s->lightmaptexturenum);
		glEnable (GL_BLEND);
		DrawGLWaterPolyLightmap (p);
		glDisable (GL_BLEND);
	}
}
#endif


/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly (glpoly_t *p)
{
	int		i;
	float	*v;
//    float    s, t, os, ot;
//    vec3_t    nv;

	GL_DisableMultitexture();

    int mark = Hunk_LowMark ();

    GLfloat* vertices = Hunk_AllocName (p->numverts * 5 * sizeof(GLfloat), "vertex_buffer");
    
    v = p->verts[0];
    int j = 0;
    for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
    {
        vertices[j++] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
        vertices[j++] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
        vertices[j++] = v[2];
        
        vertices[j++] = v[3];
        vertices[j++] = v[4];
    }
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, p->numverts * 5 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_waterpolygon_position);
    glVertexAttribPointer(gl_waterpolygon_position, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(gl_waterpolygon_texcoords);
    glVertexAttribPointer(gl_waterpolygon_texcoords, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const GLvoid *)(3 * sizeof(GLfloat)));
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
    
    glDisableVertexAttribArray(gl_waterpolygon_texcoords);
    glDisableVertexAttribArray(gl_waterpolygon_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);
    
    Hunk_FreeToLowMark (mark);
}

void DrawGLWaterPolyLightmap (glpoly_t *p)
{
	int		i;
	float	*v;
//    float    s, t, os, ot;
//    vec3_t    nv;

	GL_DisableMultitexture();

    int mark = Hunk_LowMark();
    
    GLfloat* vertices = Hunk_AllocName (p->numverts * 5 * sizeof(GLfloat), "vertex_buffer");
    
    v = p->verts[0];
    int j = 0;
    for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
    {
        vertices[j++] = v[0] + 8*sin(v[1]*0.05+realtime)*sin(v[2]*0.05+realtime);
        vertices[j++] = v[1] + 8*sin(v[0]*0.05+realtime)*sin(v[2]*0.05+realtime);
        vertices[j++] = v[2];
        
        vertices[j++] = v[5];
        vertices[j++] = v[6];
    }
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, p->numverts * 5 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_waterpolygon_position);
    glVertexAttribPointer(gl_waterpolygon_position, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(gl_waterpolygon_texcoords);
    glVertexAttribPointer(gl_waterpolygon_texcoords, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const GLvoid *)(3 * sizeof(GLfloat)));
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, p->numverts);
    
    glDisableVertexAttribArray(gl_waterpolygon_texcoords);
    glDisableVertexAttribArray(gl_waterpolygon_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);
    
    Hunk_FreeToLowMark (mark);
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly (glpoly_t *p)
{
//    int        i;
//    float    *v;

    GLint position;
    GLint texcoords;
    
    if (gl_rendermirror_enabled)
    {
        GL_Use (gl_tintedpolygon1textureprogram);
        
        glUniformMatrix4fv(gl_tintedpolygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
        glUniform4fv(gl_tintedpolygon1textureprogram_color, 1, gl_rendermirror_color);
        
        position = gl_tintedpolygon1textureprogram_position;
        texcoords = gl_tintedpolygon1textureprogram_texcoords;
    }
    else
    {
        GL_Use (gl_polygon1textureprogram);
        
        glUniformMatrix4fv(gl_polygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
        
        position = gl_polygon1textureprogram_position;
        texcoords = gl_polygon1textureprogram_texcoords;
    }

    int mark = Hunk_LowMark ();
    
    GLuint* indices;
    int indexcount;
    
    GL_Triangulate ((GLfloat*)p->verts, p->numverts, VERTEXSIZE, &indices, &indexcount);
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, p->numverts * VERTEXSIZE * sizeof(GLfloat), (GLfloat*)p->verts, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(position);
    glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(texcoords);
    glVertexAttribPointer(texcoords, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)(3 * sizeof(GLfloat)));
    
    GLuint elementbuffer;
    glGenBuffers(1, &elementbuffer);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexcount * sizeof(GLuint), indices, GL_STATIC_DRAW);
    
    glDrawElements(GL_TRIANGLES, indexcount, GL_UNSIGNED_INT, 0);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &elementbuffer);
    
    glDisableVertexAttribArray(texcoords);
    glDisableVertexAttribArray(position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);
    
    Hunk_FreeToLowMark (mark);
}


/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps (void)
{
    int			i;//, j;
	glpoly_t	*p;
//    float        *v;
	glRect_t	*theRect;

	if (r_fullbright.value)
		return;
	if (!gl_texsort.value)
		return;

	glDepthMask (0);		// don't bother writing Z

	if (gl_lightmap_format == GL_LUMINANCE)
    {
        GL_Use (gl_polygon1textureprogram);
    
        glUniformMatrix4fv(gl_polygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
    
        gl_waterpolygon_position = gl_polygon1textureprogram_position;
        gl_waterpolygon_texcoords = gl_polygon1textureprogram_texcoords;

        glBlendFunc (GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    }
	else if (gl_lightmap_format == GL_INTENSITY)
	{
        GL_Use (gl_tintedpolygon1textureprogram);
        
        glEnable (GL_BLEND);
        
        glUniformMatrix4fv(gl_tintedpolygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
        glUniform4f(gl_tintedpolygon1textureprogram_color, 0.0, 0.0, 0.0, 1.0);
        
        gl_waterpolygon_position = gl_tintedpolygon1textureprogram_position;
        gl_waterpolygon_texcoords = gl_tintedpolygon1textureprogram_texcoords;

        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

    if (!r_lightmap.value)
	{
		glEnable (GL_BLEND);
	}

	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		p = lightmap_polys[i];
		if (!p)
			continue;
		GL_Bind(lightmap_textures+i);
		if (lightmap_modified[i])
		{
			lightmap_modified[i] = false;
			theRect = &lightmap_rectchange[i];
//			glTexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes
//			, BLOCK_WIDTH, BLOCK_HEIGHT, 0, 
//			gl_lightmap_format, GL_UNSIGNED_BYTE, lightmaps+i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes);
//			glTexImage2D (GL_TEXTURE_2D, 0, lightmap_bytes
//				, BLOCK_WIDTH, theRect->h, 0, 
//				gl_lightmap_format, GL_UNSIGNED_BYTE, lightmaps+(i*BLOCK_HEIGHT+theRect->t)*BLOCK_WIDTH*lightmap_bytes);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, theRect->t, 
				BLOCK_WIDTH, theRect->h, gl_lightmap_format, GL_UNSIGNED_BYTE,
				lightmaps+(i* BLOCK_HEIGHT + theRect->t) *BLOCK_WIDTH*lightmap_bytes);
			theRect->l = BLOCK_WIDTH;
			theRect->t = BLOCK_HEIGHT;
			theRect->h = 0;
			theRect->w = 0;
		}
		for ( ; p ; p=p->chain)
		{
			if (p->flags & SURF_UNDERWATER)
				DrawGLWaterPolyLightmap (p);
			else
			{
                int mark = Hunk_LowMark ();
                
                int indexcount;
                GLuint* indices;
                
                GL_Triangulate ((GLfloat*)p->verts, p->numverts, VERTEXSIZE, &indices, &indexcount);
                
                GLuint vertexbuffer;
                glGenBuffers(1, &vertexbuffer);
                
                glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
                glBufferData(GL_ARRAY_BUFFER, p->numverts * VERTEXSIZE * sizeof(GLfloat), (GLfloat*)p->verts, GL_STATIC_DRAW);
                
                glEnableVertexAttribArray(gl_waterpolygon_position);
                glVertexAttribPointer(gl_waterpolygon_position, 3, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)0);
                glEnableVertexAttribArray(gl_waterpolygon_texcoords);
                glVertexAttribPointer(gl_waterpolygon_texcoords, 2, GL_FLOAT, GL_FALSE, VERTEXSIZE * sizeof(GLfloat), (const GLvoid *)(5 * sizeof(GLfloat)));
                
                GLuint elementbuffer;
                glGenBuffers(1, &elementbuffer);
                
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexcount * sizeof(GLuint), indices, GL_STATIC_DRAW);
                
                glDrawElements(GL_TRIANGLES, indexcount, GL_UNSIGNED_INT, 0);
                
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                
                glDeleteBuffers(1, &elementbuffer);
                
                glDisableVertexAttribArray(gl_waterpolygon_texcoords);
                glDisableVertexAttribArray(gl_waterpolygon_position);
                
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                
                glDeleteBuffers(1, &vertexbuffer);
                
                Hunk_FreeToLowMark (mark);
            }
		}
	}

	glDisable (GL_BLEND);
	if (gl_lightmap_format == GL_LUMINANCE)
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glDepthMask (1);		// back to normal Z buffering
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly (msurface_t *fa)
{
	texture_t	*t;
	byte		*base;
	int			maps;
	glRect_t    *theRect;
	int smax, tmax;

	c_brush_polys++;

	if (fa->flags & SURF_DRAWSKY)
	{	// warp texture, no lightmaps
		EmitBothSkyLayers (fa);
		return;
	}
		
	t = R_TextureAnimation (fa->texinfo->texture);
	GL_Bind (t->gl_texturenum);

	if (fa->flags & SURF_DRAWTURB)
	{	// warp texture, no lightmaps
        if (gl_rendermirror_enabled)
        {
            GL_Use (gl_tintedpolygon1textureprogram);
            
            glUniformMatrix4fv(gl_tintedpolygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
            glUniform4fv(gl_tintedpolygon1textureprogram_color, 1, gl_rendermirror_color);
            
            gl_waterpolygon_position = gl_tintedpolygon1textureprogram_position;
            gl_waterpolygon_texcoords = gl_tintedpolygon1textureprogram_texcoords;
        }
        else
        {
            GL_Use (gl_polygon1textureprogram);
            
            glUniformMatrix4fv(gl_polygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
            
            gl_waterpolygon_position = gl_polygon1textureprogram_position;
            gl_waterpolygon_texcoords = gl_polygon1textureprogram_texcoords;
        }
        
		EmitWaterPolys (fa);
		return;
	}

	if (fa->flags & SURF_UNDERWATER)
    {
        if (gl_rendermirror_enabled)
        {
            GL_Use (gl_tintedpolygon1textureprogram);
            
            glUniformMatrix4fv(gl_tintedpolygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
            glUniform4fv(gl_tintedpolygon1textureprogram_color, 1, gl_rendermirror_color);
            
            gl_waterpolygon_position = gl_tintedpolygon1textureprogram_position;
            gl_waterpolygon_texcoords = gl_tintedpolygon1textureprogram_texcoords;
        }
        else
        {
            GL_Use (gl_polygon1textureprogram);
            
            glUniformMatrix4fv(gl_polygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
            
            gl_waterpolygon_position = gl_polygon1textureprogram_position;
            gl_waterpolygon_texcoords = gl_polygon1textureprogram_texcoords;
        }

        DrawGLWaterPoly (fa->polys);
    }
	else
		DrawGLPoly (fa->polys);

	// add the poly to the proper lightmap chain

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ;
		 maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
dynamic:
		if (r_dynamic.value)
		{
			lightmap_modified[fa->lightmaptexturenum] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];
			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			smax = (fa->extents[0]>>4)+1;
			tmax = (fa->extents[1]>>4)+1;
			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t-theRect->t)+tmax;
			base = lightmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap (fa, base, BLOCK_WIDTH*lightmap_bytes);
		}
	}
}

/*
================
R_RenderDynamicLightmaps
Multitexture
================
*/
void R_RenderDynamicLightmaps (msurface_t *fa)
{
//    texture_t    *t;
	byte		*base;
	int			maps;
	glRect_t    *theRect;
	int smax, tmax;

	c_brush_polys++;

	if (fa->flags & ( SURF_DRAWSKY | SURF_DRAWTURB) )
		return;
		
	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ;
		 maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
dynamic:
		if (r_dynamic.value)
		{
			lightmap_modified[fa->lightmaptexturenum] = true;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];
			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			smax = (fa->extents[0]>>4)+1;
			tmax = (fa->extents[1]>>4)+1;
			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t-theRect->t)+tmax;
			base = lightmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap (fa, base, BLOCK_WIDTH*lightmap_bytes);
		}
	}
}

/*
================
R_MirrorChain
================
*/
void R_MirrorChain (msurface_t *s)
{
	if (mirror)
		return;
	mirror = true;
	mirror_plane = s->plane;
}


#if 0
/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	if (r_wateralpha.value == 1.0)
		return;

	//
	// go back to the world matrix
	//
    glLoadMatrixf (r_world_matrix);

	glEnable (GL_BLEND);
	glColor4f (1,1,1,r_wateralpha.value);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if ( !(s->flags & SURF_DRAWTURB) )
			continue;

		// set modulate mode explicitly
		GL_Bind (t->gl_texturenum);

		for ( ; s ; s=s->texturechain)
			R_RenderBrushPoly (s);

		t->texturechain = NULL;
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glColor4f (1,1,1,1);
	glDisable (GL_BLEND);
}
#else
/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	if (r_wateralpha.value == 1.0 && gl_texsort.value)
		return;

	if (r_wateralpha.value < 1.0)
    {
        GL_Use (gl_tintedpolygon1textureprogram);
        
		glEnable (GL_BLEND);

        glUniformMatrix4fv(gl_tintedpolygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
        glUniform4f(gl_tintedpolygon1textureprogram_color, 1.0, 1.0, 1.0, r_wateralpha.value);
        
        gl_waterpolygon_position = gl_tintedpolygon1textureprogram_position;
        gl_waterpolygon_texcoords = gl_tintedpolygon1textureprogram_texcoords;
	}
    else
    {
        GL_Use (gl_polygon1textureprogram);
        
        glUniformMatrix4fv(gl_polygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
        
        gl_waterpolygon_position = gl_polygon1textureprogram_position;
        gl_waterpolygon_texcoords = gl_polygon1textureprogram_texcoords;
    }

	if (!gl_texsort.value) {
		if (!waterchain)
			return;

		for ( s = waterchain ; s ; s=s->texturechain) {
			GL_Bind (s->texinfo->texture->gl_texturenum);
			EmitWaterPolys (s);
		}
		
		waterchain = NULL;
	} else {

		for (i=0 ; i<cl.worldmodel->numtextures ; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			s = t->texturechain;
			if (!s)
				continue;
			if ( !(s->flags & SURF_DRAWTURB ) )
				continue;

			// set modulate mode explicitly
			
			GL_Bind (t->gl_texturenum);

			for ( ; s ; s=s->texturechain)
				EmitWaterPolys (s);
			
			t->texturechain = NULL;
		}

	}

	if (r_wateralpha.value < 1.0)
    {
		glDisable (GL_BLEND);
	}
}

#endif

/*
================
DrawTextureChains
================
*/
void DrawTextureChains (void)
{
	int		i;
	msurface_t	*s;
	texture_t	*t;

	if (!gl_texsort.value) {
		GL_DisableMultitexture();

		if (skychain) {
			R_DrawSkyChain(skychain);
			skychain = NULL;
		}

		return;
	} 

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if (i == skytexturenum)
			R_DrawSkyChain (s);
		else if (i == mirrortexturenum && r_mirroralpha.value != 1.0)
		{
			R_MirrorChain (s);
			continue;
		}
		else
		{
			if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value != 1.0)
				continue;	// draw translucent water later
			for ( ; s ; s=s->texturechain)
				R_RenderBrushPoly (s);
		}

		t->texturechain = NULL;
	}
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
    int			k;//j, k;
	vec3_t		mins, maxs;
    int			i;//, numsurfaces;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;
	qboolean	rotated;

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	memset (lightmap_polys, 0, sizeof(lightmap_polys));

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend.value)
	{
		for (k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			R_MarkLights (&cl_dlights[k], 1<<k,
				clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e);
    R_ApplyProjection ();
e->angles[0] = -e->angles[0];	// stupid quake bug

    if (gl_mtexable)
    {
        GL_Use (gl_polygon2texturesprogram);
        
        glUniformMatrix4fv(gl_polygon2texturesprogram_transform, 1, 0, gl_polygon_matrix);
    }
    
    gl_batchmark = Hunk_LowMark ();
    
    gl_normal2texbatch = &gl_brushnormal2texbatch;
    gl_warped2texbatch = &gl_brushwarped2texbatch;
    
    R_BeginPolygon2Tex (gl_normal2texbatch);
    R_BeginPolygon2Tex (gl_warped2texbatch);
    
    //
	// draw texture
	//
	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
	// find which side of the node we are on
		pplane = psurf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

	// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (gl_texsort.value)
				R_RenderBrushPoly (psurf);
			else
				R_DrawSequentialPoly (psurf);
		}
	}

    R_EndPolygon2Tex (gl_warped2texbatch);
    R_EndPolygon2Tex (gl_normal2texbatch);
    
    Hunk_FreeToLowMark (gl_batchmark);
    
	R_BlendLightmaps ();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode (mnode_t *node)
{
    int            c, side;
//    int            i, c, side, *pindex;
//    vec3_t        acceptpt, rejectpt;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
    double        dot;
//    double        d, dot;
//    vec3_t        mins, maxs;

    if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;
	if (R_CullBox (node->minmaxs, node->minmaxs+3))
		return;
	
// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

	// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}

    // node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side]);

// draw stuff
	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < 0 -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;
		{
			for ( ; c ; c--, surf++)
			{
				if (surf->visframe != r_framecount)
					continue;

				// don't backface underwater surfaces, because they warp
				if ( !(surf->flags & SURF_UNDERWATER) && ( (dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)) )
					continue;		// wrong side

				// if sorting by texture, just store it out
				if (gl_texsort.value)
				{
					if (!mirror
					|| surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum])
					{
						surf->texturechain = surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					}
				} else if (surf->flags & SURF_DRAWSKY) {
					surf->texturechain = skychain;
					skychain = surf;
				} else if (surf->flags & SURF_DRAWTURB) {
					surf->texturechain = waterchain;
					waterchain = surf;
				} else
					R_DrawSequentialPoly (surf);

			}
		}

	}

// recurse down the back side
	R_RecursiveWorldNode (node->children[!side]);
}



/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	entity_t	ent;
//    int            i;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;
	currenttexture = -1;

	memset (lightmap_polys, 0, sizeof(lightmap_polys));
#ifdef QUAKE2
	R_ClearSkyBox ();
#endif

    if (gl_mtexable)
    {
        GL_Use (gl_polygon2texturesprogram);
        
        glUniformMatrix4fv(gl_polygon2texturesprogram_transform, 1, 0, gl_polygon_matrix);
    }
    
    gl_batchmark = Hunk_LowMark ();
    
    gl_normal2texbatch = &gl_worldnormal2texbatch;
    gl_warped2texbatch = &gl_worldwarped2texbatch;
    
    gl_worldnormal2texbatch.usevertexbuffer = true;

    R_BeginPolygon2Tex (gl_normal2texbatch);
    R_BeginPolygon2Tex (gl_warped2texbatch);

    R_RecursiveWorldNode (cl.worldmodel->nodes);
    
    R_EndPolygon2Tex (gl_warped2texbatch);
    R_EndPolygon2Tex (gl_normal2texbatch);

    Hunk_FreeToLowMark (gl_batchmark);

    DrawTextureChains ();

	R_BlendLightmaps ();

#ifdef QUAKE2
	R_DrawSkyBox ();
#endif
}


/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;
	byte	solid[4096];

	if (r_oldviewleaf == r_viewleaf && !r_novis.value)
		return;
	
	if (mirror)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value)
	{
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs+7)>>3);
	}
	else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);
		
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

// returns a texture number and the position inside it
int AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
//    int        bestx;
	int		texnum;

	for (texnum=0 ; texnum<MAX_LIGHTMAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (allocated[texnum][i+j] >= best)
					break;
				if (allocated[texnum][i+j] > best2)
					best2 = allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");

    return -1;
}


mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

int	nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList (msurface_t *fa)
{
    int			i, lindex, lnumverts;//, s_axis, t_axis;
//    float        dist, lastdist, lzi, scale, u, v, frac;
//    unsigned    mask;
//    vec3_t        local, transformed;
	medge_t		*pedges, *r_pedge;
//    mplane_t    *pplane;
    int			vertpage;//, newverts, newpage, lastvert;
//    qboolean    visible;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
	vertpage = 0;

	//
	// draw texture
	//
	poly = Hunk_Alloc (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s*16;
		s += 8;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t*16;
		t += 8;
		t /= BLOCK_HEIGHT*16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	//
	// remove co-linear points - Ed
	//
	if (!gl_keeptjunctions.value && !(fa->flags & SURF_UNDERWATER) )
	{
		for (i = 0 ; i < lnumverts ; ++i)
		{
			vec3_t v1, v2;
			float *prev, *this, *next;
//            float f;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			this = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract( this, prev, v1 );
			VectorNormalize( v1 );
			VectorSubtract( next, prev, v2 );
			VectorNormalize( v2 );

			// skip co-linear points
			#define COLINEAR_EPSILON 0.001
			if ((fabs( v1[0] - v2[0] ) <= COLINEAR_EPSILON) &&
				(fabs( v1[1] - v2[1] ) <= COLINEAR_EPSILON) && 
				(fabs( v1[2] - v2[2] ) <= COLINEAR_EPSILON))
			{
				int j;
				for (j = i + 1; j < lnumverts; ++j)
				{
					int k;
					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;

}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
    int		smax, tmax;//, s, t, l, i;
	byte	*base;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap (surf, base, BLOCK_WIDTH*lightmap_bytes);
}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps (void)
{
	int		i, j;
	model_t	*m;
	extern qboolean isPermedia;

	memset (allocated, 0, sizeof(allocated));

	r_framecount = 1;		// no dlightcache

	if (!lightmap_textures)
	{
        glGenTextures(MAX_LIGHTMAPS, &lightmap_textures);
	}

	gl_lightmap_format = GL_LUMINANCE;
	// default differently on the Permedia
	if (isPermedia)
		gl_lightmap_format = GL_RGBA;

	if (COM_CheckParm ("-lm_1"))
		gl_lightmap_format = GL_LUMINANCE;
	if (COM_CheckParm ("-lm_a"))
		gl_lightmap_format = GL_ALPHA;
	if (COM_CheckParm ("-lm_i"))
		gl_lightmap_format = GL_INTENSITY;
	if (COM_CheckParm ("-lm_2"))
		gl_lightmap_format = GL_RGBA4;
	if (COM_CheckParm ("-lm_4"))
		gl_lightmap_format = GL_RGBA;

	switch (gl_lightmap_format)
	{
	case GL_RGBA:
		lightmap_bytes = 4;
		break;
	case GL_RGBA4:
		lightmap_bytes = 2;
		break;
	case GL_LUMINANCE:
	//case GL_INTENSITY:  // Since we're defining GL_INTENSITY as GL_LUMINANCE, this no longer needs to be evaluated
	case GL_ALPHA:
		lightmap_bytes = 1;
		break;
	}

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;
		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i=0 ; i<m->numsurfaces ; i++)
		{
			GL_CreateSurfaceLightmap (m->surfaces + i);
			if ( m->surfaces[i].flags & SURF_DRAWTURB )
				continue;
#ifndef QUAKE2
			if ( m->surfaces[i].flags & SURF_DRAWSKY )
				continue;
#endif
			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

 	if (!gl_texsort.value)
 		GL_SelectTexture(GL_TEXTURE1);

	//
	// upload all lightmaps that were filled
	//
	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		if (!allocated[i][0])
			break;		// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		GL_Bind(lightmap_textures + i);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D, 0, gl_lightmap_format
		, BLOCK_WIDTH, BLOCK_HEIGHT, 0, 
		gl_lightmap_format, GL_UNSIGNED_BYTE, lightmaps+i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes);
	}

 	if (!gl_texsort.value)
 		GL_SelectTexture(GL_TEXTURE0);

}

