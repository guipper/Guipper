#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

// Contributions made by keiranhalcyon7
// Based on theGiallo's https://www.shadertoy.com/view/MttSz2
// MIT License. Use freely; but attribution is expected.
#define TAU 6.28318
uniform float period;
uniform float speed;
uniform float rotation_speed;
uniform float focaldepth;
const float t2 = 4.0; // Length in seconds of the effect

// This effect fades in and out of white every t2 seconds
// Remove the next def to get an infinite tunnel instead.
//#define WHITEOUT 1

// Perlin noise from Dave_Hoskins' https://www.shadertoy.com/view/4dlGW2
//----------------------------------------------------------------------------------------
float Hash(in vec2 p, in float scale)
{
	// This is tiling part, adjusts with the scale...
	p = mod(p, scale);
	return fract(sin(dot(p, vec2(27.16898, 38.90563))) * 5151.5473453);
}

//----------------------------------------------------------------------------------------
float Noise(in vec2 p, in float scale )
{
	vec2 f;
	p *= scale;
	f = fract(p);		// Separate integer from fractional

    p = floor(p);
    f = f*f*(3.0-2.0*f);	// Cosine interpolation approximation
	
    float res = mix(mix(Hash(p, 				 scale),
						Hash(p + vec2(1.0, 0.0), scale), f.x),
					mix(Hash(p + vec2(0.0, 1.0), scale),
						Hash(p + vec2(1.0, 1.0), scale), f.x), f.y);
    return res;
}

//----------------------------------------------------------------------------------------
float fBm(in vec2 p)
{
    //p += vec2(sin(iTime * .7), cos(iTime * .45))*(.1) + iMouse.xy*.1/iResolution.xy;
	float f = 0.0;
	// Change starting scale to any integer value...
	float scale = 40.0;
    p = mod(p, scale);
	float amp   = 0.6;
	
	for (int i = 0; i < 5; i++)
	{
		f += Noise(p, scale) * amp;
		amp *= 0.5;
		// Scale must be multiplied by an integer value...
		scale *= 2.0;
	}
	// Clamp it just in case....
	return min(f, 1.0);
}

void main()
{
    float t = mod(iTime, t2);
    t = t / t2; // Normalized time

    vec4 col = vec4(0.0);
	vec2 q = fragCoord.xy / iResolution.xy;
	vec2 p = ( 2.0 * fragCoord.xy - iResolution.xy ) / min( iResolution.y, iResolution.x );
    vec2 mo = vec2(.0);
    p += vec2(0.0, -0.1);

    //float ay = TAU * mod(iTime, 8.0) / 8.0;
    //ay = 45.0 * 0.01745;
    float ay = 0.0, ax = 0.0, az = 0.0;
    if (iMouse.z > 0.0) {
        ay = 3.0 * mo.x;
        ax = 3.0 * mo.y;
    }
    mat3 mY = mat3(
         cos(ay), 0.0,  sin(ay),
         0.0,     1.0,      0.0,
        -sin(ay), 0.0,  cos(ay)
    );

    mat3 mX = mat3(
        1.0,      0.0,     0.0,
        0.0,  cos(ax), sin(ax),
        0.0, -sin(ax), cos(ax)
    );
    mat3 m = mX * mY;

    vec3 v = vec3(p, 1.0);
    v = m * v;
    float v_xy = length(v.xy);
    float z = v.z / v_xy;

    // The focal_depth controls how "deep" the tunnel looks. Lower values
	// provide more depth.
	float focal_depth = focaldepth;
    #ifdef WHITEOUT
    focal_depth = mix(0.15, 0.015, smoothstep(0.65, 1.0, t));
    #endif

	float mperiod = floor(mapr(period,0.0,10.0));
	
    vec2 polar;
    //float p_len = length(p);
    float p_len = length(v.xy);
    //polar.y = focal_depth / p_len + iTime * speed;
    polar.y = z * focal_depth + iTime * speed;
    float a = atan(v.y, v.x);
    a -= iTime * mapr(rotation_speed,-1.0,1.0);
    float x = fract(a / TAU);
    polar.y /= 3.0;
    polar.x = x * mperiod + polar.y;


    // Colorize blue
    //col = texture(iChannel1, cp);
    float val = fBm(polar);
    col.rgb = vec3(0.15, 0.4, 0.9) * vec3(val)*1.3;

    // Add white spots
    vec3 white = 0.5 * vec3(smoothstep(.8, 1.0, val));
    col.rgb += white;

    float w_total = 0.0, w_out = 0.0;
    #ifdef WHITEOUT
    // Fade in and out from white every t2 seconds
    float w_in = 0.0;
    w_in = abs(1.0 - 1.0 * smoothstep(0.0, 0.25, t));
    w_out = abs(1.0 * smoothstep(0.8, 1.0, t));
    w_total = max(w_in, w_out);
    #endif


    // Add the white disk at the center
    float disk_size = max(0.025, 0.5 * w_out);
    //float disk_size = 0.11;
    float disk_col = exp(-(p_len*1.5 - disk_size) * 2.);
    //col.rgb += mix(col.xyz, vec3(1,1,1), disk_col);
    col.rgb += vec3(disk_col, disk_col, disk_col);


    #ifdef WHITEOUT
    col.rgb = mix(col.rgb, vec3(1.0), w_total);
    #endif

    fragColor = vec4(col.rgb,1);
}