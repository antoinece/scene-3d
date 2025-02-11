﻿#ifndef MODEL_H
#define MODEL_H
#include <iostream>
#include <GL/glew.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <span>

#include "mesh.h"
#include "stb_image.h"
#include "texture_loader.h"

unsigned int TextureFromFile(const char *path, const std::string &directory, bool gamma = false);

class Model
{
 public:
  Model() = default;
  explicit Model(const char* path)
  {
    LoadModel(path);
  }

  void Draw(GLuint& shader)
  {
    for (auto& meshe : meshes_)
      meshe.Draw(shader);
  }

  [[nodiscard]] std::vector<Mesh> meshes(){return meshes_;}
  [[nodiscard]] std::vector<Texture> get_textures_loaded(){return textures_loaded;}

 private:

  //Model data
  std::vector<Texture> textures_loaded;	//Make sure textures are loaded once.
  std::vector<Mesh> meshes_;
  std::string directory_;

 public:
  void GetBoundingBox(glm::vec3& min, glm::vec3& max) const {
    // Initialiser les coordonnées min et max à des valeurs opposées
    min = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX); // Valeurs maximales possibles
    max = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX); // Valeurs minimales possibles

    // Parcours de tous les meshes du modèle
    for (const Mesh& mesh : meshes_) {
      for (const Vertex& vertex : mesh.vertices_) {
        // Mettre à jour min et max pour chaque sommet
        min = glm::min(min, vertex.Position);
        max = glm::max(max, vertex.Position);
      }
    }
  }


  void LoadModel(const std::string& path)
  {
    //stbi_set_flip_vertically_on_load(true);//uncomment for .obj
    Assimp::Importer import;

    const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
      std::cerr << "ERROR::ASSIMP::" << import.GetErrorString() << "\n";
      return;
    }
    directory_ = path.substr(0, path.find_last_of('/'));

    ProcessNode(scene->mRootNode, scene);
  }

  void ProcessNode(aiNode* node, const aiScene* scene)
  {
    // process all the node's meshes (if any)
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
      aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
      meshes_.push_back(ProcessMesh(mesh, scene));
    }
    // then do the same for each of its children
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
      ProcessNode(node->mChildren[i], scene);
    }
  }

  Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene)
  {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    //Process vertex
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
      Vertex vertex{};
      glm::vec3 vector;
      //Positions
      vector.x = mesh->mVertices[i].x;
      vector.y = mesh->mVertices[i].y;
      vector.z = mesh->mVertices[i].z;
      vertex.Position = vector;
      //Normals
      vector.x = mesh->mNormals[i].x;
      vector.y = mesh->mNormals[i].y;
      vector.z = mesh->mNormals[i].z;
      vertex.Normal = vector;
      //TexCoords
      if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
      {
        glm::vec2 vec;
        vec.x = mesh->mTextureCoords[0][i].x;
        vec.y = mesh->mTextureCoords[0][i].y;
        vertex.TexCoords = vec;
      }
      else
      {
        vertex.TexCoords = glm::vec2(0.0f, 0.0f);
      }
      vertices.push_back(vertex);
    }

    //Process indices
    for(unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
      aiFace face = mesh->mFaces[i];
      for(unsigned int j = 0; j < face.mNumIndices; j++)
        indices.push_back(face.mIndices[j]);
    }

    //Process material
    if(mesh->mMaterialIndex >= 0)
    {
      aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
      std::vector<Texture> diffuseMaps = LoadMaterialTextures(material,
                                                              aiTextureType_DIFFUSE, "texture_diffuse");
      textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
      std::vector<Texture> specularMaps = LoadMaterialTextures(material,
                                                               aiTextureType_SPECULAR, "texture_specular");
      textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    }

    return {vertices, indices, textures};
  }

  std::vector<Texture> LoadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName)
  {
    std::vector<Texture> textures;
    for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
      aiString str;
      mat->GetTexture(type, i, &str);
      bool skip = false;
      for(unsigned int j = 0; j < textures_loaded.size(); j++)
      {
        if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
        {
          textures.push_back(textures_loaded[j]);
          skip = true;
          break;
        }
      }
      if(!skip)
      {   // if texture hasn't been loaded already, load it
        Texture texture;
        texture.id = TextureFromFile(str.C_Str(), this->directory_);
        texture.type = typeName;
        texture.path = str.C_Str();
        textures.push_back(texture);
        textures_loaded.push_back(texture); // add to loaded textures
      }
    }
    return textures;
  }
};




unsigned int TextureFromFile(const char *path, const std::string &directory, bool gamma)
{
  std::string filename = std::string(path);
  filename = directory + '/' + filename;

  unsigned int textureID;
  glGenTextures(1, &textureID);

  int width, height, nrComponents;
  unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
  if (data)
  {
    GLenum internal_format;
    GLenum data_format;
    if (nrComponents == 1)
    {
      internal_format = data_format = GL_RED;
    }
    else if (nrComponents == 3)
    {
      internal_format = gamma ? GL_SRGB : GL_RGB;
      data_format = GL_RGB;
    }
    else if (nrComponents == 4)
    {
      internal_format = gamma ? GL_SRGB_ALPHA : GL_RGBA;
      data_format = GL_RGBA;
    }


    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, data_format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, data_format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, data_format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
  }
  else
  {
    std::cout << "Texture failed to load at path: " << path << std::endl;
    stbi_image_free(data);
  }

  return textureID;
}
unsigned int GenerateCubemap(std::span<const std::string_view> faces_paths)
{
  unsigned int textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

  int width, height, nrChannels;
  for (unsigned int i = 0; i < faces_paths.size(); i++)
  {
    unsigned char *data = stbi_load(faces_paths[i].data(), &width, &height, &nrChannels, 0);
    if (data)
    {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
      stbi_image_free(data);
    }
    else
    {
      std::cout << "Cubemap texture failed to load at path: " << faces_paths[i] << std::endl;
      stbi_image_free(data);
    }
  }
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  return textureID;
}

#endif //MODEL_H
