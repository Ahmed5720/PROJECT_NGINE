#version 430 core
in vec4 vColor;
in float vLife;
out vec4 FragColor;
void main()
{
    vec2 coord = gl_PointCoord - vec2(0.5);
    // Calculate distance from center
    float dist = length(coord);
    
    // Discard fragments outside the circle (radius = 0.5)
    if (dist > 0.5)
        discard;
    
    float alpha = 0.8;
    if (dist > 0.45) {
        // Smooth the edge for better antialiasing
        alpha *= (0.5 - dist) * 10.0;
    }
    
    FragColor = vec4(vColor.xyz, alpha);
}