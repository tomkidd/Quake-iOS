//
//  gl_polygon1textureprogram.fsh
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/14/16.
//
//

uniform sampler2D texture;

varying mediump vec2 texcoords_fragment;

invariant gl_FragColor;

void main()
{
    gl_FragColor = texture2D(texture, texcoords_fragment);
}
