#pragma include "../common.frag"

uniform sampler2D input_texture;
uniform float effect_mix=1.0;
uniform float effect_exp=0.49;
uniform float color_mix=1.0;
uniform float sample_size=0.18;
uniform float brightness=0.36;

void main()
{
    float size=2.+sample_size*10.;
    float it=floor(size*size);
    vec4 Cs=texture(input_texture,gl_FragCoord.xy/resolution);
    vec3 C=Cs.rgb;
	  vec3 R=vec3(0.);
    // Neighbours are weighted by their alpha. A fully transparent pixel still
    // carries an RGB - almost always black - and comparing against it drew a
    // hard fake edge all the way around the transparent border of a PNG or a
    // GIF. On fully opaque input every weight is 1, so this is exactly the old
    // average and nothing changes.
    float wsum=0.;
    for(float i=0.; i<it; i++) {
    	vec2 p=vec2(mod(i,size),floor(i/size))-floor(size*.5);
        vec4 aS=texture(input_texture,(gl_FragCoord.xy+p)/resolution);
		    R+=pow(distance(C,aS.rgb),effect_exp*3.)*aS.a;
        wsum+=aS.a;
    };
    R/=max(wsum,0.0001);
    R=mix(R,C*R,color_mix);
    R*=1.+brightness*50.;
    R=mix(C,R,effect_mix);
    // Source alpha, so the transparent background stays transparent instead of
    // becoming an opaque black frame around the effect.
    fragColor = vec4(R,Cs.a);
}
 