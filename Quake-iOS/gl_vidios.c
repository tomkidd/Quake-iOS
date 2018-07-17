//
//  gl_vidiosvr.c
//  Quake-iOS
//
//  Created by Heriberto Delgado on 1/30/16.
//
//

#include "quakedef.h"

#define WARP_WIDTH 320
#define WARP_HEIGHT 200

int	gl_screenwidth;
int gl_screenheight;

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

GLuint gl_textprogram;

GLchar* gl_textprogram_vertex;
GLchar* gl_textprogram_fragment;

GLint gl_textprogram_position;
GLint gl_textprogram_texcoords;
GLint gl_textprogram_transform;
GLint gl_textprogram_color;
GLint gl_textprogram_texture;

GLuint gl_noalphatextprogram;

GLchar* gl_noalphatextprogram_vertex;
GLchar* gl_noalphatextprogram_fragment;

GLint gl_noalphatextprogram_position;
GLint gl_noalphatextprogram_texcoords;
GLint gl_noalphatextprogram_transform;
GLint gl_noalphatextprogram_color;
GLint gl_noalphatextprogram_texture;

GLuint gl_fillprogram;

GLchar* gl_fillprogram_vertex;
GLchar* gl_fillprogram_fragment;

GLint gl_fillprogram_position;
GLint gl_fillprogram_transform;
GLint gl_fillprogram_color;

GLfloat gl_textandfill_matrix[16];

GLuint gl_polygonnotextureprogram;

GLchar* gl_polygonnotextureprogram_vertex;
GLchar* gl_polygonnotextureprogram_fragment;

GLint gl_polygonnotextureprogram_position;
GLint gl_polygonnotextureprogram_transform;
GLint gl_polygonnotextureprogram_color;

GLuint gl_coloredpolygonnotextureprogram;

GLchar* gl_coloredpolygonnotextureprogram_vertex;
GLchar* gl_coloredpolygonnotextureprogram_fragment;

GLint gl_coloredpolygonnotextureprogram_position;
GLint gl_coloredpolygonnotextureprogram_color;
GLint gl_coloredpolygonnotextureprogram_transform;

GLuint gl_polygon1textureprogram;

GLchar* gl_polygon1textureprogram_vertex;
GLchar* gl_polygon1textureprogram_fragment;

GLint gl_polygon1textureprogram_position;
GLint gl_polygon1textureprogram_texcoords;
GLint gl_polygon1textureprogram_transform;
GLint gl_polygon1textureprogram_texture;

GLuint gl_alphapolygon1textureprogram;

GLchar* gl_alphapolygon1textureprogram_vertex;
GLchar* gl_alphapolygon1textureprogram_fragment;

GLint gl_alphapolygon1textureprogram_position;
GLint gl_alphapolygon1textureprogram_texcoords;
GLint gl_alphapolygon1textureprogram_transform;
GLint gl_alphapolygon1textureprogram_texture;

GLuint gl_coloredpolygon1textureprogram;

GLchar* gl_coloredpolygon1textureprogram_vertex;
GLchar* gl_coloredpolygon1textureprogram_fragment;

GLint gl_coloredpolygon1textureprogram_position;
GLint gl_coloredpolygon1textureprogram_color;
GLint gl_coloredpolygon1textureprogram_texcoords;
GLint gl_coloredpolygon1textureprogram_transform;
GLint gl_coloredpolygon1textureprogram_texture;

GLuint gl_intensitypolygon1textureprogram;

GLchar* gl_intensitypolygon1textureprogram_vertex;
GLchar* gl_intensitypolygon1textureprogram_fragment;

GLint gl_intensitypolygon1textureprogram_position;
GLint gl_intensitypolygon1textureprogram_intensity;
GLint gl_intensitypolygon1textureprogram_texcoords;
GLint gl_intensitypolygon1textureprogram_transform;
GLint gl_intensitypolygon1textureprogram_texture;

