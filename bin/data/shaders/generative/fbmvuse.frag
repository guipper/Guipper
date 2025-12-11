#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

uniform float speed = 0.5;
uniform float speedy = 0.5;
uniform float scalex;
uniform float scaley;
uniform float flush;
uniform float animationspeed1;
uniform float animationspeed2;

float random3 (in vec2 _st) {
    return fract(sin(dot(_st.xy,
                         vec2(12.9898,78.233)))*
        43758.56222123);
}

float noise2 (in vec2 st,float fase) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    float fase2 = fase;
    // Four corners in 2D of a tile
    float a = sin(random3(i)*fase2);
    float b =  sin(random3(i + vec2(1.0, 0.0))*fase2);
    float c =  sin(random3(i + vec2(0.0, 1.0))*fase2);
    float d =  sin(random3(i + vec2(1.0, 1.0))*fase2);

    // Smooth Interpolation

    // Cubic Hermine Curve.  Same as SmoothStep()
    vec2 u = f*f*(3.0-2.0*f);
    // u = smoothstep(0.,1.,f);

    // Mix 4 coorners percentages
    return mix(a, b, u.x) +
            (c - a)* u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}
void main()
{	
	float times = time*speed;
	vec2 uv = gl_FragCoord.xy / resolution;
	vec2 p = vec2(0.5) - uv;
	float r = length(p);
	float a = atan(p.x,p.y);
	
	vec3 col1 = vec3(15./255.,23./255.,45./255);
	vec3 col2 = vec3(189./255.,33./255.,207./255.);
	vec3 col3 = vec3(226./255.,232./255.,240/255.);
	
	vec3 col4 = vec3(15./255.,23./255.,45./255);
	vec3 col5 = vec3(189./255.,33./255.,207./255.);
	vec3 col6 = vec3(226./255.,232./255.,240/255.);

	float e1 = sin(uv.x*4.+sin(uv.y*10.))*.5+.5;
	e1=noise2(uv*2.5,time*.7)*.5+.2;
	vec3 fin = mix(col1,col2,e1);
	//fin = mix(fin,col3,e2);

	
	fragColor = vec4(fin,1.0);

}
