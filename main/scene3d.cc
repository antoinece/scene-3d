#include <fstream>
#include <array>
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
#include "global_utility.h"
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
  //bloom
  Shader shader_light_ = {};
  Shader shader_blur_ = {};
  Shader shader_bloom_final_ = {};

  GLuint hdr_fbo_ = 0;
  GLuint color_buffer_[2] = {};
  GLuint rbo_depth_ = 0;

  //Pingpong for blur
  GLuint pingpong_fbo_[2] = {};
  GLuint pingpong_color_buffer_[2] = {};

  std::vector<glm::vec3> light_positions_ = {};
  std::vector<glm::vec3> light_colors_ = {};


  //model
  Shader shader_model_ = {};
  Model model_;

  //skybox
  Shader skybox_program_ = {};

  GLuint skybox_vao_ = 0;
  GLuint skybox_vbo_ = 0;

  unsigned int skybox_texture_ = -1;

  float skybox_vertices_[108] = {};

  //------------------------
  FreeCamera camera_ {};
  float elapsedTime_ = 0.0f;

  float model_scale_ = 15;
  bool bloom_state_ = false;
  float exposure_ = 0.2f;


};

void Scene3D::Begin()
{
  // configure global opengl state
  // -----------------------------
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_CULL_FACE);
  // glCullFace(GL_FRONT);


  //Build shaders
  shader_light_ = Shader("data/shaders/bloom/bloom.vert", "data/shaders/bloom/light.frag");
  shader_blur_ = Shader("data/shaders/bloom/blur.vert", "data/shaders/bloom/blur.frag");
  shader_bloom_final_ = Shader("data/shaders/bloom/bloom_final.vert", "data/shaders/bloom/bloom_final.frag");
  shader_model_ = Shader("data/shaders/scene3d/model.vert", "data/shaders/scene3d/model.frag");
  skybox_program_ = Shader("data/shaders/scene3d/cubemaps.vert", "data/shaders/scene3d/cubemaps.frag");


  model_ = Model("data/roman_baths/scene.gltf");

  //Configure FBO
  glGenFramebuffers(1, &hdr_fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo_);
  //We need 2 floating point color buffers, for normal rendering and brightness thresholds
  glGenTextures(2, color_buffer_);
  for (unsigned int i = 0; i < 2; i++)
  {
    glBindTexture(GL_TEXTURE_2D, color_buffer_[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1280, 720, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //be sure to clamp to edge!
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, color_buffer_[i], 0);
  }

  //create and attach depth buffer
  glGenRenderbuffers(1, &rbo_depth_);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1280, 720);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth_);
  //select color attachment
  unsigned int attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, attachments);
  // finally check if framebuffer is complete
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "Framebuffer not complete!" << std::endl;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  //Pingpong for blur
  glGenFramebuffers(2, pingpong_fbo_);
  glGenTextures(2, pingpong_color_buffer_);
  for (unsigned int i = 0; i < 2; i++)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[i]);
    glBindTexture(GL_TEXTURE_2D, pingpong_color_buffer_[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1280, 720, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpong_color_buffer_[i], 0);
    // also check if framebuffers are complete (no need for depth buffer)
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      std::cout << "Framebuffer not complete!" << std::endl;
  }

  // lighting info
  // -------------
  // positions
  light_positions_.push_back(glm::vec3(0.0f, 0.5f, 1.5f));
  light_positions_.push_back(glm::vec3(-4.0f, 0.5f, -3.0f));
  light_positions_.push_back(glm::vec3(3.0f, 0.5f, 1.0f));
  light_positions_.push_back(glm::vec3(-.8f, 2.4f, -1.0f));
  // colors
  light_colors_.push_back(glm::vec3(5.0f, 5.0f, 5.0f));
  light_colors_.push_back(glm::vec3(10.0f, 0.0f, 0.0f));
  light_colors_.push_back(glm::vec3(0.0f, 0.0f, 15.0f));
  light_colors_.push_back(glm::vec3(0.0f, 5.0f, 0.0f));

  static constexpr std::array skyboxVertices {
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


  std::vector<std::string_view> faces
      {
          "data/textures/skybox/right.jpg",
          "data/textures/skybox/left.jpg",
          "data/textures/skybox/top.jpg",
          "data/textures/skybox/bottom.jpg",
          "data/textures/skybox/front.jpg",
          "data/textures/skybox/back.jpg"
      };

  skybox_texture_ = GenerateCubemap(faces);



  // shader configuration
  // --------------------
  shader_blur_.Use();
  shader_blur_.SetInt("image", 0);
  shader_bloom_final_.Use();
  shader_bloom_final_.SetInt("scene", 0);
  shader_bloom_final_.SetInt("bloomBlur", 1);
  skybox_program_.Use();
  skybox_program_.SetInt("skybox", 0);
}

void Scene3D::End()
{
  shader_blur_.Delete();
  shader_light_.Delete();
  shader_bloom_final_.Delete();
  skybox_program_.Delete();

}

