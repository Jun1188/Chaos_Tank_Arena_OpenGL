#version 460 core

uniform vec3 objectColor;
in vec3 WorldNormal;
out vec4 FragColor;

void main()
{
    vec3 lightDirection = normalize(vec3(-0.5, 1.0, 0.7));
    float diffuse = max(dot(normalize(WorldNormal), lightDirection), 0.0);
    vec3 linearColor = objectColor * (0.42 + 0.58 * diffuse);
    FragColor = vec4(pow(max(linearColor, vec3(0.0)), vec3(1.0 / 2.2)), 1.0);
}
