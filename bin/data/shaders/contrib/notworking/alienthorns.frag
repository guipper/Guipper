#pragma include "../common.frag"

// Alien Thorns
// Dave Hoskins
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
uniform sampler2DRect iChannel0;
uniform sampler2DRect iChannel1;
uniform sampler2DRect iChannel2;
uniform sampler2DRect iChannel3;

#define PRECISION 0.02

#define MOD3 vec3(.0631,.07369,.08787)

vec3 sunDir = normalize(vec3(-.3, 0.6, .8));

//float time;

//--------------------------------------------------------------------------------------------------
vec3 TexCube(in vec3 p, in vec3 n )
{
    p *= .5;
	vec3 x = texture2DRect( iChannel0, p.yz).xyz;
	vec3 y = texture2DRect( iChannel1, p.zx).xyz;
	vec3 z = texture2DRect( iChannel2, p.xy).xyz;
	return x*abs(n.x) + y*abs(n.y) + z*abs(n.z);
}

//--------------------------------------------------------------------------------------------------
vec4 ThornVoronoi( vec3 p, out float which)
{
    
    vec2 f = fract(p.xz);
    p.xz = floor(p.xz);
	float d = 1.0e10;
    vec3 id = vec3(0.0);
    
	for (int xo = -1; xo <= 1; xo++)
	{
		for (int yo = -1; yo <= 1; yo++)
		{
            vec2 g = vec2(xo, yo);
            vec2 n = textureLod(iChannel3,(p.xz + g+.5)/256.0, 0.0).xy;
            n = n*n*(3.0-2.0*n);
            
			vec2 tp = g + .5 + sin(p.y + 1.2831 * (n * time*.5)) - f;
            float d2 = dot(tp, tp);
			if (d2 < d)
            {
                // 'id' is the colour code for each thorn
                d = d2;
                which = n.x+n.y*3.0;
                id = vec3(tp.x, p.y, tp.y);
            }
		}
	}

    return vec4(id, 1.35-pow(d, .17));
}


//--------------------------------------------------------------------------------------------------
float MapThorns( in vec3 pos)
{
    float which;
	return pos.y * .21 - ThornVoronoi(pos, which).w  - max(pos.y-5.0, 0.0) * .5 + max(pos.y-5.5, 0.0) * .8;
}

//--------------------------------------------------------------------------------------------------
vec4 MapThornsID( in vec3 pos, out float which)
{
    vec4 ret = ThornVoronoi(pos, which);
	return vec4(ret.xyz, pos.y * .21 - ret.w - max(pos.y-5.0, 0.0) * .5 + max(pos.y-5.5, 0.0) * .8);
}

//--------------------------------------------------------------------------------------------------
float Hash12(vec2 p)
{
	vec3 p3  = fract(vec3(p.xyx) * MOD3);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract(p3.x * p3.y * p3.z);
}

//--------------------------------------------------------------------------------------------------
vec4 Raymarch( in vec3 ro, in vec3 rd, in vec2 uv, in vec2 gl_FragCoord.xy, out float which)
{
	float maxd = 40.0;
	
    vec4 h = vec4(1.0);
    float t = 0.+ Hash12(gl_FragCoord.xy.xy)*.2;
    vec3 p;
    for (int i = 0; i < 110; i++)
    {
        p = ro + rd * t;
        if(h.w < PRECISION || t > maxd || p.y > 12.0 ) break;
	    h = MapThornsID(p, which);
        t += h.w * .5 + min(t*.002, .03);
    }

    if (t > maxd || p.y > 8.0)	t = -1.0;
    
    return vec4(h.xyz, t);
}

//--------------------------------------------------------------------------------------------------
vec3 Normal( in vec3 pos )
{
    vec2 eps = vec2(PRECISION, 0.0);
	return normalize( vec3(
           MapThorns(pos+eps.xyy) - MapThorns(pos-eps.xyy),
           MapThorns(pos+eps.yxy) - MapThorns(pos-eps.yxy),
           MapThorns(pos+eps.yyx) - MapThorns(pos-eps.yyx) ) );

}

//--------------------------------------------------------------------------
float FractalNoise(in vec2 xy)
{
	float w = 1.5;
	float f = 0.0;
    xy *= .08;

	for (int i = 0; i < 5; i++)
	{
		f += texture(iChannel2, .5+xy * w, -99.0).x / w;
		w += w;
	}
	return f*.8;
}

