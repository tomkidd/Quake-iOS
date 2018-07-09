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
// r_main.c

#include "quakedef.h"

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap = false;				// true during envmap command capture

GLuint      currentprogram = 0;         // to avoid setting up the same program multiple times

int			currenttexture = -1;		// to avoid unnecessary texture sets

int			cnttextures[2] = {-1, -1};     // cached

GLuint		particletexture;	// little dot for particles
GLuint		playertextures;		// up to 16 color translated skins

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_rotate_matrix[16];
float	r_world_translate_matrix[16];
float	r_base_world_rotate_matrix[16];
float	r_base_world_translate_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves (void);

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
cvar_t	r_mirroralpha = {"r_mirroralpha","1"};
cvar_t	r_wateralpha = {"r_wateralpha","1"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};

cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_clear = {"gl_clear","0"};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_texsort = {"gl_texsort","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_flashblend = {"gl_flashblend","1"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","0"};
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t	gl_doubleeyes = {"gl_doubleeys", "1"};

extern	cvar_t	gl_ztrick;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

void R_ApplyWorld (void)
{
    memcpy (gl_polygon_matrix, r_world_rotate_matrix, sizeof(gl_polygon_matrix));

    GL_MultiplyLeft (gl_polygon_matrix, r_world_translate_matrix);

    if (glvr_enabled)
    {
        GL_MultiplyLeft (gl_polygon_matrix, glvr_eyetranslation);
    }
}

void R_RotateForEntity (entity_t *e)
{
    R_ApplyWorld ();
    
    GL_Translate(gl_polygon_matrix, e->origin[0],  e->origin[1],  e->origin[2]);
    
    GL_Rotate (gl_polygon_matrix, e->angles[1], 0.0, 0.0, 1.0);
    GL_Rotate (gl_polygon_matrix, -e->angles[0], 0.0, 1.0, 0.0);
    GL_Rotate (gl_polygon_matrix, e->angles[2], 1.0, 0.0, 0.0);
}

