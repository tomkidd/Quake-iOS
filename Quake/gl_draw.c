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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"

#define GL_COLOR_INDEX8_EXT     0x80E5

extern unsigned char d_15to8table[65536];

cvar_t		gl_nobind = {"gl_nobind", "0"};
cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t		gl_picmip = {"gl_picmip", "0"};

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

GLuint		translate_texture;
int			char_texture;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

byte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t		*conback = (qpic_t *)&conback_buffer;

int		gl_lightmap_format = GL_LUMINANCE;
int		gl_solid_format = GL_RGBA;
int		gl_alpha_format = GL_RGBA;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;


int		texels;

typedef struct
{
	GLuint	texnum;
	char	identifier[64];
	int		width, height;
	qboolean	mipmap;
} gltexture_t;

#define	MAX_GLTEXTURES	1024
gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

int gl_charsequencemark;

int gl_charsequencesegmenttop;

int gl_charsequencevertextop;

GLfloat* gl_charsequencevertices;

int gl_picsequencemark;

int gl_picsequencesegmenttop;

GLuint* gl_picsequencetexnames;

int gl_picsequencevertextop;

GLfloat* gl_picsequencevertices;

void GL_Use (GLuint program)
{
    if (currentprogram == program)
        return;
    currentprogram = program;
    glUseProgram(program);
}

void GL_Bind (int texnum)
{
	if (gl_nobind.value)
		texnum = char_texture;
	//if (currenttexture == texnum)
	//	return;
	currenttexture = texnum;
#ifdef _WIN32
	bindTexFunc (GL_TEXTURE_2D, texnum);
#else
	glBindTexture(GL_TEXTURE_2D, texnum);
#endif
}

void GL_Ortho (GLfloat* matrix, GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar)
{
    GLfloat ral = right + left;
    GLfloat rsl = right - left;
    GLfloat tab = top + bottom;
    GLfloat tsb = top - bottom;
    GLfloat fan = zFar + zNear;
    GLfloat fsn = zFar - zNear;
    
    matrix[0] = 2.0 / rsl;
    matrix[1] = 0.0;
    matrix[2] = 0.0;
    matrix[3] = 0.0;
    matrix[4] = 0.0;
    matrix[5] = 2.0 / tsb;
    matrix[6] = 0.0;
    matrix[7] = 0.0;
    matrix[8] = 0.0;
    matrix[9] = 0.0;
    matrix[10] = -2.0 / fsn;
    matrix[11] = 0.0;
    matrix[12] = -ral / rsl;
    matrix[13] = -tab / tsb;
    matrix[14] = -fan / fsn;
    matrix[15] = 1.0;
}

void GL_MultiplyResult (GLfloat* left, GLfloat *right, GLfloat* result)
{
    result[0] = left[0] * right[0] + left[4] * right[1] + left[8] * right[2] + left[12] * right[3];
    result[1] = left[1] * right[0] + left[5] * right[1] + left[9] * right[2] + left[13] * right[3];
    result[2] = left[2] * right[0] + left[6] * right[1] + left[10] * right[2] + left[14] * right[3];
    result[3] = left[3] * right[0] + left[7] * right[1] + left[11] * right[2] + left[15] * right[3];
    result[4] = left[0] * right[4] + left[4] * right[5] + left[8] * right[6] + left[12] * right[7];
    result[5] = left[1] * right[4] + left[5] * right[5] + left[9] * right[6] + left[13] * right[7];
    result[6] = left[2] * right[4] + left[6] * right[5] + left[10] * right[6] + left[14] * right[7];
    result[7] = left[3] * right[4] + left[7] * right[5] + left[11] * right[6] + left[15] * right[7];
    result[8] = left[0] * right[8] + left[4] * right[9] + left[8] * right[10] + left[12] * right[11];
    result[9] = left[1] * right[8] + left[5] * right[9] + left[9] * right[10] + left[13] * right[11];
    result[10] = left[2] * right[8] + left[6] * right[9] + left[10] * right[10] + left[14] * right[11];
    result[11] = left[3] * right[8] + left[7] * right[9] + left[11] * right[10] + left[15] * right[11];
    result[12] = left[0] * right[12] + left[4] * right[13] + left[8] * right[14] + left[12] * right[15];
    result[13] = left[1] * right[12] + left[5] * right[13] + left[9] * right[14] + left[13] * right[15];
    result[14] = left[2] * right[12] + left[6] * right[13] + left[10] * right[14] + left[14] * right[15];
    result[15] = left[3] * right[12] + left[7] * right[13] + left[11] * right[14] + left[15] * right[15];
}

