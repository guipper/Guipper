#pragma include "../common.frag"

uniform sampler2D textura1;
uniform sampler2D textura2;
uniform float offset2y=0.5; // Offset en Y para la textura 2
uniform float offset1y=0.5; // Offset en Y para la textura 1
uniform float blendmode; // Esto controlará el modo de blending

uniform float offset1x; // Offset en X para la textura 1
uniform float offset2x; // Offset en X para la textura 2
uniform float smoothness; // Controla el suavizado

float smootherstep(float edge0, float edge1, float x) {
    x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return x*x*x*(x*(x*6.0-15.0)+10.0);
}

void main()
{
    vec2 uv = gl_FragCoord.xy / resolution;

    vec4 color1 = vec4(0.0);
    vec4 color2 = vec4(0.0);

    vec2 uv_textura1 = uv * vec2(2.0, 1.0) - vec2(offset1x, mapr(offset1y,-1.,1.));
    vec2 uv_textura2 = uv * vec2(2.0, 1.0) - vec2(1.0 - offset2x, mapr(offset2y,-1.,1.));

    if (uv_textura1.x >= 0.0 && uv_textura1.x <= 1.0 && uv_textura1.y >= 0.0 && uv_textura1.y <= 1.0) {
        color1 = texture2D(textura1, uv_textura1);
        //color1 *= smootherstep(0.0, smoothness, uv_textura1.y) * smootherstep(1.0, 1.0-smoothness, uv_textura1.y);
    }
    
    if (uv_textura2.x >= 0.0 && uv_textura2.x <= 1.0 && uv_textura2.y >= 0.0 && uv_textura2.y <= 1.0) {
        color2 = texture2D(textura2, uv_textura2);
        //color2 *= smootherstep(0.0, smoothness, uv_textura2.y) * smootherstep(1.0, 1.0-smoothness, uv_textura2.y);
    }
	int bm = int(mapr(blendmode, 0.0, 25.0));
	vec3 fin = blendMode(bm, color1.rgb, color2.rgb);
    if (color1.a > 0.0 && color2.a > 0.0) {
		
		vec3 inter = mix(color1.rgb,color2.rgb,0.5);
		fin = mix(fin,inter,smoothness);
        fragColor.rgb = fin;
		
    } else {
        fragColor = color1 + color2;
    }

    fragColor.a = 1.0;
}