void R_ApplyProjection (void)
{
    // *** THIS ONE BREAKS SHIT
    if (glvr_enabled)
    {
        GL_MultiplyRight (glvr_projection, gl_polygon_matrix);
    }
    else
    {
        GL_MultiplyRight (gl_projection_matrix, gl_polygon_matrix);
    }
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	float		*up, *right;
	vec3_t		v_forward, v_right, v_up;
	msprite_t		*psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

    GL_Use (gl_alphapolygon1textureprogram);
    
    glUniformMatrix4fv(gl_alphapolygon1textureprogram_transform, 1, 0, gl_polygon_matrix);
    
    GL_DisableMultitexture();
    
    GL_Bind(frame->gl_texturenum);
    
    GLfloat vertices[20];
    
    VectorMA (e->origin, frame->down, up, point);
    VectorMA (point, frame->left, right, point);
    vertices[0] = point[0];
    vertices[1] = point[1];
    vertices[2] = point[2];
    vertices[3] = 0.0;
    vertices[4] = 1.0;

    VectorMA (e->origin, frame->up, up, point);
    VectorMA (point, frame->left, right, point);
    vertices[5] = point[0];
    vertices[6] = point[1];
    vertices[7] = point[2];
    vertices[8] = 0.0;
    vertices[9] = 0.0;

    VectorMA (e->origin, frame->up, up, point);
    VectorMA (point, frame->right, right, point);
    vertices[10] = point[0];
    vertices[11] = point[1];
    vertices[12] = point[2];
    vertices[13] = 1.0;
    vertices[14] = 0.0;

    VectorMA (e->origin, frame->down, up, point);
    VectorMA (point, frame->right, right, point);
    vertices[15] = point[0];
    vertices[16] = point[1];
    vertices[17] = point[2];
    vertices[18] = 1.0;
    vertices[19] = 1.0;
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_alphapolygon1textureprogram_position);
    glVertexAttribPointer(gl_alphapolygon1textureprogram_position, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(gl_alphapolygon1textureprogram_texcoords);
    glVertexAttribPointer(gl_alphapolygon1textureprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (const GLvoid *)(3 * sizeof(GLfloat)));
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(gl_alphapolygon1textureprogram_texcoords);
    glDisableVertexAttribArray(gl_alphapolygon1textureprogram_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
//    float    s, t;
	float 	l;
//    int        i, j;
//    int        index;
//    trivertx_t    *v, *verts;
    trivertx_t    *verts;
//    int        list;
	int		*order;
//    vec3_t    point;
//    float    *normal;
	int		count;

lastposenum = posenum;

    int mark = Hunk_LowMark ();
    
    int fansegmentcount = 0;
    int stripsegmentcount = 0;
    
	order = (int *)((byte *)paliashdr + paliashdr->commands);

    while (1)
    {
        count = *order++;
        if (!count)
            break;		// done
        if (count < 0)
        {
            fansegmentcount++;
            order -= count;
            order -= count;
        }
        else
        {
            stripsegmentcount++;
            order += count;
            order += count;
        }
    }

    GLsizei* fansegments = NULL;
    GLsizei* stripsegments = NULL;

    if (fansegmentcount > 0)
    {
        fansegments = Hunk_AllocName (fansegmentcount * sizeof(GLsizei), "fan_segments");
    }
    
    if (stripsegmentcount > 0)
    {
        stripsegments = Hunk_AllocName (stripsegmentcount * sizeof(GLsizei), "strip_segments");
    }

    int fansegmentpos = 0;
    int stripsegmentpos = 0;
    
    int fancount = 0;
    int stripcount = 0;

    order = (int *)((byte *)paliashdr + paliashdr->commands);
    
    while (1)
    {
        count = *order++;
        if (!count)
            break;		// done
        if (count < 0)
        {
            fansegments[fansegmentpos++] = -count;
            fancount -= count;
            order -= count;
            order -= count;
        }
        else
        {
            stripsegments[stripsegmentpos++] = count;
            stripcount += count;
            order += count;
            order += count;
        }
    }

    GLfloat* fanvertices = NULL;
    GLfloat* stripvertices = NULL;
    
    if (fancount > 0)
    {
        fanvertices = Hunk_AllocName (fancount * 6 * sizeof(GLfloat), "fan_vertices");
    }
    
    if (stripcount > 0)
    {
        stripvertices = Hunk_AllocName (stripcount * 6 * sizeof(GLfloat), "strip_vertices");
    }
    
    int fanvertexpos = 0;
    int stripvertexpos = 0;

    verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
    verts += posenum * paliashdr->poseverts;
    order = (int *)((byte *)paliashdr + paliashdr->commands);
    
	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
            do
            {
                // normals and vertexes come from the frame list
                fanvertices[fanvertexpos++] = verts->v[0];
                fanvertices[fanvertexpos++] = verts->v[1];
                fanvertices[fanvertexpos++] = verts->v[2];
                
                l = shadedots[verts->lightnormalindex] * shadelight;
                fanvertices[fanvertexpos++] = l;
                
                // texture coordinates come from the draw list
                fanvertices[fanvertexpos++] = ((float *)order)[0];
                fanvertices[fanvertexpos++] = ((float *)order)[1];
                
                order += 2;
                verts++;
            } while (++count);
		}
		else
        {
            do
            {
                // normals and vertexes come from the frame list
                stripvertices[stripvertexpos++] = verts->v[0];
                stripvertices[stripvertexpos++] = verts->v[1];
                stripvertices[stripvertexpos++] = verts->v[2];
                
                l = shadedots[verts->lightnormalindex] * shadelight;
                stripvertices[stripvertexpos++] = l;
                
                // texture coordinates come from the draw list
                stripvertices[stripvertexpos++] = ((float *)order)[0];
                stripvertices[stripvertexpos++] = ((float *)order)[1];
                
                order += 2;
                verts++;
            } while (--count);
        }
    }

    if (fancount > 0)
    {
        GLuint vertexbuffer;
        glGenBuffers(1, &vertexbuffer);
        
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glBufferData(GL_ARRAY_BUFFER, fancount * 6 * sizeof(GLfloat), fanvertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(gl_intensitypolygon1textureprogram_position);
        glVertexAttribPointer(gl_intensitypolygon1textureprogram_position, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const GLvoid *)0);
        glEnableVertexAttribArray(gl_intensitypolygon1textureprogram_intensity);
        glVertexAttribPointer(gl_intensitypolygon1textureprogram_intensity, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const GLvoid *)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(gl_intensitypolygon1textureprogram_texcoords);
        glVertexAttribPointer(gl_intensitypolygon1textureprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const GLvoid *)(4 * sizeof(GLfloat)));
        
        GLsizei offset = 0;
        for (int i = 0; i < fansegmentcount; i++)
        {
            GLsizei count = fansegments[i];
            glDrawArrays(GL_TRIANGLE_FAN, offset, count);
            offset += count;
        }
        
        glDisableVertexAttribArray(gl_intensitypolygon1textureprogram_texcoords);
        glDisableVertexAttribArray(gl_intensitypolygon1textureprogram_intensity);
        glDisableVertexAttribArray(gl_intensitypolygon1textureprogram_position);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glDeleteBuffers(1, &vertexbuffer);
    }

    if (stripcount > 0)
    {
        GLuint vertexbuffer;
        glGenBuffers(1, &vertexbuffer);
        
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glBufferData(GL_ARRAY_BUFFER, stripcount * 6 * sizeof(GLfloat), stripvertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(gl_intensitypolygon1textureprogram_position);
        glVertexAttribPointer(gl_intensitypolygon1textureprogram_position, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const GLvoid *)0);
        glEnableVertexAttribArray(gl_intensitypolygon1textureprogram_intensity);
        glVertexAttribPointer(gl_intensitypolygon1textureprogram_intensity, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const GLvoid *)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(gl_intensitypolygon1textureprogram_texcoords);
        glVertexAttribPointer(gl_intensitypolygon1textureprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (const GLvoid *)(4 * sizeof(GLfloat)));
        
        GLsizei offset = 0;
        for (int i = 0; i < stripsegmentcount; i++)
        {
            GLsizei count = stripsegments[i];
            glDrawArrays(GL_TRIANGLE_STRIP, offset, count);
            offset += count;
        }
        
        glDisableVertexAttribArray(gl_intensitypolygon1textureprogram_texcoords);
        glDisableVertexAttribArray(gl_intensitypolygon1textureprogram_intensity);
        glDisableVertexAttribArray(gl_intensitypolygon1textureprogram_position);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glDeleteBuffers(1, &vertexbuffer);
    }
    
    Hunk_FreeToLowMark (mark);
}


