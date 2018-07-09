//
//  gl_coloredpolygon1textureprogram.vsh
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/14/16.
//
//

attribute vec3 position;
attribute vec4 color;
attribute vec2 texcoords;

varying vec2 texcoords_fragment;
varying vec4 color_fragment;

uniform mat4 transform;

invariant gl_Position;

void main()
{
    gl_Position = transform * vec4(position, 1.0);
    
    texcoords_fragment = texcoords;
    
    color_fragment = color;
}
