#pragma include "../common.frag"


uniform float sphere_radius;
uniform float sphere_count=0.3;
uniform float sphere_sep;
uniform float zdist;
uniform float mixcolortexture;
uniform sampler2D fondo;
uniform sampler2D ce;
#define SPHERE_RADIUS mapr(sphere_radius,0.0,2.75)    // size of spheres
#define SPHERE_COUNT floor(mapr(sphere_count,0.0,40.))       // number of spheres
#define REFLECTIONS 1.          // number of reflections, expensive
#define FILL_LIGHTS 5.        // number of fill lights, expensive
#define BIAS 0.009            // don't remember if this gets used
#define COLOUR_OFFSET 0.3     // colour pallete
#define SOFT_STEPS 10         // sphere-marching steps for soft shadows
#define SHADOW_SHARPNESS 10.  // hardness of soft shadows
#define SHADOW_QUALITY 100      // 2 or higher=soft shadows, 1=hard shadows only on first bounce, 0=none
#define FILL_SHADOWBIAS 10     // reduce shadow quality for the more numerous fill lights
#define AO_RADIUS 0.5        // AO radius
#define AO_POWER 0.3          // AO flatness
#define AO_GAIN 0.02          // AO de-buttcrack-ifier
#define SKY_GAIN 0.5         // brightness of sky, and therefore also "ambient" contribution

