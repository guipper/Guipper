#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform sampler2D texture1;
uniform float contrast;
uniform float threshold;
uniform float color;
uniform float speed;

float hash12(vec2 p)
{
	vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main()
{
	vec4 t1 =  texture2D(texture1, gl_FragCoord.xy/resolution);

    float l = pow(length(t1),1.+contrast*2.);

    float r = hash12(gl_FragCoord.xy+time*.01*speed);

    float d = step(l, r+threshold);

    vec3 col = mix(vec3(1.),normalize(t1.rgb),color)*d;

	gl_FragColor = vec4(col,1.0);
}