void GL_MultiplyLeft (GLfloat* left, GLfloat *right)
{
    GLfloat result[16];

    GL_MultiplyResult (left, right, result);
    
    memcpy(left, result, sizeof(result));
}

void GL_MultiplyRight (GLfloat* left, GLfloat *right)
{
    GLfloat result[16];
    
    GL_MultiplyResult (left, right, result);

    memcpy(right, result, sizeof(result));
}

void GL_Identity (GLfloat* matrix)
{
    matrix[0] = 1.0;
    matrix[1] = 0.0;
    matrix[2] = 0.0;
    matrix[3] = 0.0;
    matrix[4] = 0.0;
    matrix[5] = 1.0;
    matrix[6] = 0.0;
    matrix[7] = 0.0;
    matrix[8] = 0.0;
    matrix[9] = 0.0;
    matrix[10] = 1.0;
    matrix[11] = 0.0;
    matrix[12] = 0.0;
    matrix[13] = 0.0;
    matrix[14] = 0.0;
    matrix[15] = 1.0;
}

void GL_Translate (GLfloat* matrix, GLfloat x, GLfloat y, GLfloat z)
{
    matrix[12] += matrix[0] * x + matrix[4] * y + matrix[8] * z;
    matrix[13] += matrix[1] * x + matrix[5] * y + matrix[9] * z;
    matrix[14] += matrix[2] * x + matrix[6] * y + matrix[10] * z;
    matrix[15] += matrix[3] * x + matrix[7] * y + matrix[11] * z;
}

void GL_Rotate (GLfloat* matrix, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat angleInRadians = angle * M_PI / 180.0;
    
    double angleSin = sin(angleInRadians);
    double angleCos = cos(angleInRadians);
    
    double inverseAngleCos = 1.0 - angleCos;
    
    GLfloat rotate[16];
    
    rotate[0] = x * x * inverseAngleCos + angleCos;
    rotate[1] = y * x * inverseAngleCos + z * angleSin;
    rotate[2] = z * x * inverseAngleCos - y * angleSin;
    rotate[3] = 0.0;
    rotate[4] = x * y * inverseAngleCos - z * angleSin;
    rotate[5] = y * y * inverseAngleCos + angleCos;
    rotate[6] = z * y * inverseAngleCos + x * angleSin;
    rotate[7] = 0.0;
    rotate[8] = x * z * inverseAngleCos + y * angleSin;
    rotate[9] = y * z * inverseAngleCos - x * angleSin;
    rotate[10] = z * z * inverseAngleCos + angleCos;
    rotate[11] = 0.0;
    rotate[12] = 0.0;
    rotate[13] = 0.0;
    rotate[14] = 0.0;
    rotate[15] = 1.0;
    
    GL_MultiplyLeft (matrix, rotate);
}

void GL_Scale (GLfloat* matrix, GLfloat x, GLfloat y, GLfloat z)
{
    matrix[0] *= x;
    matrix[1] *= x;
    matrix[2] *= x;
    matrix[3] *= x;
    
    matrix[4] *= y;
    matrix[5] *= y;
    matrix[6] *= y;
    matrix[7] *= y;

    matrix[8] *= z;
    matrix[9] *= z;
    matrix[10] *= z;
    matrix[11] *= z;
}

void GL_Triangulate (GLfloat* vertices, int vertexcount, int stride, GLuint** indices, int* indexcount)
{
    // TODO: Fix this for real. This is just triangle-fanning, and it is completely wrong.

    (*indexcount) = (vertexcount - 2) * 3;
    (*indices) = Hunk_AllocName((*indexcount) * sizeof(GLuint), "index_buffer");
    
    int indexPos = 0;
    
    (*indices)[indexPos++] = 0;
    (*indices)[indexPos++] = 1;
    (*indices)[indexPos++] = 2;
    
    for (int i = 3; i < vertexcount; i++)
    {
        (*indices)[indexPos++] = 0;
        (*indices)[indexPos++] = i - 1;
        (*indices)[indexPos++] = i;
    }
}

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT*4];
qboolean	scrap_dirty;
GLuint		scrap_texnum;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
//    int        bestx;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
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
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("Scrap_AllocBlock: full");
    return -1;
}

int	scrap_uploads;

void Scrap_Upload (void)
{
	int		texnum;

	scrap_uploads++;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++) {
		GL_Bind(scrap_texnum + texnum);
		GL_Upload8 (scrap_texels[texnum], BLOCK_WIDTH, BLOCK_HEIGHT, false, true);
	}
	scrap_dirty = false;
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;

