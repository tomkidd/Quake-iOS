//
//  gl_intensitypolygon1textureprogram.vsh
//  Quake-iOS
//
//  Created by Heriberto Delgado on 6/19/16.
//
//

attribute vec3 position;
attribute float intensity;
attribute vec2 texcoords;

varying vec2 texcoords_fragment;
varying vec4 color_fragment;

uniform mat4 transform;

invariant gl_Position;

void main()
{
    gl_Position = transform * vec4(position, 1.0);
    
    texcoords_fragment = texcoords;
    
    color_fragment = vec4(intensity, intensity, intensity, 1.0);
}
