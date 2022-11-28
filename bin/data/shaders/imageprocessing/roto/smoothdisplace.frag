#pragma include "../common.frag"

uniform sampler2D input_texture;

#define	TAU 6.28318

float GetWaveDisplacement(vec3 p)
{ 
    float time = iTime;
	float waveStrength = 0.1;
	float frequency = 10.0;
	float waveSpeed = -0.35;
	float rotSpeed = 0.01;
	float twist = 0.05;
	float falloffRange = 2.0;	// the other values have been tweaked around this...
	
	float d = length(p);
	//p.xz *= rotate2d(d*twist+(time*rotSpeed)*TAU);
	vec2 dv = p.xz*0.15;
	d = length(dv);
	d = clamp(d,0.0,falloffRange);
	float d2 = d-falloffRange;
	float t = fract(time*waveSpeed)*TAU;
	float s = sin(frequency*d*d+t);
	float k = s * waveStrength * d2*d2;
	k *= p.x*p.z*0.5;
	k-= 0.4;					// mix it up a little...
	k -= sin(time)*0.5*d2;			// really mix it up... :)
	k = smoothstep(0.0,0.75,k*k);
//	k = p.z;
	
	vec2 uv = gl_FragCoord.xy / resolution.xy;
	vec4 tk = texture2D(input_texture,p.xy);
	
	float prom = (tk.r+tk.g+tk.b)/3.;
	prom = smoothstep(0.0,0.75,prom*prom);
	return prom;
	
}


float map(vec3 p)
{
	float k = GetWaveDisplacement(p);
	
	vec2 uv = gl_FragCoord.xy / resolution;
	vec4 tk = texture2D(input_texture,uv);
	float prom = (tk.r+tk.g+tk.b)/3.;
	float dist = p.y - k - 1.0;
	return dist;
    
    //float d =  length(p.xz);
    //float t2 = fract(iTime*0.5) * TAU;
	//float y = 0.5+sin(t2 + d)*0.5;
	//y = y*=abs(p.x*p.z)*0.125;
	//y = smoothstep(0.0,4.0,y);
	//return p.y - y*y;
}

vec3 normal( in vec3 pos )
{
    vec2 e = vec2(1.0,-1.0)*0.5773*0.0005;
    return normalize( e.xyy*map( pos + e.xyy ) + 
					  e.yyx*map( pos + e.yyx ) + 
					  e.yxy*map( pos + e.yxy ) + 
					  e.xxx*map( pos + e.xxx ) );
}
vec3 render(vec3 ro, vec3 rd)
{
	// march	
	float tmin = 0.1;
	float tmax = 120.;
	vec3 p;
	float t = tmin;
	for (int i = 0; i < 80; i++)
	{
		p = ro + t * rd;
		float d = map(p);
		t += d*0.5;
		if (t > tmax)
			break;		
	}
	
    // light
	if (t < tmax)
	{
	   	vec3 lightDir = normalize(vec3(1.5, 1.0, 0.5));
		vec3 nor = normal(p);
		vec3 c = vec3(0.3, 0.1, 0.5);
		
		float dif = max(dot(nor, lightDir), 0.0);
		c += vec3(0.2) * dif;
		
		vec3 ref = reflect(rd, nor);
		float spe = max(dot(ref, lightDir), 0.0);
		c += vec3(3.0) * pow(spe, 16.);
		
		return c;
	}
	
	return vec3(0.2,0.2,0.6);
}

mat3 camera(vec3 ro, vec3 ta, vec3 up)
{
	vec3 nz = normalize(ta - ro);
	vec3 nx = cross(nz, normalize(up));
	vec3 ny = cross(nx, nz);
	return mat3(nx, ny, nz);
}

void main()
{
   	vec2 q = fragCoord.xy / iResolution.xy;
	vec2 p = (fragCoord.xy * 2.0 - iResolution.xy) / iResolution.xy;
	p.x *= iResolution.x / iResolution.y;
    
	vec3 ro = vec3(0.0, 10.0, -10.0);
	vec3 ta = vec3(0.0, 0.0, -2.0);
	vec3 rd = camera(ro, ta, vec3(0.0, 1.0, 0.0)) * normalize(vec3(p.xy, 1.0));
	
	vec3 c = render(ro, rd);

    // vignette
    c *= 0.4 + 0.6*pow( 16.0*q.x*q.y*(1.0-q.x)*(1.0-q.y), 0.1 );

    if (iMouse.z>0.5)
    {
        p*= 10.0;
        float v = GetWaveDisplacement(vec3(p.x,0.0,p.y));
        v = clamp(v,0.0,1.0);
        c = vec3(v);
    }
    
    
	fragColor = vec4(c, 1.0);
}