qpic_t *Draw_PicFromWad (char *name)
{
	qpic_t	*p;
	glpic_t	*gl;

	p = W_GetLumpName (name);
	gl = (glpic_t *)p->data;

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		texnum = Scrap_AllocBlock (p->width, p->height, &x, &y);
		scrap_dirty = true;
		k = 0;
		for (i=0 ; i<p->height ; i++)
			for (j=0 ; j<p->width ; j++, k++)
				scrap_texels[texnum][(y+i)*BLOCK_WIDTH + x + j] = p->data[k];
		texnum += scrap_texnum;
		gl->texnum = texnum;
		gl->sl = (x+0.01)/(float)BLOCK_WIDTH;
		gl->sh = (x+p->width-0.01)/(float)BLOCK_WIDTH;
		gl->tl = (y+0.01)/(float)BLOCK_WIDTH;
		gl->th = (y+p->height-0.01)/(float)BLOCK_WIDTH;

		pic_count++;
		pic_texels += p->width*p->height;
	}
	else
	{
		gl->texnum = GL_LoadPicTexture (p);
		gl->sl = 0;
		gl->sh = 1;
		gl->tl = 0;
		gl->th = 1;
	}
	return p;
}


/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;

	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, path);

//
// load the pic from disk
//
	dat = (qpic_t *)COM_LoadTempFile (path);	
	if (!dat)
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->texnum = GL_LoadPicTexture (dat);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;

	return &pic->pic;
}


void Draw_CharToConback (int num, byte *dest)
{
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x] != 255)
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
void Draw_TextureMode_f (void)
{
	int		i;
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< 6 ; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i=0 ; i< 6 ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	}
	if (i == 6)
	{
		Con_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->mipmap)
		{
			GL_Bind (glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	int		i;
	qpic_t	*cb;
    byte	*dest;//, *src;
	int		x, y;
	char	ver[40];
	glpic_t	*gl;
	int		start;
	byte	*ncdata;
//    int        f, fstep;


	Cvar_RegisterVariable (&gl_nobind);
	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);

	// 3dfx can only handle 256 wide textures
	if (!Q_strncasecmp ((char *)gl_renderer, "3dfx",4) ||
		strstr((char *)gl_renderer, "Glide"))
		Cvar_Set ("gl_max_size", "256");

	Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f);

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = W_GetLumpName ("conchars");
	for (i=0 ; i<256*64 ; i++)
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;	// proper transparent color

	// now turn them into textures
	char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true);

	start = Hunk_LowMark();

	cb = (qpic_t *)COM_LoadTempFile ("gfx/conback.lmp");	
	if (!cb)
		Sys_Error ("Couldn't load gfx/conback.lmp");
	SwapPic (cb);

	// hack the version number directly into the pic
#if defined(__linux__)
	sprintf (ver, "(Linux %2.2f, gl %4.2f) %4.2f", (float)LINUX_VERSION, (float)GLQUAKE_VERSION, (float)VERSION);
#else
	sprintf (ver, "(gl %4.2f) %4.2f", (float)GLQUAKE_VERSION, (float)VERSION);
#endif
	dest = cb->data + 320*186 + 320 - 11 - 8*strlen(ver);
	y = (int)strlen(ver);
	for (x=0 ; x<y ; x++)
		Draw_CharToConback (ver[x], dest+(x<<3));

#if 0
	conback->width = vid.conwidth;
	conback->height = vid.conheight;

 	// scale console to vid size
 	dest = ncdata = Hunk_AllocName(vid.conwidth * vid.conheight, "conback");
 
 	for (y=0 ; y<vid.conheight ; y++, dest += vid.conwidth)
 	{
 		src = cb->data + cb->width * (y*cb->height/vid.conheight);
 		if (vid.conwidth == cb->width)
 			memcpy (dest, src, vid.conwidth);
 		else
 		{
 			f = 0;
 			fstep = cb->width*0x10000/vid.conwidth;
 			for (x=0 ; x<vid.conwidth ; x+=4)
 			{
 				dest[x] = src[f>>16];
 				f += fstep;
 				dest[x+1] = src[f>>16];
 				f += fstep;
 				dest[x+2] = src[f>>16];
 				f += fstep;
 				dest[x+3] = src[f>>16];
 				f += fstep;
 			}
 		}
 	}
#else
	conback->width = cb->width;
	conback->height = cb->height;
	ncdata = cb->data;
