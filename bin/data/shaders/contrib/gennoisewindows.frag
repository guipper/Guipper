#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales
float Threshold(vec3 col, float threshold)
{
    if (col.r < threshold)
    {
     	return 0.0f;
    }
    return 1.0f;
}

float Random(float t)
{
 	return fract(sin(t) * 1e4);   
}

// Returns a pseudorandom value from [0,1]
float Random(vec2 uv)
{    
	return fract(sin(dot(uv, vec2(12.313, 53.34))) * 100000.0f);
    //return fract(sin(dot(uv, vec2(12.313, 53.34))) * 10000.0f);
}

//Gradient of rows/column
vec3 GradientColor(float x, float y)
{
	return vec3(
        x,
        y,
        1.0f
   	);
}

void main()
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;
    vec2 grid = vec2(100.0f, 50.0f);    
    vec2 ipos = uv * grid;      
   	
    float shiftFactor = iTime * 20.0f * Random(floor(ipos.y))*0.5f + 0.5f;
    if (mod(floor(ipos.y), 2.0f) == 0.0f)
    	ipos.x -= shiftFactor;
    else
    	ipos.x += shiftFactor;
    
    ipos.x -= shiftFactor;
    
    ipos = floor(ipos);
            
    vec3 col = vec3(Random(ipos));
    col = vec3(Threshold(col, 0.3f));
   
    // generate margins
    vec2 fpos = fract(uv * grid);
    
    col *= step(0.2f, fpos.y);    
    
    float pctX = smoothstep(0.5f-0.005f, 0.5f, uv.x) - smoothstep(0.5f, 0.5f+0.005f, uv.x);
    float pctY = smoothstep(0.5f-0.01f, 0.5f, uv.y) - smoothstep(0.5f, 0.5f+0.01f, uv.y);
    
    //col *= GradientColor(uv.x, uv.y);
    // Diff color for each window  
    if (uv.x < 0.5f && uv.y > 0.5f)
    {
      	col *= vec3(1.0f, 0.5f, 0.5f);
    }
    else if (uv.x < 0.5f && uv.y < 0.5f)
    {
       	col *= vec3(0.5f, 0.5f, 1.0f);
    }
    else if (uv.x > 0.5f && uv.y < 0.5f)
    {
       	col *= vec3(1.0f, 1.0f, 0.5f);
    }
    else if (uv.x > 0.5f && uv.y > 0.5f)
    {
       	col *= vec3(0.5f, 1.0f, 0.5f);
    }
    
    col = (1.0f-pctX)*col;
    col *= (1.0f-pctY)*col;
   
    // Output to screen
    fragColor = vec4(col,1.0);
}