#pragma include "../common.frag"
#define M_PI 3.14159265358979323846

const float kCharBlank = 12.0;
const float kCharMinus = 11.0;
const float kCharDecimalPoint = 10.0;

vec4 iDate = vec4(1.0,0.0,0.0,1.0);


uniform sampler2D iChannel0;


#ifndef BITMAP_VERSION

float InRect(const in vec2 vUV, const in vec4 vRect)
{
	vec2 vTestMin = step(vRect.xy, vUV.xy);
	vec2 vTestMax = step(vUV.xy, vRect.zw);	
	vec2 vTest = vTestMin * vTestMax;
	return vTest.x * vTest.y;
}

float SampleDigit(const in float fDigit, const in vec2 vUV)
{
	const float x0 = 0.0 / 4.0;
	const float x1 = 1.0 / 4.0;
	const float x2 = 2.0 / 4.0;
	const float x3 = 3.0 / 4.0;
	const float x4 = 4.0 / 4.0;
	
	const float y0 = 0.0 / 5.0;
	const float y1 = 1.0 / 5.0;
	const float y2 = 2.0 / 5.0;
	const float y3 = 3.0 / 5.0;
	const float y4 = 4.0 / 5.0;
	const float y5 = 5.0 / 5.0;

	// In this version each digit is made of up to 3 rectangles which we XOR together to get the result
	
	vec4 vRect0 = vec4(0.0);
	vec4 vRect1 = vec4(0.0);
	vec4 vRect2 = vec4(0.0);
		
	if(fDigit < 0.5) // 0
	{
		vRect0 = vec4(x0, y0, x3, y5); vRect1 = vec4(x1, y1, x2, y4);
	}
	else if(fDigit < 1.5) // 1
	{
		vRect0 = vec4(x1, y0, x2, y5); vRect1 = vec4(x0, y0, x0, y0);
	}
	else if(fDigit < 2.5) // 2
	{
		vRect0 = vec4(x0, y0, x3, y5); vRect1 = vec4(x0, y3, x2, y4); vRect2 = vec4(x1, y1, x3, y2);
	}
	else if(fDigit < 3.5) // 3
	{
		vRect0 = vec4(x0, y0, x3, y5); vRect1 = vec4(x0, y3, x2, y4); vRect2 = vec4(x0, y1, x2, y2);
	}
	else if(fDigit < 4.5) // 4
	{
		vRect0 = vec4(x0, y1, x2, y5); vRect1 = vec4(x1, y2, x2, y5); vRect2 = vec4(x2, y0, x3, y3);
	}
	else if(fDigit < 5.5) // 5
	{
		vRect0 = vec4(x0, y0, x3, y5); vRect1 = vec4(x1, y3, x3, y4); vRect2 = vec4(x0, y1, x2, y2);
	}
	else if(fDigit < 6.5) // 6
	{
		vRect0 = vec4(x0, y0, x3, y5); vRect1 = vec4(x1, y3, x3, y4); vRect2 = vec4(x1, y1, x2, y2);
	}
	else if(fDigit < 7.5) // 7
	{
		vRect0 = vec4(x0, y0, x3, y5); vRect1 = vec4(x0, y0, x2, y4);
	}
	else if(fDigit < 8.5) // 8
	{
		vRect0 = vec4(x0, y0, x3, y5); vRect1 = vec4(x1, y1, x2, y2); vRect2 = vec4(x1, y3, x2, y4);
	}
	else if(fDigit < 9.5) // 9
	{
		vRect0 = vec4(x0, y0, x3, y5); vRect1 = vec4(x1, y3, x2, y4); vRect2 = vec4(x0, y1, x2, y2);
	}
	else if(fDigit < 10.5) // '.'
	{
		vRect0 = vec4(x1, y0, x2, y1);
	}
	else if(fDigit < 11.5) // '-'
	{
		vRect0 = vec4(x0, y2, x3, y3);
	}	
	
	float fResult = InRect(vUV, vRect0) + InRect(vUV, vRect1) + InRect(vUV, vRect2);
	
	return mod(fResult, 2.0);
}

#else