#endif

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	gl = (glpic_t *)conback->data;
	gl->texnum = GL_LoadTexture ("conback", conback->width, conback->height, ncdata, false, false);
	gl->sl = 0;
	gl->sh = 1;
	gl->tl = 0;
	gl->th = 1;
	conback->width = vid.width;
	conback->height = vid.height;

	// free loaded console
	Hunk_FreeToLowMark(start);

	// save a texture slot for translated picture
    glGenTextures(1, &translate_texture);

	// save slots for scraps
	glGenTextures(MAX_SCRAPS, &scrap_texnum);

	//
	// get the other pics we need
	//
	draw_disc = Draw_PicFromWad ("disc");
	draw_backtile = Draw_PicFromWad ("backtile");
}



/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
//    byte            *dest;
//    byte            *source;
//    unsigned short    *pusdest;
//    int                drawline;    
	int				row, col;
	float			frow, fcol, size;

	if (num == 32)
		return;		// space

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;

    GL_Use (gl_noalphatextprogram);
    
	GL_Bind (char_texture);

    GLfloat vertices[16];

    vertices[0] = x;
    vertices[1] = y;
    vertices[2] = fcol;
    vertices[3] = frow;
    
    vertices[4] = x + 8;
    vertices[5] = y;
    vertices[6] = fcol + size;
    vertices[7] = frow;
    
    vertices[8] = x + 8;
    vertices[9] = y + 8;
    vertices[10] = fcol + size;
    vertices[11] = frow + size;

    vertices[12] = x;
    vertices[13] = y + 8;
    vertices[14] = fcol;
    vertices[15] = frow + size;

    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_noalphatextprogram_position);
    glVertexAttribPointer(gl_noalphatextprogram_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(gl_noalphatextprogram_texcoords);
    glVertexAttribPointer(gl_noalphatextprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)(2 * sizeof(GLfloat)));

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(gl_noalphatextprogram_texcoords);
    glDisableVertexAttribArray(gl_noalphatextprogram_position);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);
}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
    int charcount = (int)strlen(str);
    
    Draw_BeginCharSequence (charcount);
    
	while (*str)
	{
		Draw_CharInSequence (x, y, *str);
		str++;
		x += 8;
	}
    
    Draw_EndCharSequence ();
}

/*
================
Draw_BeginCharSequence
================
*/
void Draw_BeginCharSequence (int count)
{
    gl_charsequencemark = Hunk_LowMark ();

    gl_charsequencevertices = Hunk_AllocName (count * 4 * 4 * sizeof(GLfloat), "char_sequence");

    gl_charsequencevertextop = 0;
    gl_charsequencesegmenttop = 0;
}

/*
================
Draw_CharInSequence
================
*/
void Draw_CharInSequence (int x, int y, int num)
{
    int				row, col;
    float			frow, fcol, size;
    
    if (num == 32)
        return;		// space
    
    num &= 255;
    
    if (y <= -8)
        return;			// totally off screen
    
    row = num>>4;
    col = num&15;
    
    frow = row*0.0625;
    fcol = col*0.0625;
    size = 0.0625;
    
    gl_charsequencevertices[gl_charsequencevertextop++] = x;
    gl_charsequencevertices[gl_charsequencevertextop++] = y;
    gl_charsequencevertices[gl_charsequencevertextop++] = fcol;
    gl_charsequencevertices[gl_charsequencevertextop++] = frow;
    
    gl_charsequencevertices[gl_charsequencevertextop++] = x + 8;
    gl_charsequencevertices[gl_charsequencevertextop++] = y;
    gl_charsequencevertices[gl_charsequencevertextop++] = fcol + size;
    gl_charsequencevertices[gl_charsequencevertextop++] = frow;
    
    gl_charsequencevertices[gl_charsequencevertextop++] = x + 8;
    gl_charsequencevertices[gl_charsequencevertextop++] = y + 8;
    gl_charsequencevertices[gl_charsequencevertextop++] = fcol + size;
    gl_charsequencevertices[gl_charsequencevertextop++] = frow + size;
    
    gl_charsequencevertices[gl_charsequencevertextop++] = x;
    gl_charsequencevertices[gl_charsequencevertextop++] = y + 8;
    gl_charsequencevertices[gl_charsequencevertextop++] = fcol;
    gl_charsequencevertices[gl_charsequencevertextop++] = frow + size;
    
    gl_charsequencesegmenttop++;
}

/*
================
Draw_StringInSequence
================
*/
void Draw_StringInSequence (int x, int y, char *str)
{
    while (*str)
    {
        Draw_CharInSequence (x, y, *str);
        str++;
        x += 8;
    }
}