/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
//    float    s, t, l;
//    int        i, j;
//    int        index;
//    trivertx_t    *v, *verts;
    trivertx_t    *verts;
//    int        list;
	int		*order;
	vec3_t	point;
//    float    *normal;
	float	height, lheight;
	int		count;
    GLenum  elementtype;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
            elementtype = GL_TRIANGLE_FAN;
        }
        else
            elementtype = GL_TRIANGLE_STRIP;

        int mark = Hunk_LowMark ();
        
        int vertexcount = count;
        GLfloat* vertices = Hunk_AllocName (count * 3 * sizeof(GLfloat), "vertex_buffer");
        
        int vertexpos = 0;
		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
            
            vertices[vertexpos++] = point[0];
            vertices[vertexpos++] = point[1];
            vertices[vertexpos++] = point[2];

			verts++;
		} while (--count);

        GLuint vertexbuffer;
        glGenBuffers(1, &vertexbuffer);
        
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glBufferData(GL_ARRAY_BUFFER, vertexcount * 3 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(gl_polygonnotextureprogram_position);
        glVertexAttribPointer(gl_polygonnotextureprogram_position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const GLvoid *)0);
        
        glDrawArrays(elementtype, 0, vertexcount);
        
        glDisableVertexAttribArray(gl_polygonnotextureprogram_position);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glDeleteBuffers(1, &vertexbuffer);

        Hunk_FreeToLowMark (mark);
	}
}



