
#version 430
out vec4 FragColor;

//in vec2 TexCoord;
in vec3 ourColor;

void main()
{
    FragColor =  vec4(ourColor,1); //mix(texture(texture1, TexCoord), texture(texture2, TexCoord), 0.6);
}