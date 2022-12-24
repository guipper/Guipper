#pragma include "../common.frag"
#define time iTime
#define resolution iResolution.xy

float hash( float n ){
    return fract(sin(n)*43758.5453);
}

float noise2( vec2 uv ){
    vec3 x = vec3(uv, 0);

    vec3 p = floor(x);
    vec3 f = fract(x);
    
    f       = f*f*(3.0-2.0*f);
    float n = p.x + p.y*57.0 + 113.0*p.z;
    
    return mix(mix(mix( hash(n+0.0), hash(n+1.0),f.x),
                   mix( hash(n+57.0), hash(n+58.0),f.x),f.y),
               mix(mix( hash(n+113.0), hash(n+114.0),f.x),
                   mix( hash(n+170.0), hash(n+171.0),f.x),f.y),f.z);
}

mat2 m = mat2(0.8,0.6,-0.6,0.8);

float fbm2(vec2 p)
{
    float f = 0.0;
    f += 0.5000*noise2( p ); p*=m*2.02;
    f += 0.2500*noise2( p ); p*=m*2.03;
    f += 0.1250*noise2( p ); p*=m*2.01;
    f += 0.0625*noise2( p );
    f /= 0.9375;
    return f;
}

vec3 voronoi( in vec2 x )
{
    ivec2 p = ivec2(floor( x ));
    vec2 f = fract(x);

    ivec2 mb = ivec2(0);
    vec2 mr = vec2(0.0);
    vec2 mg = vec2(0.0);

    float md = 8.0;
    for(int j=-1; j<=1; ++j)
    for(int i=-1; i<=1; ++i)
    {
        ivec2 b = ivec2( i, j );
        vec2  r = vec2( b ) + noise( vec2(p + b) ) - f;
        vec2 g = vec2(float(i),float(j));
		vec2 o = vec2(noise2( vec2(p) + g ));
        float d = length(r);

        if( d<md )
        {
            md = d;
            mr = r;
            mg = g;
        }
    }

    md = 8.0;
    for(int j=-2; j<=2; ++j)
    for(int i=-2; i<=2; ++i)
    {
        ivec2 b = ivec2( i, j );
        vec2 r = vec2( b ) + noise2( vec2(p + b) ) - f;


        if( length(r-mr)>0.00001 )
        md = min( md, dot( 0.5*(mr+r), normalize(r-mr) ) );
    }
    return vec3( md, mr );
}

vec2 tr(vec2 p)
{
 	p = -1.0+2.0*(p/resolution.xy);
    p.x *= resolution.x/resolution.y;
    return p;
}

void main()
{	
	vec2 uv = gl_FragCoord.xy / resolution;
    float map_radius = mod(600.0 - 250.0 * (time*0.05),600.0);
	vec2 focus = vec2(map_radius,0.0);
	float crack_radius = 50.0;

	float radius = max(10,map_radius);	
		//  radius = sin(uv.x*100.+time);
	vec2 fc = fragCoord.xy + focus - resolution/2.0;
	vec2 p = tr(fc);

	vec3 col = 	vec3(0.0);

	vec3 lava = vec3(0.0);
	vec3 ground = vec3(0.5,0.3,0.1);
	float vor = 0.0;
	float len = length(fc) + cos(fbm2(p*10.0)*15.0)*15.0;
    float crack = smoothstep(radius-crack_radius,radius,len);

	{
		float val = 1.0 + cos(p.x*p.y + fbm2(p*5.0) * 20.0 + time*2.0)/ 2.0;
		lava = vec3(val*1.0, val*0.33, val*0.1);
		lava = mix(lava*0.95,lava,len-radius);
		lava *= exp(-1.8);
	}

	{
		float val = 1.0 + sin(fbm2(p * 7.5) * 8.0) / 2.0;
		ground *= exp(-val*0.3);
		vec3 sand = vec3(0.2,0.25,0.0);
		ground = mix(ground,sand,val*0.1);
	}

	{   
		vor = voronoi(p*50.5).x*(1.0-crack)*0.75;
		vor = 1.0-vor;
		vor *= smoothstep(0.0,radius,len);
	}

	col = mix(ground,lava,crack);
	//col = mix(col,lava,smoothstep(radius-crack_radius,radius,vor*radius));

	fragColor = vec4(col, 1.0);
}
