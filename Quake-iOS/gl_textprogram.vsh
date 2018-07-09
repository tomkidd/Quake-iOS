//
//  gl_textprogram.vsh
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/14/16.
//
//

attribute vec2 position;
attribute vec2 texcoords;

varying vec2 texcoords_fragment;

uniform mat4 transform;

invariant gl_Position;

void main()
{
    gl_Position = transform * vec4(position, -4.0, 1.0);
    
    texcoords_fragment = texcoords;
}
