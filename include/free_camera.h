#ifndef FREE_CAMERA_H
#define FREE_CAMERA_H
#include <glm/fwd.hpp>
#include <glm/vec3.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include "model.h"

enum Camera_Movement { FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN };

struct FreeCamera {

  glm::vec3 camera_position_ = glm::vec3(0.0f, 0.0f, -3.0f);
  glm::vec3 camera_target_ = glm::vec3(0.0f, 0.0f, 0.0f);
  glm::vec3 camera_direction_ = glm::normalize(camera_position_ - camera_target_);

  glm::vec3 world_up_ = glm::vec3(0.0f, 1.0f, 0.0f);

  glm::vec3 camera_right_ = glm::normalize(glm::cross(world_up_, camera_direction_));
  glm::vec3 camera_up_ = glm::normalize(glm::cross(camera_direction_, camera_right_));
  glm::vec3 camera_front_ = glm::vec3(0.0f, 0.0f, 1.0f);


  glm::mat4 view_ = glm::lookAt(camera_position_, camera_position_ + camera_front_, camera_up_);
  glm::vec3 direction_;
  float yaw_ = 90.0f;
  float pitch_ = 0.0f;
  float sensitivity_ = 0.1f;

  float camera_speed_ = 10.0f;
  bool sprint_ = false;


  float z_near_;
  float z_far_ ;

  void Update(const int x_yaw, const int y_pitch)
  {

    yaw_ += static_cast<float>(x_yaw) * sensitivity_;
    pitch_ -= static_cast<float>(y_pitch) * sensitivity_;
    if(pitch_ > 89.0f)
      pitch_ =  89.0f;
    if(pitch_ < -89.0f)
      pitch_ = -89.0f;
    direction_.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    direction_.y = sin(glm::radians(pitch_));
    direction_.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    camera_front_ = glm::normalize(direction_);

    view_ = glm::lookAt(camera_position_, camera_position_ + camera_front_, camera_up_);
  }

  void ToggleSprint()
  {
    sprint_ = !sprint_;
  }

  void Move(const Camera_Movement direction, const float dt)
  {
    float base_speed = camera_speed_ * dt;
    float speed = sprint_ ? 10.0f * base_speed : base_speed;
    if (direction == FORWARD)
    {
      camera_position_ += speed * camera_front_;
    }
    if (direction == BACKWARD)
    {
      camera_position_ -= speed * camera_front_;
    }
    if (direction == LEFT)
    {
      camera_position_ -= glm::normalize(glm::cross(camera_front_, camera_up_)) * speed;
    }
    if (direction == RIGHT)
    {
      camera_position_ += glm::normalize(glm::cross(camera_front_, camera_up_)) * speed;
    }
    if (direction == UP)
    {
      camera_position_ += world_up_ * speed;
    }
    if (direction == DOWN)
    {
      camera_position_ -= world_up_ * speed;
    }
    view_ = glm::lookAt(camera_position_, camera_position_ + camera_front_, camera_up_);
  }

  glm::mat4 GetViewMatrix() const { return glm::lookAt(camera_position_, camera_position_ + camera_front_, camera_up_);}

  glm::mat4 view() const { return view_;}


};

struct Sphere {
 private:
  glm::vec3 center_{0.f, 0.f, 0.f};
  float radius_{10.f};

 public:
  Sphere(const glm::vec3 &inCenter, float inRadius) : center_{inCenter}, radius_{inRadius} {}

  [[nodiscard]] glm::vec3 center() const{return center_;}
  [[nodiscard]] float radius() const{return radius_;}
};


struct Plane {
  glm::vec3 normal = {0.f, 1.f, 0.f}; // unit vector
  float distance = 0.f;        // Distance with origin

  Plane() = default;

  Plane(const glm::vec3 &p1, const glm::vec3 &norm)
      : normal(glm::normalize(norm)),
        distance(glm::dot(normal, p1)) {}

  [[nodiscard]] float GetSignedDistanceToPlaneFromACircle(const Sphere &circle) const {
    return glm::dot(normal, circle.center()) - distance;
  }
};




struct Frustum {
 private:
  Plane topFace;
  Plane bottomFace;

  Plane rightFace;
  Plane leftFace;

  Plane farFace;
  Plane nearFace;
  std::array<Plane, 6> planes_ = {topFace, bottomFace, rightFace, leftFace, nearFace, farFace};


 public:

  void CreateFrustumFromCamera(const FreeCamera &cam, float aspect, float fovY, float zNear, float zFar) {
    const float halfVSide = zFar * tanf(fovY * 0.5f);
    const float halfHSide = halfVSide * aspect;
    const glm::vec3 frontMultFar = zFar * cam.camera_front_;

    nearFace = {cam.camera_position_ + zNear * cam.camera_front_, cam.camera_front_};
    farFace = {cam.camera_position_ + frontMultFar, -cam.camera_front_};
    rightFace = {cam.camera_position_, glm::cross(frontMultFar - cam.camera_right_ * halfHSide, cam.camera_up_)};
    leftFace = {cam.camera_position_, glm::cross(cam.camera_up_, frontMultFar + cam.camera_right_ * halfHSide)};
    topFace = {cam.camera_position_, glm::cross(cam.camera_right_, frontMultFar - cam.camera_up_ * halfVSide)};
    bottomFace = {cam.camera_position_, glm::cross(frontMultFar + cam.camera_up_ * halfVSide, cam.camera_right_)};
  }