GLuint gl_tintedpolygon1textureprogram;

GLchar* gl_tintedpolygon1textureprogram_vertex;
GLchar* gl_tintedpolygon1textureprogram_fragment;

GLint gl_tintedpolygon1textureprogram_position;
GLint gl_tintedpolygon1textureprogram_texcoords;
GLint gl_tintedpolygon1textureprogram_transform;
GLint gl_tintedpolygon1textureprogram_color;
GLint gl_tintedpolygon1textureprogram_texture;

GLuint gl_polygon2texturesprogram;

GLchar* gl_polygon2texturesprogram_vertex;
GLchar* gl_polygon2texturesprogram_fragment;

GLint gl_polygon2texturesprogram_position;
GLint gl_polygon2texturesprogram_texcoords0;
GLint gl_polygon2texturesprogram_texcoords1;
GLint gl_polygon2texturesprogram_transform;
GLint gl_polygon2texturesprogram_texture0;
GLint gl_polygon2texturesprogram_texture1;

qboolean gl_rendermirror_enabled;
GLfloat gl_rendermirror_color[4];

GLfloat gl_polygon_matrix[16];
GLfloat gl_projection_matrix[16];

qboolean tiltaim_enabled;

qboolean glvr_enabled;

GLfloat glvr_eyetranslation[16];
GLfloat glvr_projection[16];

unsigned short	d_8to16table[256];
unsigned*	d_8to24table;
unsigned char d_15to8table[65536];

float		gldepthmin, gldepthmax;

qboolean isPermedia = false;
qboolean gl_mtexable = true;

//int		texture_mode = GL_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_NEAREST;
//int		texture_mode = GL_NEAREST_MIPMAP_LINEAR;
int		texture_mode = GL_LINEAR;
//int		texture_mode = GL_LINEAR_MIPMAP_NEAREST;
//int		texture_mode = GL_LINEAR_MIPMAP_LINEAR;

static qboolean fullsbardraw = false;

cvar_t	gl_ztrick = {"gl_ztrick","0"};

GLuint GL_Compile (char* name, char* extension, GLenum type, GLchar* program)
{
    GLint shader = glCreateShader(type);
    
    glShaderSource(shader, 1, (const GLchar* const *)&program, NULL);
    glCompileShader(shader);
    
    char* infolog = NULL;
    GLint length = 0;

    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);

    if (length > 0)
    {
        infolog = malloc(length);
        
        glGetShaderInfoLog(shader, length, &length, infolog);
    }
    
    GLint status;
    
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    
    if (status == 0)
    {
        glDeleteShader(shader);
        
        if (infolog != NULL)
        {
            Sys_Error("%s.%s: %s", name, extension, infolog);
        }
    }
    
    if (infolog != NULL)
    {
        free(infolog);
    }
    
    return shader;
}

void GL_Link (char* name, GLuint program)
{
    glLinkProgram(program);
    
    char* infolog = NULL;
    GLint length = 0;
    
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    
    if (length > 0)
    {
        infolog = malloc(length);
        
        glGetProgramInfoLog(program, length, &length, infolog);
    }
    
    GLint status;
    
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    
    if (status != 0 && infolog != NULL)
    {
        Sys_Error("GL_Link %s: %s", name, infolog);
    }

    if (infolog != NULL)
    {
        free(infolog);
    }
}

GLuint GL_CreateProgram (char* name, GLchar* vertexprogram, GLchar* fragmentprogram)
{
    GLuint program = glCreateProgram();
    
    GLuint vertexshader = GL_Compile (name, "vsh", GL_VERTEX_SHADER, vertexprogram);
    
    GLuint fragmentshader = GL_Compile (name, "fsh", GL_FRAGMENT_SHADER, fragmentprogram);

    glAttachShader(program, vertexshader);
    glAttachShader(program, fragmentshader);
    
    GL_Link (name, program);
    
    glDetachShader(program, vertexshader);
    glDeleteShader(vertexshader);
    
    glDetachShader(program, fragmentshader);
    glDeleteShader(fragmentshader);
    
    return program;
}