/*
================
Draw_EndCharSequence
================
*/
void Draw_EndCharSequence (void)
{
    if (gl_charsequencevertextop > 0)
    {
        GL_Use (gl_noalphatextprogram);
        
        GL_Bind (char_texture);
        
        GLuint vertexbuffer;
        glGenBuffers(1, &vertexbuffer);
        
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glBufferData(GL_ARRAY_BUFFER, gl_charsequencevertextop * sizeof(GLfloat), gl_charsequencevertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(gl_noalphatextprogram_position);
        glVertexAttribPointer(gl_noalphatextprogram_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)0);
        glEnableVertexAttribArray(gl_noalphatextprogram_texcoords);
        glVertexAttribPointer(gl_noalphatextprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)(2 * sizeof(GLfloat)));
        
        GLsizei offset = 0;
        for (int i = 0; i < gl_charsequencesegmenttop; i++)
        {
            glDrawArrays(GL_TRIANGLE_FAN, offset, 4);
            offset += 4;
        }
        
        glDisableVertexAttribArray(gl_noalphatextprogram_texcoords);
        glDisableVertexAttribArray(gl_noalphatextprogram_position);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glDeleteBuffers(1, &vertexbuffer);
    }

    Hunk_FreeToLowMark (gl_charsequencemark);
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
}

/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
//    byte            *dest, *source;
//    unsigned short    *pusdest;
//    int                v, u;
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	glEnable (GL_BLEND);

    GL_Use (gl_textprogram);
    
    glUniform4f(gl_textprogram_color, 1.0, 1.0, 1.0, alpha);
    
    GL_Bind (gl->texnum);
    
    GLfloat vertices[16];
    
    vertices[0] = x;
    vertices[1] = y;
    vertices[2] = gl->sl;
    vertices[3] = gl->tl;
    
    vertices[4] = x + pic->width;
    vertices[5] = y;
    vertices[6] = gl->sh;
    vertices[7] = gl->tl;
    
    vertices[8] = x + pic->width;
    vertices[9] = y + pic->height;
    vertices[10] = gl->sh;
    vertices[11] = gl->th;
    
    vertices[12] = x;
    vertices[13] = y + pic->height;
    vertices[14] = gl->sl;
    vertices[15] = gl->th;
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_textprogram_position);
    glVertexAttribPointer(gl_textprogram_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(gl_textprogram_texcoords);
    glVertexAttribPointer(gl_textprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)(2 * sizeof(GLfloat)));
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(gl_textprogram_texcoords);
    glDisableVertexAttribArray(gl_textprogram_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glUniform4f(gl_textprogram_color, 1.0, 1.0, 1.0, 1.0);
    
    glDeleteBuffers(1, &vertexbuffer);
    
    glDisable (GL_BLEND);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
//    byte            *dest, *source;
//    unsigned short    *pusdest;
//    int                v, u;
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;

    GL_Use (gl_noalphatextprogram);
    
    glUniform4f(gl_noalphatextprogram_color, 1.0, 1.0, 1.0, 1.0);
    
    GL_Bind (gl->texnum);
    
    GLfloat vertices[16];
    
    vertices[0] = x;
    vertices[1] = y;
    vertices[2] = gl->sl;
    vertices[3] = gl->tl;
    
    vertices[4] = x + pic->width;
    vertices[5] = y;
    vertices[6] = gl->sh;
    vertices[7] = gl->tl;
    
    vertices[8] = x + pic->width;
    vertices[9] = y + pic->height;
    vertices[10] = gl->sh;
    vertices[11] = gl->th;
    
    vertices[12] = x;
    vertices[13] = y + pic->height;
    vertices[14] = gl->sl;
    vertices[15] = gl->th;
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_noalphatextprogram_position);
    glVertexAttribPointer(gl_noalphatextprogram_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(gl_noalphatextprogram_texcoords);
    glVertexAttribPointer(gl_noalphatextprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)(2 * sizeof(GLfloat)));
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(gl_noalphatextprogram_texcoords);
    glDisableVertexAttribArray(gl_noalphatextprogram_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
//    byte    *dest, *source, tbyte;
//    unsigned short    *pusdest;
//    int                v, u;

	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	Draw_Pic (x, y, pic);
}


/*
 ================
 Draw_BeginPicSequence
 ================
 */