/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose);
}



/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e)
{
    int			i;//, j;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
//    trivertx_t    *verts, *v;
//    int            index;
//    float        s, t, an;
    float        an;
	int			anim;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox (mins, maxs))
		return;


	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	//

	ambientlight = shadelight = R_LightPoint (currententity->origin);

	// allways give the gun some light
	if (e == &cl.viewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (currententity->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - Length(dist);

			if (add > 0) {
				ambientlight += add;
				//ZOID models should be affected by dlights as well
				shadelight += add;
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// ZOID: never allow players to go totally black
	i = (int)(currententity - cl_entities);
	if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
		if (ambientlight < 8)
			ambientlight = shadelight = 8;

	// HACK HACK HACK -- no fullbright colors, so make torches full light
	if (!strcmp (clmodel->name, "progs/flame2.mdl")
		|| !strcmp (clmodel->name, "progs/flame.mdl") )
		ambientlight = shadelight = 256;

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;
	
	an = e->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

    GL_Use (gl_intensitypolygon1textureprogram);
    
    GL_DisableMultitexture();

	R_RotateForEntity (e);

	if (!strcmp (clmodel->name, "progs/eyes.mdl") && gl_doubleeyes.value)
    {
        GL_Translate (gl_polygon_matrix, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
// double size of eyes, since they are really hard to see in gl
		GL_Scale (gl_polygon_matrix, paliashdr->scale[0]*2, paliashdr->scale[1]*2, paliashdr->scale[2]*2);
	}
    else
    {
		GL_Translate (gl_polygon_matrix, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		GL_Scale (gl_polygon_matrix, paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

    R_ApplyProjection ();

    glUniformMatrix4fv(gl_intensitypolygon1textureprogram_transform, 1, 0, gl_polygon_matrix);

    anim = (int)(cl.time*10) & 3;
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != vid.colormap && !gl_nocolors.value)
	{
        i = (int)(currententity - cl_entities);
		if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
		    GL_Bind(playertextures - 1 + i);
	}

	R_SetupAliasFrame (currententity->frame, paliashdr);

	if (r_shadows.value)
	{
        GL_Use (gl_polygonnotextureprogram);
        
		R_RotateForEntity (e);
        R_ApplyProjection ();
        
        glUniformMatrix4fv(gl_polygonnotextureprogram_transform, 1, 0, gl_polygon_matrix);
        
		glEnable (GL_BLEND);

        glUniform4f(gl_polygonnotextureprogram_color, 0.0, 0.0, 0.0, 0.5);

		GL_DrawAliasShadow (paliashdr, lastposenum);

		glDisable (GL_BLEND);
	}
}

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (currententity);
			break;

		case mod_brush:
			R_DrawBrushModel (currententity);
			break;

		default:
			break;
		}
	}

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
            case mod_brush:
                break;
            case mod_sprite:
                R_DrawSpriteModel (currententity);
                break;
            case mod_alias:
                break;
        }
	}

    // restore the world matrix
    R_ApplyWorld ();
    R_ApplyProjection ();
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	float		ambient[4], diffuse[4];
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	int			ambientlight, shadelight;

	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	j = R_LightPoint (currententity->origin);

	if (j < 24)
		j = 24;		// allways give some light on gun
	ambientlight = j;
	shadelight = j;

// add dynamic lights		
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if (add > 0)
			ambientlight += add;
	}

	ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float)ambientlight / 128;
	diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float)shadelight / 128;

	// hack the depth range to prevent view model from poking into walls
	glDepthRangef (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	R_DrawAliasModel (currententity);
	glDepthRangef (gldepthmin, gldepthmax);
    
    // restore the world matrix
    R_ApplyWorld ();
    R_ApplyProjection ();
}