void GL_Init (void)
{
    gl_vendor = (const char *)glGetString (GL_VENDOR);
    Con_Printf ("GL_VENDOR: %s\n", gl_vendor);
    gl_renderer = (const char *)glGetString (GL_RENDERER);
    Con_Printf ("GL_RENDERER: %s\n", gl_renderer);
    
    gl_version = (const char *)glGetString (GL_VERSION);
    Con_Printf ("GL_VERSION: %s\n", gl_version);
    gl_extensions = (const char *)glGetString (GL_EXTENSIONS);
    Con_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);
    
    //	Con_Printf ("%s %s\n", gl_renderer, gl_version);
    
    gl_textprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_textprogram.vsh");
    gl_textprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_textprogram.fsh");
    
    gl_noalphatextprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_textprogram.vsh");
    gl_noalphatextprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_textprogram.fsh");
    
    gl_fillprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_fillprogram.vsh");
    gl_fillprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_fillprogram.fsh");
    
    gl_polygonnotextureprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_polygonnotextureprogram.vsh");
    gl_polygonnotextureprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_polygonnotextureprogram.fsh");;
    
    gl_coloredpolygonnotextureprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_coloredpolygonnotextureprogram.vsh");
    gl_coloredpolygonnotextureprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_coloredpolygonnotextureprogram.fsh");
    
    gl_polygon1textureprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_polygon1textureprogram.vsh");
    gl_polygon1textureprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_polygon1textureprogram.fsh");
    
    gl_alphapolygon1textureprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_alphapolygon1textureprogram.vsh");
    gl_alphapolygon1textureprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_alphapolygon1textureprogram.fsh");
    
    gl_coloredpolygon1textureprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_coloredpolygon1textureprogram.vsh");
    gl_coloredpolygon1textureprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_coloredpolygon1textureprogram.fsh");
    
    gl_intensitypolygon1textureprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_intensitypolygon1textureprogram.vsh");
    gl_intensitypolygon1textureprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_intensitypolygon1textureprogram.fsh");
    
    gl_tintedpolygon1textureprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_tintedpolygon1textureprogram.vsh");
    gl_tintedpolygon1textureprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_tintedpolygon1textureprogram.fsh");
    
    gl_polygon2texturesprogram_vertex = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_polygon2texturesprogram.vsh");
    gl_polygon2texturesprogram_fragment = Sys_LoadTextFromFile(gl_shaderdirectory, "gl_polygon2texturesprogram.fsh");
    
    gl_textprogram = GL_CreateProgram ("gl_textprogram", gl_textprogram_vertex, gl_textprogram_fragment);
    
    gl_textprogram_position = glGetAttribLocation(gl_textprogram, "position");
    gl_textprogram_texcoords = glGetAttribLocation(gl_textprogram, "texcoords");
    
    gl_textprogram_transform = glGetUniformLocation(gl_textprogram, "transform");
    gl_textprogram_color = glGetUniformLocation(gl_textprogram, "color");
    gl_textprogram_texture = glGetUniformLocation(gl_textprogram, "texture");
    
    gl_noalphatextprogram = GL_CreateProgram ("gl_noalphatextprogram", gl_noalphatextprogram_vertex, gl_noalphatextprogram_fragment);
    
    gl_noalphatextprogram_position = glGetAttribLocation(gl_noalphatextprogram, "position");
    gl_noalphatextprogram_texcoords = glGetAttribLocation(gl_noalphatextprogram, "texcoords");
    
    gl_noalphatextprogram_transform = glGetUniformLocation(gl_noalphatextprogram, "transform");
    gl_noalphatextprogram_color = glGetUniformLocation(gl_noalphatextprogram, "color");
    gl_noalphatextprogram_texture = glGetUniformLocation(gl_noalphatextprogram, "texture");

    gl_fillprogram = GL_CreateProgram ("gl_fillprogram", gl_fillprogram_vertex, gl_fillprogram_fragment);
    
    gl_fillprogram_position = glGetAttribLocation(gl_fillprogram, "position");
    
    gl_fillprogram_transform = glGetUniformLocation(gl_fillprogram, "transform");
    gl_fillprogram_color = glGetUniformLocation(gl_fillprogram, "color");
    
    gl_polygonnotextureprogram = GL_CreateProgram ("gl_polygonnotextureprogram", gl_polygonnotextureprogram_vertex, gl_polygonnotextureprogram_fragment);
    
    gl_polygonnotextureprogram_position = glGetAttribLocation(gl_polygonnotextureprogram, "position");

    gl_polygonnotextureprogram_transform = glGetUniformLocation(gl_polygonnotextureprogram, "transform");
    gl_polygonnotextureprogram_color = glGetUniformLocation(gl_polygonnotextureprogram, "color");
    
    gl_coloredpolygonnotextureprogram = GL_CreateProgram ("gl_coloredpolygonnotextureprogram", gl_coloredpolygonnotextureprogram_vertex, gl_coloredpolygonnotextureprogram_fragment);
    
    gl_coloredpolygonnotextureprogram_position = glGetAttribLocation(gl_coloredpolygonnotextureprogram, "position");
    gl_coloredpolygonnotextureprogram_color = glGetAttribLocation(gl_coloredpolygonnotextureprogram, "color");
    
    gl_coloredpolygonnotextureprogram_transform = glGetUniformLocation(gl_coloredpolygonnotextureprogram, "transform");
    
    gl_polygon1textureprogram = GL_CreateProgram ("gl_polygon1textureprogram", gl_polygon1textureprogram_vertex, gl_polygon1textureprogram_fragment);
    
    gl_polygon1textureprogram_position = glGetAttribLocation(gl_polygon1textureprogram, "position");
    gl_polygon1textureprogram_texcoords = glGetAttribLocation(gl_polygon1textureprogram, "texcoords");
    
    gl_polygon1textureprogram_transform = glGetUniformLocation(gl_polygon1textureprogram, "transform");
    gl_polygon1textureprogram_texture = glGetUniformLocation(gl_polygon1textureprogram, "texture");
    
    gl_alphapolygon1textureprogram = GL_CreateProgram ("gl_alphapolygon1textureprogram", gl_alphapolygon1textureprogram_vertex, gl_alphapolygon1textureprogram_fragment);
    
    gl_alphapolygon1textureprogram_position = glGetAttribLocation(gl_alphapolygon1textureprogram, "position");
    gl_alphapolygon1textureprogram_texcoords = glGetAttribLocation(gl_alphapolygon1textureprogram, "texcoords");
    
    gl_alphapolygon1textureprogram_transform = glGetUniformLocation(gl_alphapolygon1textureprogram, "transform");
    gl_alphapolygon1textureprogram_texture = glGetUniformLocation(gl_alphapolygon1textureprogram, "texture");
    
    gl_coloredpolygon1textureprogram = GL_CreateProgram ("gl_coloredpolygon1textureprogram", gl_coloredpolygon1textureprogram_vertex, gl_coloredpolygon1textureprogram_fragment);
    
    gl_coloredpolygon1textureprogram_position = glGetAttribLocation(gl_coloredpolygon1textureprogram, "position");
    gl_coloredpolygon1textureprogram_color = glGetAttribLocation(gl_coloredpolygon1textureprogram, "color");
    gl_coloredpolygon1textureprogram_texcoords = glGetAttribLocation(gl_coloredpolygon1textureprogram, "texcoords");
    
    gl_coloredpolygon1textureprogram_transform = glGetUniformLocation(gl_coloredpolygon1textureprogram, "transform");
    gl_coloredpolygon1textureprogram_texture = glGetUniformLocation(gl_coloredpolygon1textureprogram, "texture");
    
    gl_intensitypolygon1textureprogram = GL_CreateProgram ("gl_intensitypolygon1textureprogram", gl_intensitypolygon1textureprogram_vertex, gl_intensitypolygon1textureprogram_fragment);
    
    gl_intensitypolygon1textureprogram_position = glGetAttribLocation(gl_intensitypolygon1textureprogram, "position");
    gl_intensitypolygon1textureprogram_intensity = glGetAttribLocation(gl_intensitypolygon1textureprogram, "intensity");
    gl_intensitypolygon1textureprogram_texcoords = glGetAttribLocation(gl_intensitypolygon1textureprogram, "texcoords");
    
    gl_intensitypolygon1textureprogram_transform = glGetUniformLocation(gl_intensitypolygon1textureprogram, "transform");
    gl_intensitypolygon1textureprogram_texture = glGetUniformLocation(gl_intensitypolygon1textureprogram, "texture");
    
    gl_tintedpolygon1textureprogram = GL_CreateProgram ("gl_tintedpolygon1textureprogram", gl_tintedpolygon1textureprogram_vertex, gl_tintedpolygon1textureprogram_fragment);
    
    gl_tintedpolygon1textureprogram_position = glGetAttribLocation(gl_tintedpolygon1textureprogram, "position");
    gl_tintedpolygon1textureprogram_texcoords = glGetAttribLocation(gl_tintedpolygon1textureprogram, "texcoords");
    
    gl_tintedpolygon1textureprogram_transform = glGetUniformLocation(gl_tintedpolygon1textureprogram, "transform");
    gl_tintedpolygon1textureprogram_color = glGetUniformLocation(gl_tintedpolygon1textureprogram, "color");
    gl_tintedpolygon1textureprogram_texture = glGetUniformLocation(gl_tintedpolygon1textureprogram, "texture");
    
    gl_polygon2texturesprogram = GL_CreateProgram ("gl_polygon2texturesprogram", gl_polygon2texturesprogram_vertex, gl_polygon2texturesprogram_fragment);
    
    gl_polygon2texturesprogram_position = glGetAttribLocation(gl_polygon2texturesprogram, "position");
    gl_polygon2texturesprogram_texcoords0 = glGetAttribLocation(gl_polygon2texturesprogram, "texcoords0");
    gl_polygon2texturesprogram_texcoords1 = glGetAttribLocation(gl_polygon2texturesprogram, "texcoords1");
    
    gl_polygon2texturesprogram_transform = glGetUniformLocation(gl_polygon2texturesprogram, "transform");
    gl_polygon2texturesprogram_texture0 = glGetUniformLocation(gl_polygon2texturesprogram, "texture0");
    gl_polygon2texturesprogram_texture1 = glGetUniformLocation(gl_polygon2texturesprogram, "texture1");

    GL_Use (gl_textprogram);
    
    glUniform1i(gl_textprogram_texture, 0);
    
    GL_Use (gl_noalphatextprogram);
    
    glUniform1i(gl_noalphatextprogram_texture, 0);
    
    GL_Use (gl_polygon1textureprogram);
    
    glUniform1i(gl_polygon1textureprogram_texture, 0);
    
    GL_Use (gl_alphapolygon1textureprogram);
    
    glUniform1i(gl_alphapolygon1textureprogram_texture, 0);

    GL_Use (gl_coloredpolygon1textureprogram);
    
    glUniform1i(gl_coloredpolygon1textureprogram_texture, 0);
    
    GL_Use (gl_intensitypolygon1textureprogram);
    
    glUniform1i(gl_intensitypolygon1textureprogram_texture, 0);
    
    GL_Use (gl_tintedpolygon1textureprogram);
    
    glUniform1i(gl_tintedpolygon1textureprogram_texture, 0);
    
    GL_Use (gl_polygon2texturesprogram);
    
    glUniform1i(gl_polygon2texturesprogram_texture0, 0);
    glUniform1i(gl_polygon2texturesprogram_texture1, 1);

    GL_Use (0);
    
    glvr_enabled = true;
    
    glClearColor (1,0,0,0);
    glEnable(GL_DEPTH_TEST);
