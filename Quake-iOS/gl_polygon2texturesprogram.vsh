//
//  gl_polygon2texturesprogram.vsh
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/14/16.
//
//

attribute vec3 position;
attribute vec2 texcoords0;
attribute vec2 texcoords1;

varying vec2 texcoords0_fragment;
varying vec2 texcoords1_fragment;

uniform mat4 transform;

invariant gl_Position;

void main()
{
    gl_Position = transform * vec4(position, 1.0);
    
    texcoords0_fragment = texcoords0;
    texcoords1_fragment = texcoords1;
}