  void Update(const glm::mat4& projView) {
    // Extraire les plans du frustum à partir de la matrice projetée-vue (projView)
    glm::mat4 matrix = glm::transpose(projView); // Assurez-vous de transposer la matrice si nécessaire



    // Plage de valeurs pour les plans
    topFace.normal = glm::vec3(matrix[0][3] + matrix[0][0], matrix[1][3] + matrix[1][0], matrix[2][3] + matrix[2][0]);
    bottomFace.normal = glm::vec3(matrix[0][3] - matrix[0][0], matrix[1][3] - matrix[1][0], matrix[2][3] - matrix[2][0]);
    rightFace.normal = glm::vec3(matrix[0][3] - matrix[0][1], matrix[1][3] - matrix[1][1], matrix[2][3] - matrix[2][1]);
    leftFace.normal = glm::vec3(matrix[0][3] + matrix[0][1], matrix[1][3] + matrix[1][1], matrix[2][3] + matrix[2][1]);
    nearFace.normal = glm::vec3(matrix[0][3] + matrix[0][2], matrix[1][3] + matrix[1][2], matrix[2][3] + matrix[2][2]);
    farFace.normal = glm::vec3(matrix[0][3] - matrix[0][2], matrix[1][3] - matrix[1][2], matrix[2][3] - matrix[2][2]);

    // Calculer les distances de chaque plan à l'origine (le point d'intersection du plan et de la vue)
    topFace.distance = -(matrix[0][3] + matrix[0][0]);
    bottomFace.distance = -(matrix[0][3] - matrix[0][0]);
    rightFace.distance = -(matrix[0][3] - matrix[0][1]);
    leftFace.distance = -(matrix[0][3] + matrix[0][1]);
    nearFace.distance = -(matrix[0][3] + matrix[0][2]);
    farFace.distance = -(matrix[0][3] - matrix[0][2]);

    // Normaliser les vecteurs normaux des plans
    topFace.normal = glm::normalize(topFace.normal);
    bottomFace.normal = glm::normalize(bottomFace.normal);
    rightFace.normal = glm::normalize(rightFace.normal);
    leftFace.normal = glm::normalize(leftFace.normal);
    nearFace.normal = glm::normalize(nearFace.normal);
    farFace.normal = glm::normalize(farFace.normal);
  }

  bool IsObjectInFrustum(const Model& model) const {
    // Calculer la boîte englobante de l'objet (par exemple, avec les coins du modèle)
    glm::vec3 min, max;
    model.GetBoundingBox(min, max);

    // Vérifier si la boîte est à l'intérieur du frustum (cela dépend de votre implémentation du frustum)
    return IsAABBInFrustum(min, max);
  }


  bool IsAABBInFrustum(const glm::vec3& min, const glm::vec3& max) const {
    for (int i = 0; i < 6; ++i) {
      const Plane& plane = planes_[i]; // Utilise une référence constante

      // Les 8 coins de la boîte englobante
      glm::vec3 corners[8] = {
          glm::vec3(min.x, min.y, min.z), // coin inférieur-gauche-arrière
          glm::vec3(min.x, min.y, max.z), // coin inférieur-gauche-avant
          glm::vec3(min.x, max.y, min.z), // coin supérieur-gauche-arrière
          glm::vec3(min.x, max.y, max.z), // coin supérieur-gauche-avant
          glm::vec3(max.x, min.y, min.z), // coin inférieur-droit-arrière
          glm::vec3(max.x, min.y, max.z), // coin inférieur-droit-avant
          glm::vec3(max.x, max.y, min.z), // coin supérieur-droit-arrière
          glm::vec3(max.x, max.y, max.z)  // coin supérieur-droit-avant
      };

      bool inside = false;
      for (int j = 0; j < 8; ++j) {
        // Vérifie si un coin de la boîte est à l'intérieur du plan
        if (glm::dot(plane.normal, corners[j]) + plane.distance > 0) {
          inside = true;
          break;
        }
      }

      if (!inside)
        return false;
    }

    return true;
  }


  [[nodiscard]] bool IsSphereInFrustum(const Sphere &sphere) const {
    // Pour chaque plan du frustum
    std::array<Plane, 6> allPlanes = {topFace, bottomFace, rightFace, leftFace, nearFace, farFace};

    for (const auto &plane : allPlanes) {
      // Calcul de la distance entre le centre de la sphère et le plan
      float distance = plane.GetSignedDistanceToPlaneFromACircle(sphere);

      // Si la distance est plus grande que le rayon de la sphère, la sphère est à l'extérieur du frustum
      if (distance < -sphere.radius()) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] bool IsCubeInFrustum(const glm::vec3 &center, float halfSize) const {
    glm::vec3 vertices[8] = {
        center + glm::vec3(-halfSize, -halfSize, -halfSize),
        center + glm::vec3(-halfSize, -halfSize,  halfSize),
        center + glm::vec3(-halfSize,  halfSize, -halfSize),
        center + glm::vec3(-halfSize,  halfSize,  halfSize),
        center + glm::vec3( halfSize, -halfSize, -halfSize),
        center + glm::vec3( halfSize, -halfSize,  halfSize),
        center + glm::vec3( halfSize,  halfSize, -halfSize),
        center + glm::vec3( halfSize,  halfSize,  halfSize)
    };

    const Plane planes[] = {topFace, bottomFace, rightFace, leftFace, nearFace, farFace};
    for (const auto &plane : planes) {
      bool allOutside = true;

      for (const auto &vertex : vertices) {
        float distance = glm::dot(plane.normal, vertex) - plane.distance;
        if (distance >= 0) {
          allOutside = false;
          break; // Au moins un sommet est devant ce plan
        }
      }

      if (allOutside) {
        return false;
      }
    }

    return true;
  }

};



#endif //FREE_CAMERA_H
