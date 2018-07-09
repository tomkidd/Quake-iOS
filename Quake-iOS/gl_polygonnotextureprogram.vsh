//
//  gl_polygonnotextureprogram.vsh
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/14/16.
//
//

attribute vec4 position;

uniform mat4 transform;

invariant gl_Position;

void main()
{
    gl_Position = transform * vec4(position);
}