vec3 hash31(float p)
{
   vec3 p3 = fract(vec3(p) * vec3(.1031, .1030, .0973));
   p3 += dot(p3, p3.yzx+33.33);
   return fract((p3.xxy+p3.yzz)*p3.zyx); 
}
float hash12(vec2 p)
{
	vec3 p3  = fract(vec3(p.xyx) * .1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
vec3 hash32(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz+33.33);
    return fract((p3.xxy+p3.yzz)*p3.zyx);
}

vec3 colourOfID(int ID)
{
    vec3 azul = vec3(0.0, 0.0, 1.0);
    vec3 magenta = vec3(1.0, 0.0, 1.0);
    vec3 blanco = vec3(1.0, 1.0, 1.0);
	
	vec2 uv = gl_FragCoord.xy / resolution.xy;
    vec4 cee = texture(ce,uv);
	
	azul = mix(cee.rgb,azul,mixcolortexture);
	magenta = mix(cee.rgb,magenta,mixcolortexture);
	blanco = mix(cee.rgb,blanco,mixcolortexture);
    float t = fract(sin(float(ID)*12.9898+COLOUR_OFFSET) * 43758.5453); // hash simple

    if (t < 0.3333) {
        return mix(azul, magenta, t / 0.3333);
    } else if (t < 0.6666) {
        return mix(magenta, blanco, (t - 0.3333) / (0.6666 - 0.3333));
    } else {
        return mix(blanco, azul, (t - 0.6666) / (1.0 - 0.6666));
    }
}

// for tracing a ray
struct RayToTrace
{
    // ray parameters    
    vec3 origin;
    vec3 direction;
    
    float dist; // how far is ray allowed to travel/how far did it travel?
    
    int emitter; // object ID of emitting surface, to ignore
};

// for returning from raytracing
struct RayHit
{
    vec3 pos; // location of hitpoint
    int ID; // material ID of surface ray hit (-1 if nothing)
    vec3 normal; // normal of surface ray hit
};


// given a ray origin, ray direction, and sphere position
// return distance along ray to nearest (first?) hitpoint on sphere
float sphere(vec3 ro, vec3 rd, vec3 s, float r)
{
    vec3 tos = s-ro;
    float t = dot(rd,tos);
    vec3 p = ro + rd*t;
	
	//s+= sin(p.x*1.+time)*0.1;
	//s+= sin(p.y*1.+time)*0.1;
    float y = length(s-p);
    
    if(y < r)
    {
        float x = sqrt(r*r-y*y);
        return t-x;
        
    } else return 1000.;
}

// get sphere position from the cache
vec3 SpherePos(int i)
{
    vec3 rand =  hash31(float(i));            
    return sin(rand*6.1+iTime*.15)*mapr(sphere_sep,0.0,5.0);
}

// trace rays through spheres, return hit information
RayHit SphereIntersection(RayToTrace rayIn)
{
    RayHit hit;
    hit.normal = rayIn.direction; // probably don't need this, just initiliazing it with something
    hit.pos = rayIn.origin+rayIn.direction * rayIn.dist; // sky hitpoint
    hit.ID = -1; // sky ID
    
    //test against each sphere
    for(int i = 1; i<= SPHERE_COUNT;i++)
    {
        if(i == rayIn.emitter) // no self reflection (we prefer unhealthy coping mechanisms)
            continue;
        vec3 pos = SpherePos(i);
        float dist2hit = sphere(rayIn.origin, rayIn.direction, pos, SPHERE_RADIUS);
		if(dist2hit < rayIn.dist && dist2hit > 0.)
        {
            rayIn.dist = dist2hit;
            hit.pos = rayIn.origin+rayIn.direction*dist2hit;//calculate hitpoint
            hit.ID = i;
            hit.normal = normalize(hit.pos-pos);
        }       
    }
    return hit;
}


// cone tracing, for soft shadows
float ConeTrace(vec3 pos, vec3 dir, bool isFill)
{
    float gain = 1.;
    float dist = 0.005; // launching bias, for saving time
    int iterations = SOFT_STEPS; // less marching steps for fill lights
    if(isFill)
        iterations /= 4;
    for(int j = 0; j<iterations; j++) //sphere marching, because of course it is
    {
        float d = 999.;
        for(int i = 1; i<= SPHERE_COUNT;i++) //iterate over all ... spheres, get dist to nearest surface
        {
            vec3 spos = SpherePos(i);
            float sd = length(pos+(dir*dist)-spos);
            sd -= SPHERE_RADIUS;
        
            if(sd < d)
                d = sd;
        }
        // minimum ratio between step size and distance flown represents the narrowest cone
        gain = min(gain, d/dist*SHADOW_SHARPNESS);
    
        dist += d;
        
        if(d<0.0001 || dist>20.) // save performance on hits or breakaways
            break;
    }
    
    return max(gain,0.);
}

// AO
float SphereOcclusion(vec3 pos, int emitter)
{
    float AO = 1.;
    //accumulate AO influence from each sphere
    for(int i = 1; i<= SPHERE_COUNT;i++)
    {
        if(i == emitter) // no self occlusion
            continue;
        vec3 spos = SpherePos(i);
        float d = length(pos-spos);
        d -= SPHERE_RADIUS;
        d /= AO_RADIUS;
        d = clamp(d,0.,1.);
        
        AO = min(AO, d);
    }
    return pow(AO,.6);
}


float doALight(vec3 p, vec3 normal , vec3 l, vec3 view, int ID, int shadowQuality, bool isFill)
{
    vec3 toLight = l-p;
    vec3 lv = normalize(toLight); // light direction
    float ld = length(toLight); // light distance
    float bias = 0.2; // (bias to prevent the soft shadows from artifacting)
    float light = max(dot(lv,normal)-bias,0.); // N*L term
                           
    view = reflect(view, normal); // do phong
    float phong = max(dot(view,lv),0.1); // simple reflect*L highlight for the OGs
    phong = pow(phong, 140.); // raise to power to make the highlight sharper
    light += phong * 1.8; // apply phong
    light /= pow(ld, 2.0); // inverse-square term
     
    // hard shadows
    if(shadowQuality == 1)
    {
        // prepare ray from position to light location
        RayToTrace ray;
        ray.direction = lv;
        ray.origin = p;
        ray.dist = ld;
        ray.emitter = ID; // don't self-shadow
        // apply switch light off if ray hits anything other then the sky
        if(SphereIntersection(ray).ID != -1)
            light = 0.;      
    }
    
    // soft shadows
    if(shadowQuality > 1)
        light *= ConeTrace(p,lv,isFill);
    
    
    return light;
}





// do all the lights, don't self shadow (use ID to ignore shaded sphere)
// use shadowQuality to limit soft shadows to direct visibility, not reflections
vec3 getLighting(vec3 p, vec3 normal, vec3 view, int ID, int iteration)
{
    vec3 light = vec3(0.);
    
    
    // main orbiting light, casting the main shadows
    vec3 pos = vec3(vec2(cos(iTime),sin(iTime))*20., -3);
    int sq = SHADOW_QUALITY - iteration;
    light += vec3(doALight(p, normal, pos, view, ID, SHADOW_QUALITY-iteration, false))*600.;
    
    
    // fill light shadow quality, NO soft fill lights after first bounce
    sq = SHADOW_QUALITY - iteration - FILL_SHADOWBIAS;
    if(iteration > 0 && sq > 1)
        sq = 1;
        
    // fill lights, arranged in a circle around the camera
    for(float i = 0.; i<6.283185307; i+=6.283185307/FILL_LIGHTS)
    {
        //pos = vec3(vec2(sin(i+iTime/5.),cos(i+iTime/5.))*7.,-9.);
		 pos = vec3(vec2(.0,cos(i+iTime*0.))*2.,-9.);
        float fillLight = doALight(p, normal, pos, view, ID, sq, true);
        fillLight = fillLight/FILL_LIGHTS*180.; // gain
        
        // give each fill light a random colour :)
        light += fillLight *.3;
    }
    
    return light;
}

void main()
{  
      vec2 uv = (2.*fragCoord-iResolution.xy)/iResolution.y;
     // Ajuste explícito para compensar la relación de aspecto
    float aspectRatio = iResolution.x / iResolution.y;
    uv.x *= aspectRatio;
     // initialize ray package
    RayToTrace ray;
    ray.origin = vec3(0.,0.,mapr(zdist,0.0,-40));
    ray.direction = normalize(vec3(uv,3.));  // Dirección del rayo ajustada
    ray.dist = 999.;
    ray.emitter = -1;
        
    vec3 pathColour = vec3(1.1);    // transmissibility of the whole path so far
    vec3 pixelColour = vec3(0.2);  // accumulated light reaching the pixel so far
    
    // info about the first bounce
    vec3 screenPos;
    int screenID;
    
 
    // bounce loop
    for(int i = 0;i<REFLECTIONS  ;i++)
    {
        // trace ray through spheres :eyes:
        RayHit hit = SphereIntersection(ray);
  
        // record results of first hit only
        if(i == 0)
        {
            screenPos = hit.pos;
            screenID = hit.ID;
        }
  
        // stop if hit sky
        if(hit.ID == -1)
            break;
        
        // get material
        vec3 surfaceColour = colourOfID(hit.ID);
        
        // do lighting
        vec3 bounceColour = getLighting(hit.pos,hit.normal,ray.direction,hit.ID, i)*.4;
        bounceColour *= surfaceColour;
     
        // accumulate through path
        pixelColour += bounceColour * pathColour;
        
        // accumulate path attenuation
        float fresnel = pow(1.1-dot(hit.normal,-ray.direction),3.); 
        pathColour *= surfaceColour * (0.55 + fresnel*0.7);
        
        // reflect ray
        ray.dist = 999.; // just in case this is a pass-by-reference language...?
        ray.direction = reflect(ray.direction, hit.normal);
        ray.origin = hit.pos;
        ray.emitter = hit.ID; // make sure sure ray doesn't hit whatever it just hit again
    }
    // hit the sky (set to false for night mode :) )
    if(true)
    {
        // get material
        vec2 uv = gl_FragCoord.xy / resolution.xy;
		vec4 tx = texture(fondo,uv);
		vec3 surfaceColour = colourOfID(-1);         
        surfaceColour = vec3(tx);
		
		
		// accumulate through path
        // if we haven't hit anything, the path is fully open/white
        // if we have been reflecting around, pathColour will
        // attenuate how much sky gets to this pixel appropriately
        pixelColour += surfaceColour * pathColour * SKY_GAIN;
    }
    // gamma
    pixelColour = pow(pixelColour*1., vec3(1.6));
    
    // AO after gamma ... looks nice, idk
    pixelColour *= min(1.,pow(SphereOcclusion(screenPos, screenID),AO_POWER)+AO_GAIN);
    fragColor = vec4(pixelColour,1.0);
}