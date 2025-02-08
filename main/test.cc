
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <sstream>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "engine.h"
#include "file_utility.h"
#include "free_camera.h"
#include "model.h"
#include "scene3d.h"
#include "shader.h"
#include "texture_loader.h"

namespace gpr5300
{
class Scene3D final : public Scene
{
 public:
  void Begin() override;
  void End() override;
  void Update(float dt) override;
  void OnEvent(const SDL_Event& event) override;
  void DrawImGui() override;
  void UpdateCamera(const float dt) override;

 private:
  Shader program_ = {};
  Shader skybox_program_ = {};

  GLuint skybox_vao_ = 0;
  GLuint skybox_vbo_ = 0;

  Model model_;

  unsigned int skybox_texture_ = -1;

  float elapsedTime_ = 0.0f;

  float model_scale_ = 10;

  float skybox_vertices_[108] = {};

  FreeCamera* camera_ = nullptr;
};

void Scene3D::Begin()
{
  camera_ = new FreeCamera();
  // stbi_set_flip_vertically_on_load(true);
  glEnable(GL_DEPTH_TEST);

  program_ = Shader("data/shaders/scene3d/model.vert", "data/shaders/scene3d/model.frag");
  skybox_program_ = Shader("data/shaders/scene3d/cubemaps.vert", "data/shaders/scene3d/cubemaps.frag");

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  //model_ = Model("data/pickle_gltf/Pickle_uishdjrva_Mid.gltf");
  model_ = Model("data/roman_baths/scene.gltf");

  float skyboxVertices[] = {
      // positions
      -1.0f, 1.0f, -1.0f,
      -1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,

      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,
      -1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,

      -1.0f, 1.0f, -1.0f,
      1.0f, 1.0f, -1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f, -1.0f,

      -1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f, 1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f, 1.0f,
      1.0f, -1.0f, 1.0f
  };

  std::ranges::copy(skyboxVertices, skybox_vertices_);

  //skybox VAO
  glGenVertexArrays(1, &skybox_vao_);
  glGenBuffers(1, &skybox_vbo_);
  glBindVertexArray(skybox_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(skybox_vertices_), &skybox_vertices_, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);


  std::vector<std::string> faces
      {
          "data/textures/skybox/right.jpg",
          "data/textures/skybox/left.jpg",
          "data/textures/skybox/top.jpg",
          "data/textures/skybox/bottom.jpg",
          "data/textures/skybox/front.jpg",
          "data/textures/skybox/back.jpg"
      };

  // load textures
  skybox_texture_ = GenerateCubemap(faces);



  // shader configuration
  program_.Use();
  program_.SetInt("skybox", 0);

  skybox_program_.Use();
  skybox_program_.SetInt("skybox", 0);
}

void Scene3D::End()
{
  //Unload program/pipeline
  program_.Delete();
  skybox_program_.Delete();

}


void Scene3D::Update(const float dt)
{
  UpdateCamera(dt);
  elapsedTime_ += dt;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // also clear the depth buffer

  program_.Use();

  // Create transformations
  auto projection = glm::perspective(glm::radians(45.0f), (float)1280 / (float)720, 0.1f, 10000.0f);
  auto view = camera_->view();

  program_.SetMat4("projection", projection);
  program_.SetMat4("view", view);

  const glm::vec3 view_pos = camera_->camera_position_;
  program_.SetVec3("viewPos", glm::vec3(view_pos.x, view_pos.y, view_pos.z));

  //Draw model
  auto model = glm::mat4(1.0f);
  model = glm::translate(model, glm::vec3(1.0f, 0.0f, 0.0f));
  model = glm::scale(model, model_scale_ * glm::vec3(1.0f, 1.0f, 1.0f));

  program_.SetMat4("model", model);
  model_.Draw(program_.id_);

  glBindVertexArray(0);


  //Draw skybox
  glDepthFunc(GL_LEQUAL);
  skybox_program_.Use();
  view = glm::mat4(glm::mat3(camera_->view()));
  skybox_program_.SetMat4("view", view);
  skybox_program_.SetMat4("projection", projection);
  //Skybox cube
  glBindVertexArray(skybox_vao_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);
  glDepthFunc(GL_LESS);
}

void Scene3D::OnEvent(const SDL_Event& event)
{
  switch (event.type)
  {
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_LSHIFT)
      {
        camera_->ToggleSprint();
      }
      break;
    default:
      break;
  }
  //TODO: Add zoom
}

void Scene3D::UpdateCamera(const float dt)
{
  // Get keyboard state
  const Uint8* state = SDL_GetKeyboardState(NULL);

  // Camera controls
  if (state[SDL_SCANCODE_W])
  {
    camera_->Move(FORWARD, dt);
  }
  if (state[SDL_SCANCODE_S])
  {
    camera_->Move(BACKWARD, dt);
  }
  if (state[SDL_SCANCODE_A])
  {
    camera_->Move(LEFT, dt);
  }
  if (state[SDL_SCANCODE_D])
  {
    camera_->Move(RIGHT, dt);
  }
  if (state[SDL_SCANCODE_SPACE])
  {
    camera_->Move(UP, dt);
  }
  if (state[SDL_SCANCODE_LCTRL])
  {
    camera_->Move(DOWN, dt);
  }

  int mouseX, mouseY;
  const Uint32 mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);
  if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT) && !ImGui::GetIO().WantCaptureMouse)
  {
    camera_->Update(mouseX, mouseY);
  }
}

void Scene3D::DrawImGui()
{
  ImGui::Begin("My Window"); // Start a new window
  //ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
  //ImGui::SliderFloat("Model Size", &model_scale_, 10.0f, 100.0f, "%.1f");
  static ImVec4 LightColour = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default color
  ImGui::End(); // End the window
}
}

int main(int argc, char* argv[])
{
  gpr5300::Scene3D scene;
  gpr5300::Engine engine(&scene);
  engine.Run();

  return EXIT_SUCCESS;
}



/*

Minimum requirements:

Frustum Culling

Shadow Map

         3d model loading

Blinn-Phong shading model

  Bloom

Framebuffer

Back face culling

        Cubemaps

Deferred rendering

SSAO

Normal map

Gamma correction

 */