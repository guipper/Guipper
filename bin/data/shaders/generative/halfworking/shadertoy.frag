#pragma include "../common.frag"

/*#define iTime time
#define iResolution resolution
#define iMouse mouse
#define texture(tex,uv) texture2DRect(tex,(uv)*resolution)
*/
uniform sampler2DRect iChannel0;

// begin shadertoy code

mat2 r(float a) {
    float se=sin(a), co=cos(a);
    return mat2(se,co,co,-se);
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = fragCoord.xy / iResolution.xy;
	vec2 xy = uv*2.-1.;
    uv.x*=iResolution.x/iResolution.y;
    xy.x*=iResolution.x/iResolution.y;
	vec2 p=xy*.6;
    vec3 c=vec3(0.);
	float b=.2,a=3.;
    p.x=abs(p.x);
    for (int i=0; i<20; i++) {
		float l=1.25+cos(iTime*5.)*.5;
        float t=sin((iTime+80.)*.2)+sin(iTime*a)*b;
        c+=max(0.,1.-length(p))*texture(iChannel0,p+.4+vec2(0.,iTime*3.)*.2).xyz*l*.15;
        p.x=abs(p.x); p=p*1.3-vec2(.3,0.);
        p*=r(t);
		a*=0.95;
		b*=1.1;
    }
    c+=abs(p.y)*.005*vec3(.7,.8,.9);
    c+=abs(p.x)*.005*vec3(.9,.7,.8);
    fragColor = vec4(pow(c,vec3(1.5)),1.);
}

// end shadertoy code

void main() {
  vec4 fragColor;
  mainImage(fragColor, gl_FragCoord.xy);
  gl_FragColor = fragColor;
}