/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!gl_polyblend.value)
		return;
	if (!v_blend[3])
		return;

	GL_DisableMultitexture();

    GL_Use (gl_polygonnotextureprogram);

	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);

    GLfloat transform[16];
    
    GL_Identity (transform);
    
    GL_Rotate (transform, -90.0,  1.0, 0.0, 0.0);	    // put Z going up
    GL_Rotate (transform, 90.0,  0.0, 0.0, 1.0);	    // put Z going up

    if (glvr_enabled)
    {
        GL_MultiplyRight (glvr_projection, transform);
    }
    else
    {
        GL_MultiplyRight (gl_projection_matrix, transform);
    }

    glUniformMatrix4fv(gl_polygonnotextureprogram_transform, 1, 0, transform);

    glUniform4fv(gl_polygonnotextureprogram_color, 1, v_blend);

    GLfloat vertices[12];
    
    vertices[0] = 10.0;
    vertices[1] = 100.0;
    vertices[2] = 100.0;

    vertices[3] = 10.0;
    vertices[4] = -100.0;
    vertices[5] = 100.0;

    vertices[6] = 10.0;
    vertices[7] = -100.0;
    vertices[8] = -100.0;

    vertices[9] = 10.0;
    vertices[10] = 100.0;
    vertices[11] = -100.0;
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_polygonnotextureprogram_position);
    glVertexAttribPointer(gl_polygonnotextureprogram_position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (const GLvoid *)0);
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(gl_polygonnotextureprogram_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);

	glDisable (GL_BLEND);
}


int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
//    int                edgecount;
//    vrect_t            vrect;
//    float            w, h;

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;

}


void MYgluPerspective( GLfloat fovy, GLfloat aspect,
		     GLfloat zNear, GLfloat zFar )
{
    GLfloat xmin, xmax, ymin, ymax;
    
    ymax = zNear * tan( fovy * M_PI / 360.0 );
    ymin = -ymax;
    
    xmin = ymin * aspect;
    xmax = ymax * aspect;

    GLfloat xdelta = xmax - xmin;
    GLfloat ydelta = ymax - ymin;
    GLfloat zdelta = zFar - zNear;
    
    gl_projection_matrix[0] = 2.0 * zNear / xdelta;
    gl_projection_matrix[1] = 0.0;
    gl_projection_matrix[2] = 0.0;
    gl_projection_matrix[3] = 0.0;
    
    gl_projection_matrix[4] = 0.0;
    gl_projection_matrix[5] = 2.0 * zNear / ydelta;
    gl_projection_matrix[6] = 0.0;
    gl_projection_matrix[7] = 0.0;
    
    gl_projection_matrix[8] = (xmax + xmin) / xdelta;
    gl_projection_matrix[9] = (ymax + ymin) / ydelta;
    gl_projection_matrix[10] = (zFar + zNear) / zdelta;
    gl_projection_matrix[11] = -1.0;
    
    gl_projection_matrix[12] = 0.0;
    gl_projection_matrix[13] = 0.0;
    gl_projection_matrix[14] = 2.0 * zFar * zNear / zdelta;
    gl_projection_matrix[15] = 0.0;
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
//    float    yfov;
//    int        i;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

    //
	// set up viewpoint
	//
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

    if (!glvr_enabled)
    {
        glViewport (glx + x, gly + y2, w, h);
    }

    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
//	yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
    MYgluPerspective (r_refdef.fov_y,  screenaspect,  4,  4096);

	if (mirror)
	{
		if (mirror_plane->normal[2])
			GL_Scale (gl_projection_matrix, 1.0, -1.0, 1.0);
		else
			GL_Scale (gl_projection_matrix, -1.0, 1.0, 1.0);
		glCullFace(GL_BACK);
	}
	else
		glCullFace(GL_FRONT);

    GL_Identity (r_world_rotate_matrix);

    GL_Rotate (r_world_rotate_matrix, -90.0,  1.0, 0.0, 0.0);	    // put Z going up
    GL_Rotate (r_world_rotate_matrix, 90.0,  0.0, 0.0, 1.0);	    // put Z going up
    GL_Rotate (r_world_rotate_matrix, -r_refdef.viewangles[2],  1.0, 0.0, 0.0);
    GL_Rotate (r_world_rotate_matrix, -r_refdef.viewangles[0],  0.0, 1.0, 0.0);
    GL_Rotate (r_world_rotate_matrix, -r_refdef.viewangles[1],  0.0, 0.0, 1.0);

    GL_Identity (r_world_translate_matrix);
    
    GL_Translate (r_world_translate_matrix, -r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

    R_ApplyWorld ();
    R_ApplyProjection ();

    //
	// set drawing parms
	//
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	GL_DisableMultitexture();

	R_RenderDlights ();

	R_DrawParticles ();

#ifdef GLTEST
	Test_Draw ();
#endif

}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if (r_mirroralpha.value != 1.0)
	{
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		glDepthFunc (GL_LEQUAL);
	}
	else if (gl_ztrick.value)
	{
		static int trickframe;

		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT);

        if (glvr_enabled)
        {
            if (glvr_eyeindex == 0)
            {
                trickframe++;
            }
        }
        else
        {
            trickframe++;
        }
        
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);
	}

	glDepthRangef (gldepthmin, gldepthmax);
}

