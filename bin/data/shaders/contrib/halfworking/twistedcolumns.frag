#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales


const int numSamples = 6;
uniform sampler2DRect texture1;

vec4 segment( float x0, float x1, vec2 uv, float id, float time, float f )
{
	float u = (uv.x - x0)/(x1 - x0);
	float v =-1.0*(id+0.5)*time+2.0*uv.y/3.141593 + f*2.0;
	float w = (x1 - x0);

	vec3 col = texture2DRect( texture1, vec2(u) ).xyz;
	col += 0.3*sin( 2.0*f + 2.0*id + vec3(0.0,1.0,2.0) );

	//col *= mix( 1.0, smoothstep(-0.95,-0.94, sin(8.0*6.2831*v + 3.0*u + 2.0*f)), smoothstep(0.4,0.5,sin(f*13.0)) );
	col *= mix( 1.0, smoothstep(-0.8,-0.7, sin(80.0*v)*sin(20.0*u) ), smoothstep(0.4,0.5,sin(f*17.0)) );

	col *= smoothstep( 0.01, 0.03, 0.5-abs(u-0.5) );

	// lighting
	col *= vec3(0.0,0.1,0.3) + w*vec3(1.1,0.7,0.4);
	col *= mix(1.0-u,1.0,w*w*w*0.45);

	float edge = 1.0-smoothstep( 0.5,0.5+0.02/w, abs(u-0.5) );
	return vec4(col,  edge * step(x0,x1) );

}

void main(){
  vec2 uv = (-resolution.xy+2.0*gl_FragCoord.xy.xy) / max(resolution.x,resolution.y);
	uv *= 5.0;

  vec2 st = vec2( length(uv), atan(uv.y, uv.x) );
  //st = uv;  // uncomment to see the effect in cartersian coordinates
  float id = floor((st.x)/2.0);
  vec3 tot = vec3(0.0);
  for( int j=0; j<numSamples; j++ )
  {
      float h = float(j)/float(numSamples);
      float time2 = time + h*(1.0/30.0);
      vec3 col = vec3(0.2)*(1.0-0.08*st.x);
      vec2 uvr = vec2( mod( st.x, 2.0 ) - 1.0, st.y );
      float a = uvr.y + (id+0.5) * 1.0*time2 + 0.2*sin(3.0*uvr.y)*sin(2.0*time2);
      float r = 0.9;
      float x0 = r*sin(a);
      for(int i=0; i<5; i++ )
      {
          float f = float(i+1)/5.0;
          float x1 = r*sin(a + 6.2831*f );
          vec4 seg = segment(x0, x1, uvr, id, time2, f );
          col = mix( col, seg.rgb, seg.a );
          x0 = x1;
      }
      col *= (1.6-0.1*st.x);
      tot += col;
  }

  tot = tot / float(numSamples);

 	fragColor = vec4( tot, 1.0);

	//fragColor = vec4(fin,1.0);
}