float SampleDigit(const in float fDigit, const in vec2 vUV)
{		
	if(vUV.x < 0.0) return 0.0;
	if(vUV.y < 0.0) return 0.0;
	if(vUV.x >= 1.0) return 0.0;
	if(vUV.y >= 1.0) return 0.0;
	
	// In this version, each digit is made up of a 4x5 array of bits
	
	float fDigitBinary = 0.0;
	
	if(fDigit < 0.5) // 0
	{
		fDigitBinary = 7.0 + 5.0 * 16.0 + 5.0 * 256.0 + 5.0 * 4096.0 + 7.0 * 65536.0;
	}
	else if(fDigit < 1.5) // 1
	{
		fDigitBinary = 2.0 + 2.0 * 16.0 + 2.0 * 256.0 + 2.0 * 4096.0 + 2.0 * 65536.0;
	}
	else if(fDigit < 2.5) // 2
	{
		fDigitBinary = 7.0 + 1.0 * 16.0 + 7.0 * 256.0 + 4.0 * 4096.0 + 7.0 * 65536.0;
	}
	else if(fDigit < 3.5) // 3
	{
		fDigitBinary = 7.0 + 4.0 * 16.0 + 7.0 * 256.0 + 4.0 * 4096.0 + 7.0 * 65536.0;
	}
	else if(fDigit < 4.5) // 4
	{
		fDigitBinary = 4.0 + 7.0 * 16.0 + 5.0 * 256.0 + 1.0 * 4096.0 + 1.0 * 65536.0;
	}
	else if(fDigit < 5.5) // 5
	{
		fDigitBinary = 7.0 + 4.0 * 16.0 + 7.0 * 256.0 + 1.0 * 4096.0 + 7.0 * 65536.0;
	}
	else if(fDigit < 6.5) // 6
	{
		fDigitBinary = 7.0 + 5.0 * 16.0 + 7.0 * 256.0 + 1.0 * 4096.0 + 7.0 * 65536.0;
	}
	else if(fDigit < 7.5) // 7
	{
		fDigitBinary = 4.0 + 4.0 * 16.0 + 4.0 * 256.0 + 4.0 * 4096.0 + 7.0 * 65536.0;
	}
	else if(fDigit < 8.5) // 8
	{
		fDigitBinary = 7.0 + 5.0 * 16.0 + 7.0 * 256.0 + 5.0 * 4096.0 + 7.0 * 65536.0;
	}
	else if(fDigit < 9.5) // 9
	{
		fDigitBinary = 7.0 + 4.0 * 16.0 + 7.0 * 256.0 + 5.0 * 4096.0 + 7.0 * 65536.0;
	}
	else if(fDigit < 10.5) // '.'
	{
		fDigitBinary = 2.0 + 0.0 * 16.0 + 0.0 * 256.0 + 0.0 * 4096.0 + 0.0 * 65536.0;
	}
	else if(fDigit < 11.5) // '-'
	{
		fDigitBinary = 0.0 + 0.0 * 16.0 + 7.0 * 256.0 + 0.0 * 4096.0 + 0.0 * 65536.0;
	}
	
	vec2 vPixel = floor(vUV * vec2(4.0, 5.0));
	float fIndex = vPixel.x + (vPixel.y * 4.0);
	
	return mod(floor(fDigitBinary / pow(2.0, fIndex)), 2.0);
}

#endif


float rand(vec2 co){return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);}
float rand (vec2 co, float l) {return rand(vec2(rand(co), l));}
float rand (vec2 co, float l, float t) {return rand(vec2(rand(co, l), t));}

float perlin(vec2 p, float dim, float time) {
	vec2 pos = floor(p * dim);
	vec2 posx = pos + vec2(1.0, 0.0);
	vec2 posy = pos + vec2(0.0, 1.0);
	vec2 posxy = pos + vec2(1.0);
	
	float c = rand(pos, dim, time);
	float cx = rand(posx, dim, time);
	float cy = rand(posy, dim, time);
	float cxy = rand(posxy, dim, time);
	
	vec2 d = fract(p * dim);
	d = -0.5 * cos(d * M_PI) + 0.5;
	
	float ccx = mix(c, cx, d.x);
	float cycxy = mix(cy, cxy, d.x);
	float center = mix(ccx, cycxy, d.y);
	
	return center * 2.0 - 1.0;
}

float perlin(vec2 p, float dim) {
	
	return perlin(p, dim, 0.0);
}
void main()
{
    //properties
    float noise_intensity = 0.2;
    float flickering = 0.1;
    float bar_freq = 200.0;
    float color_intensity = 0.4;
    
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    float val = rand(vec2(iTime, iTime)) * flickering + flickering;
    
    float sin_val = 1.0 - ((sin(uv.y*bar_freq) + 1.0 ) / 2.0 * val * 2.0);
    
    vec4 color = vec4(1.0 - length(vec2(1, 0) - uv)*0.5,length(vec2(1, 0) - uv), 1, 1);
    
    float noise = rand(uv * iTime) * noise_intensity + (1.0 - noise_intensity);
    
    vec4 out_image = texture(iChannel0, uv);

    float shift_val = clamp(perlin(vec2(0, uv.y * 40.0 + 123.0 + iTime), 1.0) * 4.0 - 3.0, 0.0, 1.0) * (rand(vec2(iTime)) * 0.01 + 0.3);
    
    vec2 vFontSize = vec2(4.0, 5.0) * 5.0;
    
    vec2 timePos = vec2(400.0, 300.0);
    vec2 datePos = vec2(-40.0, 20.0);
    
    float r = texture(iChannel0, uv + vec2(shift_val, 0.0) + vec2(0.005, 0)).r;
    float g = texture(iChannel0, uv + vec2(shift_val, 0.0) + vec2(-0.005, 0)).g;
    float b = texture(iChannel0, uv + vec2(shift_val, 0.0)).b;
    
    out_image = vec4(r, g, b, 1.0);
     
    fragColor = mix(out_image * sin_val, color, color_intensity) * noise;

}