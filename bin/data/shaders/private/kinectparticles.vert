#version 330
uniform vec2 targetSize;
uniform float pointSize, zoom, colorCycle, brightness;
uniform int mirror;
uniform float depthFocalY, surfaceContrast, plumeOpacity;
in vec4 position;
in vec4 color;
out vec4 particleColor;
out float sourceLayer;
void main() {
    vec3 p=position.xyz;
    if(mirror!=0)p.x=-p.x;
    // Fixed camera at the sensor. OF FBO textures use a top-left origin;
    // writing clip coordinates directly must invert Y once for texture output.
    float distance=max(.3,p.z);
    float aspect=targetSize.x/max(1.,targetSize.y);
    gl_Position=vec4(p.x*1.9*zoom/aspect,-p.y*1.9*zoom,
        ((10.1/9.9)*distance-2./9.9),distance);
    // Fine plume grains should not inflate into discs as they approach the camera.
    gl_PointSize=clamp(pointSize,1.,8.);
    sourceLayer=color.g;
    if(sourceLayer>.5)
        gl_PointSize=clamp(pointSize*.7*3.5/distance,1.,8.);
    // A native depth pixel has a distance-independent projected footprint.
    // Separate grains retain the fine point-cloud texture.
    if(sourceLayer>.5 && sourceLayer<.9)
        gl_PointSize=clamp(targetSize.y*.95*zoom/max(depthFocalY,1.)*.7*pointSize/1.2,1.,12.);
    float age=color.r;
    // Reference: a luminous white depth silhouette with red detached material.
    // The hue control rotates only the plume, preserving the white source.
    vec3 hue=clamp(abs(fract(vec3(0.,2./3.,1./3.)+colorCycle)*6.-3.)-1.,0.,1.);
    vec3 plume=mix(vec3(.85),mix(vec3(.012),vec3(1.),hue),smoothstep(0.,.06,age));
    vec3 palette=sourceLayer>.5?vec3(color.b):plume;
    if(sourceLayer>.5 && sourceLayer<.9){
        // Apply detail strength once, as luminance, not alpha per overlapping
        // sample. Smooth exposure preserves slopes even at brightness=2.5.
        float shaped=pow(clamp(color.b,0.,1.),mix(.65,2.2,clamp(surfaceContrast*.5,0.,1.)));
        float exposed=1.-exp(-shaped*max(brightness,0.)*1.4);
        particleColor=vec4(vec3(exposed*color.a),1.);
    }else{
        float alpha=sourceLayer<.5?sqrt(max(0.,color.a))*plumeOpacity:color.a;
        particleColor=vec4(palette*brightness,alpha);
    }
}
