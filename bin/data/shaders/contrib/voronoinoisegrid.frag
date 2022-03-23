#pragma include "../common.frag" //ESta linea tiene todas las definiciones de las funciones globales

#define TILING 12.0

// distance functions
#define DISTANCE 0
#define SQR_DISTANCE 1
#define MINKOWSKI 2
#define MANHATTAN 3
#define CHEBYCHEV 4
#define QUADRATIC 5

// voronoi types
#define CLOSEST_1 0
#define CLOSEST_2 1
#define DIFFERENCE_21 2
#define CRACKLE 3

float sqr_distance(vec3 a, vec3 b)
{
    vec3 d = a - b;
    return dot(d, d);
}

float manhattan(vec3 a, vec3 b)
{
    vec3 d = abs(a - b);
    return d.x + d.y + d.z;
}

float chebychev(vec3 a, vec3 b)
{
    return max(max(abs(a.x - b.x), abs(a.y - b.y)), abs(a.z - b.z));
}

float quadratic(vec3 a, vec3 b)
{
    vec3 d = a - b;
    return (d.x*d.x+d.y*d.y+d.z*d.z+d.x*d.y+d.x*d.z+d.y*d.z);
}

float minkowski(vec3 a, vec3 b)
{
    vec3 d = a - b;
    float p = 0.5;

    return pow(dot(pow(abs(d), vec3(p)), vec3(1.0)), 1.0 / p);
}

// iq
vec3 random3f( vec3 p )
{
    return fract(sin(vec3( dot(p,vec3(1.0,57.0,113.0)),
                           dot(p,vec3(57.0,113.0,1.0)),
                           dot(p,vec3(113.0,1.0,57.0))))*43758.5453);
}

float voronoi3(vec3 p, int dist_func, int type)
{
    vec3 fp = floor(p);

    float d1 = 1./0.;
    float d2 = 1./0.;

    for(int i = -1; i < 2; i++)
    {
        for(int j = -1; j < 2; j++)
        {
            for(int k = -1; k < 2; k++)
            {
                vec3 cur_p = fp + vec3(i, j, k);

                vec3 r = random3f(cur_p);

                float cd = 0.0;

                if(dist_func == DISTANCE)
                	cd = distance(p, cur_p + r);
                else if(dist_func == SQR_DISTANCE)
                	cd = sqr_distance(p, cur_p + r);
                else if(dist_func == MANHATTAN)
                	cd = manhattan(p, cur_p + r);
                else if(dist_func == CHEBYCHEV)
                    cd = chebychev(p, cur_p + r);
                else if(dist_func == QUADRATIC)
                    cd = quadratic(p, cur_p + r);
                else if(dist_func == MINKOWSKI)
                    cd = minkowski(p, cur_p + r);

                d2 = min(d2, max(cd, d1));
                d1 = min(d1, cd);
            }
        }
    }

    if(type == CLOSEST_1)
    	return d1;
    else if(type == CLOSEST_2)
        return d2;
    else if(type == DIFFERENCE_21)
        return d2 - d1;
    else if(type == CRACKLE)
        return clamp(max(0.5, 16.0 * (d2-d1)), 0.0, 1.0);
}

void main()
{
    vec2 screen = ((fragCoord.xy / iResolution.xy) * 2.0 - 1.0);

    // determine the voronoi type and distance function to use
    int voronoi_type = int((1.0 - screen.y) / (2./4.));
    int dist_func = int((screen.x + 1.0) / (2./6.));

    // get the value at this pixel
    screen.x *= iResolution.x / iResolution.y;
    vec3 pos = vec3(screen * TILING, iTime * 0.5);
    float h = voronoi3(pos, dist_func, voronoi_type);

    // invert every 4 seconds
    float invert_time = mod(iTime, 8.0);
    if(invert_time > 4.0) h = 1.0 - h;

    fragColor = vec4(vec3(1.0f - h), 1);
}
