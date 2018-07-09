//
//  gl_alphapolygon1textureprogram.fsh
//  Quake-iOS
//
//  Created by Heriberto Delgado on 5/18/16.
//
//

uniform sampler2D texture;

varying mediump vec2 texcoords_fragment;

invariant gl_FragColor;

void main()
{
    mediump vec4 result = texture2D(texture, texcoords_fragment);
    
    if (result.a > 0.66)
    {
        gl_FragColor = result;
    }
    else
    {
        discard;
    }
}