void Draw_BeginPicSequence (int count)
{
    gl_picsequencemark = Hunk_LowMark ();
    
    gl_picsequencevertices = Hunk_AllocName (count * 4 * 4 * sizeof(GLfloat), "pic_sequence_vertices");
    
    gl_picsequencetexnames = Hunk_AllocName (count * sizeof(GLuint), "pic_sequence_texnames");
    
    gl_picsequencevertextop = 0;
    gl_picsequencesegmenttop = 0;
}

/*
 ================
 Draw_PicInSequence
 ================
 */
void Draw_PicInSequence (int x, int y, qpic_t *pic)
{
    glpic_t			*gl;
    
    if (scrap_dirty)
        Scrap_Upload ();
    gl = (glpic_t *)pic->data;
    
    GL_Bind (gl->texnum);
    
    gl_picsequencetexnames[gl_picsequencesegmenttop++] = gl->texnum;
    
    gl_picsequencevertices[gl_picsequencevertextop++] = x;
    gl_picsequencevertices[gl_picsequencevertextop++] = y;
    gl_picsequencevertices[gl_picsequencevertextop++] = gl->sl;
    gl_picsequencevertices[gl_picsequencevertextop++] = gl->tl;
    
    gl_picsequencevertices[gl_picsequencevertextop++] = x + pic->width;
    gl_picsequencevertices[gl_picsequencevertextop++] = y;
    gl_picsequencevertices[gl_picsequencevertextop++] = gl->sh;
    gl_picsequencevertices[gl_picsequencevertextop++] = gl->tl;
    
    gl_picsequencevertices[gl_picsequencevertextop++] = x + pic->width;
    gl_picsequencevertices[gl_picsequencevertextop++] = y + pic->height;
    gl_picsequencevertices[gl_picsequencevertextop++] = gl->sh;
    gl_picsequencevertices[gl_picsequencevertextop++] = gl->th;
    
    gl_picsequencevertices[gl_picsequencevertextop++] = x;
    gl_picsequencevertices[gl_picsequencevertextop++] = y + pic->height;
    gl_picsequencevertices[gl_picsequencevertextop++] = gl->sl;
    gl_picsequencevertices[gl_picsequencevertextop++] = gl->th;
}

/*
 ================
 Draw_TransPicInSequence
 ================
 */
void Draw_TransPicInSequence (int x, int y, qpic_t *pic)
{
    if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
        (unsigned)(y + pic->height) > vid.height)
    {
        Sys_Error ("Draw_TransPicInSequence: bad coordinates");
    }
    
    Draw_PicInSequence (x, y, pic);
}

/*
 ================
 Draw_EndPicSequence
 ================
 */
void Draw_EndPicSequence (void)
{
    if (gl_picsequencevertextop > 0)
    {
        GL_Use (gl_noalphatextprogram);
        
        glUniform4f(gl_noalphatextprogram_color, 1.0, 1.0, 1.0, 1.0);
        
        GLuint vertexbuffer;
        glGenBuffers(1, &vertexbuffer);
        
        glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
        glBufferData(GL_ARRAY_BUFFER, gl_picsequencevertextop * sizeof(GLfloat), gl_picsequencevertices, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(gl_noalphatextprogram_position);
        glVertexAttribPointer(gl_noalphatextprogram_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)0);
        glEnableVertexAttribArray(gl_noalphatextprogram_texcoords);
        glVertexAttribPointer(gl_noalphatextprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)(2 * sizeof(GLfloat)));
        
        GLsizei offset = 0;
        for (int i = 0; i < gl_picsequencesegmenttop; i++)
        {
            GL_Bind (gl_picsequencetexnames[i]);
            
            glDrawArrays(GL_TRIANGLE_FAN, offset, 4);
            offset += 4;
        }
        
        glDisableVertexAttribArray(gl_noalphatextprogram_texcoords);
        glDisableVertexAttribArray(gl_noalphatextprogram_position);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        glDeleteBuffers(1, &vertexbuffer);
    }
    
    Hunk_FreeToLowMark (gl_picsequencemark);
}

