#pragma include "../common.frag"

uniform sampler2D input_texture;


void main() {
	
//ve2 uv =gl_FragCoord.xy/ resolution.xy;
  vec4 p = vec4(gl_FragCoord.xy,0.,1.)/iResolution.xyxy-.5, d=p, t, c;
    p.x += iTime;
    for(float i=1.; i>0.; i-=.02)
    {
        t = abs(mod(p, 8.)-4.); 
        c = texture(input_texture, t.zy-5.);
        float x = -log(exp(-t.y) + exp(-length(t.xz)))-c.x; 
        fragColor = mix(c*i*i, vec4(0,1,2,0), p.z*.01);
        if(x<.01) break;
        p -= d*x;
     }
}
