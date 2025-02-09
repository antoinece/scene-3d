#version 300 es
precision highp float;

out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform vec3 lightPos[4];
uniform vec3 lightColor[4];
uniform vec3 viewPos;

void main()
{
    vec3 textureColor = texture(texture_diffuse1, TexCoords).rgb;
    vec3 norm = normalize(Normal);
    vec3 result = vec3(0.0); // Accumulate the light contributions

    for (int i = 0; i < 4; i++) {
        vec3 lightDir = normalize(lightPos[i] - FragPos);

        // Diffuse lighting
        float diff = max(dot(norm, lightDir), 0.0);

        // View direction
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);

        // Specular lighting
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

        // Light Attenuation (distance-based falloff)
        float distance = length(lightPos[i] - FragPos);
        float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));

        // Lighting components
        vec3 ambient = 0.2 * textureColor;
        vec3 diffuse = diff * textureColor * lightColor[i];
        vec3 specular = spec * lightColor[i];

        // Accumulate lighting from all lights
        result += (ambient + diffuse + specular) * attenuation;
    }

    FragColor = vec4(result, 1.0);
}