void Scene3D::Update(const float dt)
{
  UpdateCamera(dt);
  elapsedTime_ += dt;

  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // 1. render scene into floating point framebuffer
  // -----------------------------------------------
  glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo_);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  auto projectionB = glm::perspective(glm::radians(45.0f), (float)1280 / (float)720, 0.1f, 1000.0f);
  auto view = camera_.view();
  auto model = glm::mat4(1.0f);

  glActiveTexture(GL_TEXTURE0);

  shader_model_.Use();

  { // Create transformations
    auto projection = glm::perspective(glm::radians(45.0f), (float)1280 / (float)720, 0.1f, 10000.0f);
    auto view = camera_.view();

    shader_model_.SetMat4("projection", projection);
    shader_model_.SetMat4("view", view);

    shader_model_.SetVec3Array("lightPos", light_positions_, light_positions_.size());
    shader_model_.SetVec3Array("lightColor", light_colors_, light_colors_.size());

    const glm::vec3 view_pos = camera_.camera_position_;
    shader_model_.SetVec3("viewPos", glm::vec3(view_pos.x, view_pos.y, view_pos.z));

    //Draw model
    auto model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, model_scale_ * glm::vec3(1.0f, 1.0f, 1.0f));

    shader_model_.SetMat4("model", model);

    model_.Draw(shader_model_.id_);



  }



  // finally show all the light sources as bright cubes
  shader_light_.Use();
  shader_light_.SetMat4("projection", projectionB);
  shader_light_.SetMat4("view", view);

  for (unsigned int i = 0; i < light_positions_.size(); i++)
  {
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(light_positions_[i]));
    model = glm::scale(model, glm::vec3(0.25f));
    shader_light_.SetMat4("model", model);
    shader_light_.SetVec3("lightColor", light_colors_[i]);
    renderCube();
  }
  glBindVertexArray(0);






  glBindFramebuffer(GL_FRAMEBUFFER, 1);

  glDepthFunc(GL_LEQUAL); // Ensure skybox is drawn in the background
  glDepthMask(GL_FALSE);  // Disable depth writing


  skybox_program_.Use();
  glm::mat4 viewS = glm::mat4(glm::mat3(camera_.view())); // Remove translation
  glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)1280 / (float)720, 0.1f, 10000.0f);
  skybox_program_.SetMat4("view", viewS);
  skybox_program_.SetMat4("projection", projection);

  glBindVertexArray(skybox_vao_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);  // Re-enable depth writing
  glDepthFunc(GL_LESS);  // Restore normal depth function




  // 2. blur bright fragments with two-pass Gaussian Blur
  // --------------------------------------------------
  bool horizontal = true, first_iteration = true;
  unsigned int amount = 10;
  shader_blur_.Use();
  for (unsigned int i = 0; i < amount; i++)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[horizontal]);
    shader_blur_.SetInt("horizontal", horizontal);
    glBindTexture(GL_TEXTURE_2D, first_iteration ? color_buffer_[1] : pingpong_color_buffer_[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
    renderQuad();
    horizontal = !horizontal;
    if (first_iteration)
      first_iteration = false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // 3. now render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
  // --------------------------------------------------------------------------------------------------------------------------
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  shader_bloom_final_.Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, color_buffer_[0]);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, pingpong_color_buffer_[!horizontal]);
  shader_bloom_final_.SetInt("bloom", bloom_state_);
  shader_bloom_final_.SetFloat("exposure", exposure_);
  renderQuad();



}

void Scene3D::OnEvent(const SDL_Event& event)
{
  //TODO: Add zoom
}

void Scene3D::UpdateCamera(const float dt)
{
  // Get keyboard state
  const Uint8* state = SDL_GetKeyboardState(NULL);

  // Camera controls
  if (state[SDL_SCANCODE_W])
  {
    camera_.Move(FORWARD, dt);
  }
  if (state[SDL_SCANCODE_S])
  {
    camera_.Move(BACKWARD, dt);
  }
  if (state[SDL_SCANCODE_A])
  {
    camera_.Move(LEFT, dt);
  }
  if (state[SDL_SCANCODE_D])
  {
    camera_.Move(RIGHT, dt);
  }
  if (state[SDL_SCANCODE_SPACE])
  {
    camera_.Move(UP, dt);
  }
  if (state[SDL_SCANCODE_LCTRL])
  {
    camera_.Move(DOWN, dt);
  }

  int mouseX, mouseY;
  const Uint32 mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);
  if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT) && !ImGui::GetIO().WantCaptureMouse)
  {
    camera_.Update(mouseX, mouseY);
  }
}


void Scene3D::DrawImGui()
{
  ImGui::Begin("My Window"); // Start a new window

  //ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

  ImGui::Checkbox("Enable bloom", &bloom_state_);

  ImGui::SliderFloat("Exposure", &exposure_, 0.01f, 1.0f, "%.1f");

  // static ImVec4 LightColour = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default color
  // ImGui::ColorPicker3("Light Colour", reinterpret_cast<float*>(&light_colors_[0]));
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

