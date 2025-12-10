#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


glm::mat3 CreateRotationMatrix(const glm::vec3& up, float angle);

glm::mat4 CreateRotationMatrixMat4(const glm::vec3& up, float angle);