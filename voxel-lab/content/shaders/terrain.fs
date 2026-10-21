#version 330
in vec3 worldPosition;
in vec4 color;
uniform vec4 colDiffuse;
uniform vec3 cameraPosition;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogEnd;
out vec4 finalColor;
void main(){
    vec3 cell=floor(worldPosition*5.0);
    float grain=fract(sin(dot(cell,vec3(12.9898,78.233,39.425)))*43758.5453);
    vec3 surface=color.rgb*colDiffuse.rgb*(0.96+0.08*grain);
    float dist=length(worldPosition-cameraPosition);
    float fog=smoothstep(fogStart,fogEnd,dist);
    finalColor=vec4(mix(surface,fogColor,fog),color.a*colDiffuse.a);
}
