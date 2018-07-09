//
//  gl_coloredpolygonnotextureprogram.vsh
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/14/16.
//
//

attribute vec4 position;
attribute vec4 color;

uniform mat4 transform;

varying vec4 color_fragment;

invariant gl_Position;

void main()
{
    color_fragment = color;
    
    gl_Position = transform * vec4(position);
}
