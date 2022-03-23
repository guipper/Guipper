#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float samples;
uniform float brightness;
uniform float contrast;
uniform float modo;
uniform sampler2D textura1;

void main()
{
	vec2 uv = gl_FragCoord.xy;

	vec3 t1 =  texture(textura1, gl_FragCoord.xy/resolution).rgb;

	vec3 res = vec3(0.);

	float smp=samples*50.;

	float scale=sqrt(smp);

	for (float i=0.; i<smp; i++) {
		vec2 p=vec2(mod(i,scale),floor(i/scale))-scale*.5;
		vec3 fb = texture(textura1, gl_FragCoord.xy+p).rgb;
		if (modo<.3) res+=abs(fb-t1);
		else if (modo>=.3 && modo<.6) res+=abs(fb-t1)*(1.-dot(fb,t1));
		else res+=dot(fb,t1)*.1;
	}

	res/=smp;
	res*=brightness*50.;
	res=pow(res,vec3(contrast*5.));

	gl_FragColor = vec4(res,1.0);
}