/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	byte			*src;
	int				p;

    GL_Use (gl_noalphatextprogram);

    GL_Bind (translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24table[translation[p]];
		}
	}

	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glUniform4f(gl_noalphatextprogram_color, 1.0, 1.0, 1.0, 1.0);

    GLfloat vertices[16];
    
    vertices[0] = x;
    vertices[1] = y;
    vertices[2] = 0.0;
    vertices[3] = 0.0;
    
    vertices[4] = x + pic->width;
    vertices[5] = y;
    vertices[6] = 1.0;
    vertices[7] = 0.0;
    
    vertices[8] = x + pic->width;
    vertices[9] = y + pic->height;
    vertices[10] = 1.0;
    vertices[11] = 1.0;
    
    vertices[12] = x;
    vertices[13] = y + pic->height;
    vertices[14] = 0.0;
    vertices[15] = 1.0;
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_noalphatextprogram_position);
    glVertexAttribPointer(gl_noalphatextprogram_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(gl_noalphatextprogram_texcoords);
    glVertexAttribPointer(gl_noalphatextprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)(2 * sizeof(GLfloat)));
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(gl_noalphatextprogram_texcoords);
    glDisableVertexAttribArray(gl_noalphatextprogram_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	int y = (vid.height * 3) >> 2;

	if (lines > y)
		Draw_Pic(0, lines - vid.height, conback);
	else
		Draw_AlphaPic (0, lines - vid.height, conback, (float)(1.2 * lines)/y);
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
    GL_Use (gl_noalphatextprogram);
    
    glUniform4f(gl_noalphatextprogram_color, 1.0, 1.0, 1.0, 1.0);
    
    GL_Bind (*(int *)draw_backtile->data);
    
    GLfloat vertices[16];
    
    vertices[0] = x;
    vertices[1] = y;
    vertices[2] = x / 64.0;
    vertices[3] = y / 64.0;
    
    vertices[4] = x + w;
    vertices[5] = y;
    vertices[6] = (x + w) / 64.0;
    vertices[7] = y / 64.0;
    
    vertices[8] = x + w;
    vertices[9] = y + h;
    vertices[10] = (x + w) / 64.0;
    vertices[11] = (y + h) / 64.0;
    
    vertices[12] = x;
    vertices[13] = y + h;
    vertices[14] = x / 64.0;
    vertices[15] = (y + h) / 64.0;
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_noalphatextprogram_position);
    glVertexAttribPointer(gl_noalphatextprogram_position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)0);
    glEnableVertexAttribArray(gl_noalphatextprogram_texcoords);
    glVertexAttribPointer(gl_noalphatextprogram_texcoords, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (const GLvoid *)(2 * sizeof(GLfloat)));
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(gl_noalphatextprogram_texcoords);
    glDisableVertexAttribArray(gl_noalphatextprogram_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
    GL_Use (gl_fillprogram);
    
    glUniform4f(gl_fillprogram_color, host_basepal[c*3]/255.0, host_basepal[c*3+1]/255.0, host_basepal[c*3+2]/255.0, 1.0);
    
    GLfloat vertices[8];
    
    vertices[0] = x;
    vertices[1] = y;
    vertices[2] = x + w;
    vertices[3] = y;
    vertices[4] = x + w;
    vertices[5] = y + h;
    vertices[6] = x;
    vertices[7] = y + h;
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_fillprogram_position);
    glVertexAttribPointer(gl_fillprogram_position, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const GLvoid *)0);
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(gl_fillprogram_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);

    glUniform4f(gl_fillprogram_color, 1.0, 1.0, 1.0, 1.0);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
    // note: glvr_enabled exited this function immediately. Don't know why. -tkidd

    glEnable(GL_BLEND);
    
    GL_Use (gl_fillprogram);
    
    glUniform4f(gl_fillprogram_color, 0.0, 0.0, 0.0, 0.8);
    
    GLfloat vertices[8];
    
    vertices[0] = 0.0;
    vertices[1] = 0.0;
    vertices[2] = vid.width;
    vertices[3] = 0.0;
    vertices[4] = vid.width;
    vertices[5] = vid.height;
    vertices[6] = 0.0;
    vertices[7] = vid.height;
    
    GLuint vertexbuffer;
    glGenBuffers(1, &vertexbuffer);
    
    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glEnableVertexAttribArray(gl_fillprogram_position);
    glVertexAttribPointer(gl_fillprogram_position, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (const GLvoid *)0);
    
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisableVertexAttribArray(gl_fillprogram_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDeleteBuffers(1, &vertexbuffer);
    
    glUniform4f(gl_fillprogram_color, 1.0, 1.0, 1.0, 1.0);

    glDisable(GL_BLEND);

    Sbar_Changed();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc (void)
{
}


/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc (void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
    GL_Identity (gl_textandfill_matrix);
    
    GL_Translate (gl_textandfill_matrix, -2.0, 1.2, 0.0);
    GL_Scale (gl_textandfill_matrix, 4.0 / vid.width, -4.0 / vid.height, 1.0);
    
    GL_MultiplyLeft (gl_textandfill_matrix, gl_translation);
    
    GL_MultiplyRight (gl_projection, gl_textandfill_matrix);
    
    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glDisable (GL_BLEND);

    GL_Use (gl_textprogram);
    
    glUniform4f(gl_textprogram_color, 1.0, 1.0, 1.0, 1.0);
    glUniformMatrix4fv(gl_textprogram_transform, 1, 0, gl_textandfill_matrix);
    
    GL_Use (gl_noalphatextprogram);
    
    glUniform4f(gl_noalphatextprogram_color, 1.0, 1.0, 1.0, 1.0);
    glUniformMatrix4fv(gl_noalphatextprogram_transform, 1, 0, gl_textandfill_matrix);
    
    GL_Use (gl_fillprogram);

    glUniform4f(gl_fillprogram_color, 1.0, 1.0, 1.0, 1.0);
    glUniformMatrix4fv(gl_fillprogram_transform, 1, 0, gl_textandfill_matrix);
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (!strcmp (identifier, glt->identifier))
			return gltextures[i].texnum;
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

/*
================
GL_Resample8BitTexture -- JACK
================
*/
void GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	char *inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}


/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
================
GL_MipMap8Bit

Mipping for 8 bit textures
================
*/
void GL_MipMap8Bit (byte *in, int width, int height)
{
	int		i, j;
	unsigned short     r,g,b;
	byte	*out, *at1, *at2, *at3, *at4;

//	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=2, out+=1, in+=2)
		{
			at1 = (byte *) (d_8to24table + in[0]);
			at2 = (byte *) (d_8to24table + in[1]);
			at3 = (byte *) (d_8to24table + in[width+0]);
			at4 = (byte *) (d_8to24table + in[width+1]);

 			r = (at1[0]+at2[0]+at3[0]+at4[0]); r>>=5;
 			g = (at1[1]+at2[1]+at3[1]+at4[1]); g>>=5;
 			b = (at1[2]+at2[2]+at3[2]+at4[2]); b>>=5;

			out[0] = d_15to8table[(r<<0) + (g<<5) + (b<<10)];
		}
	}
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			samples;
static	unsigned	scaled[1024*512];	// [512*256];
	int			scaled_width, scaled_height;

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_LoadTexture: too big");

	samples = alpha ? gl_alpha_format : gl_solid_format;

#if 0
	if (mipmap)
		gluBuild2DMipmaps (GL_TEXTURE_2D, samples, width, height, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	else if (scaled_width == width && scaled_height == height)
		glTexImage2D (GL_TEXTURE_2D, 0, samples, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	else
	{
		gluScaleImage (GL_RGBA, width, height, GL_UNSIGNED_BYTE, trans,
			scaled_width, scaled_height, GL_UNSIGNED_BYTE, scaled);
		glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	}
#else
texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height*4);
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);

	glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (mipmap)
	{
		int		miplevel;

		miplevel = 0;
		while (scaled_width > 1 || scaled_height > 1)
		{
			GL_MipMap ((byte *)scaled, scaled_width, scaled_height);
			scaled_width >>= 1;
			scaled_height >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;
			if (scaled_height < 1)
				scaled_height = 1;
			miplevel++;
			glTexImage2D (GL_TEXTURE_2D, miplevel, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
		}
	}
done: ;
#endif


	if (mipmap)
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
static	unsigned	trans[640*480];		// FIXME, temporary
	int			i, s;
	qboolean	noalpha;
	int			p;

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	}
	else
	{
		if (s&3)
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Upload32 (trans, width, height, mipmap, alpha);
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha)
{
//    qboolean    noalpha;
    int			i;//, p, s;
	gltexture_t	*glt;

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				if (width != glt->width || height != glt->height)
					Sys_Error ("GL_LoadTexture: cache mismatch");
				return gltextures[i].texnum;
			}
		}
	}
	else {
		glt = &gltextures[numgltextures];
		numgltextures++;
	}

    GLuint texture;
    glGenTextures(1, &texture);
    
	strcpy (glt->identifier, identifier);
	glt->texnum = texture;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;

	GL_Bind(texture);

	GL_Upload8 (data, width, height, mipmap, alpha);

	return texture;
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
	return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true);
}

/****************************************/

static GLenum oldtarget = GL_TEXTURE0;

void GL_SelectTexture (GLenum target) 
{
	if (!gl_mtexable)
		return;
	glActiveTexture(target);
	if (target == oldtarget) 
		return;
	cnttextures[oldtarget-GL_TEXTURE0] = currenttexture;
	currenttexture = cnttextures[target-GL_TEXTURE0];
	oldtarget = target;
}
