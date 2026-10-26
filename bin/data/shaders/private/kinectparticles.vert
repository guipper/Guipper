#version 330
uniform vec2 targetSize;
uniform float pointSize, zoom, colorCycle, brightness;
uniform int mirror;
uniform float depthFocalY, surfaceContrast, plumeOpacity;
uniform float surfaceSize, plumeSize, plumeBrightness, plumeVariation;
uniform int surfaceGrains;
uniform float surfaceVariation, surfaceJitter;
in vec4 position;
in vec4 color;
out vec4 particleColor;
out float sourceLayer;
out float surfaceCoverage;
void main() {
    vec3 p=position.xyz;
    if(mirror!=0)p.x=-p.x;
    // Fixed camera at the sensor. OF FBO textures use a top-left origin;
    // writing clip coordinates directly must invert Y once for texture output.
    float distance=max(.3,p.z);
    float aspect=targetSize.x/max(1.,targetSize.y);
    gl_Position=vec4(p.x*1.9*zoom/aspect,-p.y*1.9*zoom,
        ((10.1/9.9)*distance-2./9.9),distance);
    sourceLayer=color.g;
    surfaceCoverage=1.;
    float footprint=targetSize.y*.95*zoom/max(depthFocalY,1.);
    if(sourceLayer>.5){
        // Surface geometry never inherits the plume's point-size control.
        // Both shapes respect the requested diameter, including subpixel sizes.
        float coverage=surfaceSize;
        float requested=footprint*coverage;
        if(surfaceGrains!=0){
            // Source pixel identity stays stable through depth changes and holes.
            vec3 random=fract(sin(color.r*vec3(127.1,311.7,74.7)+vec3(1.,2.,3.))*43758.5453)*2.-1.;
            requested*=1.+random.x*surfaceVariation;
            vec2 offset=random.yz*footprint*surfaceJitter;
            if(mirror!=0)offset.x=-offset.x;
            gl_Position.xy+=2.*offset/targetSize*gl_Position.w;
        }
        // Rasterizers cannot draw geometry smaller than one pixel reliably.
        // Preserve subpixel area instead of flattening every small size to 1 px.
        surfaceCoverage=min(1.,requested*requested);
        gl_PointSize=clamp(requested,1.,64.);
    }else{
        // A fixed per-particle seed varies size without flickering each frame.
        float seed=color.b;
        float variation=mix(1.,mix(.55,1.45,seed),plumeVariation);
        gl_PointSize=clamp(pointSize*plumeSize*variation*mix(1.,.55,color.r),1.,64.);
    }
    float age=color.r;
    // Reference: a luminous white depth silhouette with red detached material.
    // The hue control rotates only the plume, preserving the white source.
    vec3 hue=clamp(abs(fract(vec3(0.,2./3.,1./3.)+colorCycle)*6.-3.)-1.,0.,1.);
    vec3 plume=mix(vec3(.85),mix(vec3(.012),vec3(1.),hue),smoothstep(0.,.06,age));
    vec3 palette=sourceLayer>.5?vec3(color.b):plume;
    if(sourceLayer>.5){
        // Apply detail strength once, as luminance, not alpha per overlapping
        // sample. Smooth exposure preserves slopes even at brightness=2.5.
        float shaped=pow(clamp(color.b,0.,1.),mix(.65,2.2,clamp(surfaceContrast*.5,0.,1.)));
        float exposed=1.-exp(-shaped*max(brightness,0.)*1.4);
        particleColor=vec4(vec3(exposed*color.a),1.);
    }else{
        float alpha=sourceLayer<.5?sqrt(max(0.,color.a))*plumeOpacity:color.a;
        particleColor=vec4(palette*(sourceLayer<.5?plumeBrightness:brightness),alpha);
    }
}
