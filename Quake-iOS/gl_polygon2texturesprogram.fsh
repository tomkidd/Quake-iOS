//
//  gl_polygon2texturesprogram.fsh
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/14/16.
//
//

uniform sampler2D texture0;
uniform sampler2D texture1;

varying mediump vec2 texcoords0_fragment;
varying mediump vec2 texcoords1_fragment;

invariant gl_FragColor;

void main()
{
    gl_FragColor = texture2D(texture0, texcoords0_fragment) * (vec4(1.0, 1.0, 1.0, 1.0) - texture2D(texture1, texcoords1_fragment));
}