/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{
	float		d;
	msurface_t	*s;
	entity_t	*ent;
    GLfloat     previous_world_rotate_matrix[16];
    GLfloat     previous_world_translate_matrix[16];

	if (!mirror)
		return;

    memcpy (previous_world_rotate_matrix, r_world_rotate_matrix, sizeof(previous_world_rotate_matrix));
    memcpy (previous_world_translate_matrix, r_world_translate_matrix, sizeof(previous_world_translate_matrix));
    
	memcpy (r_base_world_rotate_matrix, r_world_rotate_matrix, sizeof(r_base_world_rotate_matrix));
    memcpy (r_base_world_translate_matrix, r_world_translate_matrix, sizeof(r_base_world_translate_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	gldepthmin = 0.5;
	gldepthmax = 1;
	glDepthRangef (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	R_RenderScene ();
	R_DrawWaterSurfaces ();

	gldepthmin = 0;
	gldepthmax = 0.5;
	glDepthRangef (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	// blend on top
	glEnable (GL_BLEND);
	if (mirror_plane->normal[2])
		GL_Scale (gl_projection_matrix, 1.0, -1.0, 1.0);
	else
		GL_Scale (gl_projection_matrix, -1.0, 1.0, 1.0);
	glCullFace(GL_FRONT);

    memcpy (r_world_rotate_matrix, r_base_world_rotate_matrix, sizeof(r_world_rotate_matrix));
    memcpy (r_world_translate_matrix, r_base_world_translate_matrix, sizeof(r_world_translate_matrix));

    R_ApplyWorld ();
    R_ApplyProjection ();
    
    gl_rendermirror_enabled = true;
    gl_rendermirror_color[0] = 1.0;
    gl_rendermirror_color[1] = 1.0;
    gl_rendermirror_color[2] = 1.0;
    gl_rendermirror_color[3] = r_mirroralpha.value;

    s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for ( ; s ; s=s->texturechain)
		R_RenderBrushPoly (s);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;

    gl_rendermirror_enabled = true;
	
    glDisable (GL_BLEND);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
    double	time1 = 0.0, time2;
//    GLfloat colors[4] = {(GLfloat) 0.0, (GLfloat) 0.0, (GLfloat) 1, (GLfloat) 0.20};

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish ();
		time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

	if (gl_finish.value)
		glFinish ();

	R_Clear ();

	// render normal view

/***** Experimental silly looking fog ******
****** Use r_fullbright if you enable ******
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogfv(GL_FOG_COLOR, colors);
	glFogf(GL_FOG_END, 512.0);
	glEnable(GL_FOG);
********************************************/

	R_RenderScene ();
	R_DrawViewModel ();
	R_DrawWaterSurfaces ();

//  More fog right here :)
//	glDisable(GL_FOG);
//  End of all fog code...

	// render mirror view
	R_Mirror ();

	R_PolyBlend ();

	if (r_speeds.value)
	{
//		glFinish ();
		time2 = Sys_FloatTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys); 
	}
}
