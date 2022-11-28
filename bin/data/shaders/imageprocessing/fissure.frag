#pragma include "../common.frag"

uniform sampler2D input_texture;


void main() {
	 
  vec4 p = vec4(gl_FragCoord.xy,0.,1.)/iResolution.xy-.5, d=p, t;
    float s = sign(d.y);
    p.z -= iTime*4.;
    d.y = -abs(d.y);
    for(float i = 1.5; i >0.; i-=.01)
    {
        t = max(vec4(.3,.7,1,0)*-s, texture(input_texture, p.xz * .001 * s ));
        fragColor = t*i;
        t.x -= p.y*.04;
        if(t.x>.99) break;
        p += (d-d*t.x)*8.;
    }
}