//    glEnable(GL_SCISSOR_TEST);
    glCullFace(GL_FRONT);
    
    glActiveTexture(GL_TEXTURE1);
    
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glActiveTexture(GL_TEXTURE0);
    
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
#if 0
    CheckArrayExtensions ();
    glEnable (GL_VERTEX_ARRAY_EXT);
    glEnable (GL_TEXTURE_COORD_ARRAY_EXT);
    glVertexPointerEXT (3, GL_FLOAT, 0, 0, &glv.x);
    glTexCoordPointerEXT (2, GL_FLOAT, 0, 0, &glv.s);
    glColorPointerEXT (3, GL_FLOAT, 0, 0, &glv.r);
#endif
}

void GL_BeginRendering (int *x, int *y, int *width, int *height)
{
    (*x) = 0;
    (*y) = 0;
    (*width) = gl_screenwidth;
    (*height) = gl_screenheight;
}

void GL_EndRendering (void)
{
    if (fullsbardraw)
        Sbar_Changed();
}

void	VID_SetPalette (unsigned char *palette)
{
    byte	*pal;
    unsigned r,g,b;
    unsigned v;
    int     r1,g1,b1;
    int		j,k,l;//,m;
    unsigned short i;
    unsigned	*table;
    
    //
    // 8 8 8 encoding
    //
    pal = palette;
    table = d_8to24table;
    for (i=0 ; i<256 ; i++)
    {
        r = pal[0];
        g = pal[1];
        b = pal[2];
        pal += 3;
        
        v = (255 << 24) | (b << 16) | (g << 8) | r;
        *table++ = v;
    }
    d_8to24table[255] &= 0xFFFFFF;	// 255 is transparent
    for (i=0; i < (1<<15); i++) {
        /* Maps
         000000000000000
         000000000011111 = Red  = 0x1F
         000001111100000 = Blue = 0x03E0
         111110000000000 = Grn  = 0x7C00
         */
        r = ((i & 0x1F) << 3)+4;
        g = ((i & 0x03E0) >> 2)+4;
        b = ((i & 0x7C00) >> 7)+4;
        pal = (unsigned char *)d_8to24table;
        for (v=0,k=0,l=10000*10000; v<256; v++,pal+=4) {
            r1 = r-pal[0];
            g1 = g-pal[1];
            b1 = b-pal[2];
            j = (r1*r1)+(g1*g1)+(b1*b1);
            if (j<l) {
                k=v;
                l=j;
            }
        }
        d_15to8table[i]=k;
    }
}

void	VID_ShiftPalette (unsigned char *palette)
{
    //VID_SetPalette(palette);
}

void	VID_Init (unsigned char *palette)
{
    char	gldir[MAX_OSPATH];

    Cvar_RegisterVariable (&gl_ztrick);
    
    d_8to24table = malloc(256 * sizeof(unsigned));
    
    vid.width = vid.conwidth = vid.maxwarpwidth = WARP_WIDTH;
    vid.height = vid.conheight = vid.maxwarpheight = WARP_HEIGHT;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
    
    VID_SetPalette(palette);
    
    GL_Init ();

    sprintf (gldir, "%s/glquake", com_gamedir);
    Sys_mkdir (gldir);

    if (COM_CheckParm("-fullsbar"))
        fullsbardraw = true;
}

void	VID_Shutdown (void)
{
    if (d_8to24table != NULL)
    {
        free(d_8to24table);
    }
}
