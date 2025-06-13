#pragma once

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <math.h>

class Camera {
private:

	glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 cameraRight;

	double lastX;
	double lastY;

	float yaw;
	float pitch;
	float roll;

	glm::vec3 direction;
	bool firstMouse;

	void TakeInput(float deltaTime, GLFWwindow* window) {
		float cameraSpeed = 10.0f * deltaTime;

		if (glfwGetKey(window, GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(window, true);

		if (glfwGetKey(window, GLFW_KEY_W))  cameraPos += cameraSpeed * cameraFront;
		if (glfwGetKey(window, GLFW_KEY_S))  cameraPos -= cameraSpeed * cameraFront;

		if (glfwGetKey(window, GLFW_KEY_D)) cameraPos += cameraSpeed * cameraRight;
		if (glfwGetKey(window, GLFW_KEY_A)) cameraPos -= cameraSpeed * cameraRight;
	}



public:
	Camera() {
		Initialize();
	}

	~Camera() {

	}

	void Initialize() {
		lastX = (double)1920 * 0.5;
		lastY = (double)1080 * 0.5;
		firstMouse = true;

		yaw = -90.0f;
		pitch = 0.0f;
		roll = 0.0f;

		direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		direction.y = sin(glm::radians(pitch));
		direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	}

	void UpdateCamera(float deltaTime, GLFWwindow* window, glm::mat4* view) {
		cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
		TakeInput(deltaTime, window);
		*view = glm::lookAt(cameraPos, cameraFront + cameraPos, cameraUp);
	}

	void MouseInput(GLFWwindow* window, double xpos, double ypos) {

		if (firstMouse) {
			lastX = xpos;
			lastY = ypos;
			firstMouse = false;
		}

		const float senstivity = 0.1f;

		float x = senstivity * (xpos - lastX);
		float y = senstivity * (ypos - lastY);
		lastX = xpos;
		lastY = ypos;

		pitch += y;
		yaw += x;

		if (pitch > 89.5f) pitch = 89.5f;
		if (pitch < -89.5f) pitch = -89.5f;

		direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
		direction.y = sin(glm::radians(pitch));
		direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

		cameraFront = glm::normalize(direction);
	}

	inline glm::vec3 worldPos() { return cameraPos; }
};