//--------------------------------------------------------------------------
vec3 GetClouds(in vec3 sky, in vec3 cameraPos, in vec3 rd)
{
    //if (rd.y < 0.0) return vec3(0);
	// Uses the ray's y component for horizon fade of fixed colour clouds...
	float v = (70.0-cameraPos.y)/rd.y;
	rd.xz = (rd.xz * v + cameraPos.xz+vec2(0.0,0.0)) * 0.004;
	float f = (FractalNoise(rd.xz) -.5);
	vec3 cloud = mix(sky, vec3(.4, .2, .2), max(f, 0.0));
   	return cloud;
}

//
//--------------------------------------------------------------------------------------------------
float Shadow( in vec3 ro, in vec3 rd, float mint)
{
    float res = 1.0;
    float t = .15;
    for( int i=0; i < 15; i++ )
    {
        float h = MapThorns(ro + rd*t);
		h = max( h, 0.0 );
        res = min( res, 4.0*h/t );
        t+= clamp( h*.6, 0.05, .1);
		if(h < .001) break;
    }
    return clamp(res,0.05,1.0);
}

//--------------------------------------------------------------------------------------------------
vec3 Path( float time )
{
	return vec3(1.3+ 17.2*cos(0.2-0.5*.33*time*.75), 4.7, 7.- 16.2*sin(0.5*0.11*time*.75) );
}

//--------------------------------------------------------------------------------------------------
void main()
{

    vec2 q = gl_FragCoord.xy.xy / iResolution.xy;
	vec2 p = (-1.0 + 2.0*q)*vec2(iResolution.x / iResolution.y, 1.0);
	
    // Camera...
	float off = iMouse.x*1.0*iMouse.x/iResolution.x;
	time =173.0+iTime + off;
	vec3 ro = Path( time+0.0 );
    
	vec3 ta = Path( time+5.2 );
    float add = (sin(time*.3)+1.0)*2.0;
    ro.y+= add;
    ta.y -= add;
	ta.y *= 1.0+sin(3.0+0.12*time) * .5;
	float roll = 0.3*sin(0.07*time);
	
	vec3 cw = normalize(ta-ro);
	vec3 cp = vec3(sin(roll), cos(roll),0.0);
	vec3 cu = normalize(cross(cw,cp));
	vec3 cv = (cross(cu,cw));
	
	float r2 = p.x*p.x*0.32 + p.y*p.y;
    p *= (7.0-sqrt(37.5-11.5*r2))/(r2+1.0);

	vec3 rd = normalize( p.x*cu + p.y*cv + 2.1*cw );

	vec3 col 		= mix(vec3(.3, .3, .5), GetClouds(vec3(0.), ro, rd),	pow(abs(rd.y), .5));
    vec3 background = mix(vec3(.3, .3, .5), vec3(.0), 						pow(abs(rd.y), .5));

	float sun = clamp( dot(rd, sunDir), 0.0, 1.0 );
	float which;
	vec4 ret = Raymarch(ro, rd, q, gl_FragCoord.xy, which);
    
    if(ret.w > 0.0)
	{
		vec3 pos = ro + ret.w*rd;
		vec3 nor = Normal(pos);
		vec3 ref = reflect(rd, nor);
		
		float s = clamp( dot( nor, sunDir ), 0.0, 1.0 );
		
        float sha = 0.0; if( s>0.01) sha = Shadow(pos, sunDir, 0.05);
		vec3 lin = s*vec3(1.0,.9,.8) * sha;
		lin += background*(max(nor.y, 0.0)*.2);

		col = TexCube(ret.xyz, nor);
        vec3 wormCol =  clamp(abs(fract(which * 1.5 + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) -1.0, 0.0, 1.0);
        
		col = lin * col * (.7 + wormCol * .6);
        col += vec3(1.0, .6, 1.0)*pow(clamp( dot( ref, sunDir ), 0.0, 1.0 ), 10.0) * sha;
		
		col = mix( col, background, 1.0-exp(-0.002*ret.w*ret.w) );
	}

    col += vec3(.4,.25,.25)*pow( sun, 20.0 )*4.0*clamp( (rd.y+0.4) / .2,0.0,1.0);
    // Gamma & colour adjust...
	col = pow(col, vec3(.45, .45, .5));
    // Border shading...
    col *= 0.5 + 0.5*pow( 52.0*q.x*q.y*(1.0-q.x)*(1.0-q.y), 0.2 );

	fragColor = vec4( col, 1.0 );
}