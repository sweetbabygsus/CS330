#include <iostream>
#include <cstdlib>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
//image loading utility functions
#include "stb_image1.h"

//GLM math headers
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//camera class
#include "camera.h"

#include <vector>
#define _USE_MATH_DEFINES
#ifndef M_PI
const double M_PI = 3.14159265358979323846;
#endif
#include <glm/gtc/constants.hpp>
//standard namespace
using namespace std;

/* Shader program Macro */
#ifndef GLSL
#define GLSL(Version, Source) "#version " #Version " core \n" #Source
#endif

namespace {
    const char* const WINDOW_TITLE = "3D scene";
    //variables for window width and height
    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;

    //stores GL data relative to a given mesh
    struct GLMesh
    {
        GLuint vao;         // Handle for the vertex array object
        GLuint vbo[2];      // Handle for the vertex buffer object
        GLuint ebo;         // Handle for the element buffer object
        GLuint nVertices;   // Number of vertices of the mesh
        GLuint nIndices;    // Number of indices of the mesh
    };

    //main glfw window
    GLFWwindow* gWindow = nullptr;
    GLMesh gMesh;
    GLMesh gPlaneMesh;
    GLMesh gSphereMesh;
    GLuint gProgramId;
    GLuint gTextureId; // Texture ID


    // camera
    Camera gCamera(glm::vec3(0.0f, 0.0f, 3.0f));
    float gLastX = WINDOW_WIDTH / 2.0f;
    float gLastY = WINDOW_HEIGHT / 2.0f;
    bool gFirstMouse = true;

    // timing
    // time between current frame and last frame
    float gDeltaTime = 0.0f;
    float gLastFrame = 0.0f;

    bool gIsPerspective = true; // Variable to track the current view mode


}

//user definded Functions prototypes
bool UInitialize(int, char* [], GLFWwindow** window);
void UResizeWindow(GLFWwindow* window, int width, int height);
void UProcessInput(GLFWwindow* window);
void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos);
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void UCreateMesh(GLMesh& mesh);
void UDestroyMesh(GLMesh& mesh);
// Function to create plane mesh
void UCreatePlaneMesh(GLMesh& mesh);
// Function to destroy plane mesh
void UDestroyPlaneMesh(GLMesh& mesh);
void UCreateSphereMesh(GLMesh& mesh);

void URender();
bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId);
void UDestroyShaderProgram(GLuint programId);


//vertex shader source
const GLchar* vertexShaderSource = GLSL(440,
    layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoord;

out vec2 vertexTexCoord;
out vec3 FragPos;
out vec3 Normal;

//global variables for transformation matrices
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(position, 1.0f);

    //Calculate texture coordinates based on vertex position
    vertexTexCoord = vec2(position.x + 0.5, position.y + 0.5);

    //Pass the fragment position and normal in view space to the fragment shader
    FragPos = vec3(model * vec4(position, 1.0));
    Normal = mat3(transpose(inverse(model))) * vec3(0.0, 0.0, 1.0);
}
);
//fragment shader source
const GLchar* fragmentShaderSource = GLSL(440,
    in vec2 vertexTexCoord;
in vec3 FragPos;
in vec3 Normal;

out vec4 fragmentColor;

uniform sampler2D textureSampler;
// Light position in world space
uniform vec3 lightPos;
void main() {
    // Flip the texture vertically
    vec2 flippedTexCoord = vec2(vertexTexCoord.x, 1.0 - vertexTexCoord.y);

    // Calculate distance (light direction) between light source and fragments
    vec3 lightDir = normalize(lightPos - FragPos);

    // Calculate diffuse impact by generating dot product of normal and light
    float diffuseStrength = max(dot(normalize(Normal), lightDir), 0.0);

    //Final color by combining the texture color and diffuse lighting
    vec4 texColor = texture(textureSampler, flippedTexCoord);
    // Office yellow color
    vec3 diffuseColor = vec3(1.0, 0.95, 0.5);
    vec3 finalColor = texColor.rgb * diffuseColor * diffuseStrength;

    fragmentColor = vec4(finalColor, texColor.a);
}
);

// Images are loaded with Y axis going down, but OpenGL's Y axis goes up, so let's flip it
void flipImageVertically(unsigned char* image, int width, int height, int channels)
{
    for (int j = 0; j < height / 2; ++j)
    {
        int index1 = j * width * channels;
        int index2 = (height - 1 - j) * width * channels;

        for (int i = width * channels; i > 0; --i)
        {
            unsigned char tmp = image[index1];
            image[index1] = image[index2];
            image[index2] = tmp;
            ++index1;
            ++index2;
        }
    }
}


int main(int argc, char* argv[]) {
    if (!UInitialize(argc, argv, &gWindow))
        return EXIT_FAILURE;

    //cylinder mesh
    UCreateMesh(gMesh);

    // Create the plane mesh
    UCreatePlaneMesh(gPlaneMesh);

    UCreateSphereMesh(gSphereMesh);


    //create shader program
    if (!UCreateShaderProgram(vertexShaderSource, fragmentShaderSource, gProgramId))
        return EXIT_FAILURE;

    // Load texture image
    int textureWidth, textureHeight, numChannels;
    unsigned char* textureData = stbi_load("texture.jpg", &textureWidth, &textureHeight, &numChannels, 0);
    if (!textureData) {
        std::cout << "Failed to load texture image" << std::endl;
        return EXIT_FAILURE;
    }

    // Create texture
    glGenTextures(1, &gTextureId);
    glBindTexture(GL_TEXTURE_2D, gTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureWidth, textureHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(textureData);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    //render loop
    while (!glfwWindowShouldClose(gWindow)) {
        // per-frame timing
        float currentFrame = glfwGetTime();
        gDeltaTime = currentFrame - gLastFrame;
        gLastFrame = currentFrame;

        //input
        UProcessInput(gWindow);
        URender();
        glfwPollEvents();
    }

    //release mesh data
    UDestroyMesh(gMesh);
    // Release texture
    UDestroyPlaneMesh(gPlaneMesh);
    //release shader program
    UDestroyShaderProgram(gProgramId);

    //terminate program
    exit(EXIT_SUCCESS);
}

bool UInitialize(int argc, char* argv[], GLFWwindow** window) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    //GLFW window creation
    * window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (*window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(*window);
    glfwSetFramebufferSizeCallback(*window, UResizeWindow);
    glfwSetCursorPosCallback(*window, UMousePositionCallback);
    glfwSetScrollCallback(*window, UMouseScrollCallback);
    glfwSetMouseButtonCallback(*window, UMouseButtonCallback);


    // tell GLFW to capture our mouse
    glfwSetInputMode(*window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    //use GLFW version 1.13 or earlier
    glewExperimental = GL_TRUE;
    GLenum GlewInitResult = glewInit();

    if (GLEW_OK != GlewInitResult) {
        std::cerr << glewGetErrorString(GlewInitResult) << std::endl;
        return false;
    }

    //display GPU OPENGL VERSION
    cout << "INFO: OpenGL Version: " << glGetString(GL_VERSION) << endl;

    return true;
}

//process all input
void UProcessInput(GLFWwindow* window)
{
    static const float cameraSpeed = 2.5f;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        gCamera.ProcessKeyboard(FORWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        gCamera.ProcessKeyboard(BACKWARD, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        gCamera.ProcessKeyboard(LEFT, gDeltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        gCamera.ProcessKeyboard(RIGHT, gDeltaTime);
    // Move camera up
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        gCamera.Position += cameraSpeed * gDeltaTime * gCamera.Up;

    // Move camera down
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        gCamera.Position -= cameraSpeed * gDeltaTime * gCamera.Up;

    // Toggle view mode between perspective and orthographic
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        gIsPerspective = !gIsPerspective;

        // Adjust viewport based on the view mode
        int width, height;
        glfwGetWindowSize(gWindow, &width, &height);
        glViewport(0, 0, width, height);
    }
}

//whenever the window changes
void UResizeWindow(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void UMousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    if (gFirstMouse)
    {
        gLastX = xpos;
        gLastY = ypos;
        gFirstMouse = false;
    }

    float xoffset = xpos - gLastX;
    // reversed since y-coordinates go from bottom to top
    float yoffset = gLastY - ypos;

    gLastX = xpos;
    gLastY = ypos;

    gCamera.ProcessMouseMovement(xoffset, yoffset);
}


// glfw: whenever the mouse scroll wheel scrolls, this callback is called
void UMouseScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    gCamera.ProcessMouseScroll(yoffset);
}

// glfw: handle mouse button events
void UMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
    {
        if (action == GLFW_PRESS)
            cout << "Left mouse button pressed" << endl;
        else
            cout << "Left mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_MIDDLE:
    {
        if (action == GLFW_PRESS)
            cout << "Middle mouse button pressed" << endl;
        else
            cout << "Middle mouse button released" << endl;
    }
    break;

    case GLFW_MOUSE_BUTTON_RIGHT:
    {
        if (action == GLFW_PRESS)
            cout << "Right mouse button pressed" << endl;
        else
            cout << "Right mouse button released" << endl;
    }
    break;

    default:
        cout << "Unhandled mouse button event" << endl;
        break;
    }
}


//function called to render the fram
void URender() {
    glEnable(GL_DEPTH_TEST);

    //clear teh background
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set the light position in world space
    glm::vec3 lightPosition(1.0f, 1.0f, 1.0f);

    // Direction of the directional light source
    glm::vec3 gDirectionalLightDirection(-1.0f, 0.0f, 0.0f);

    // Get the uniform location for the light position
    GLint lightPosLoc = glGetUniformLocation(gProgramId, "lightPos");
    // Pass the light position to the shader
    glUniform3fv(lightPosLoc, 1, glm::value_ptr(lightPosition));

    //render the cylinder mesh
        // 1. Scales the shape down by half of its original size in all 3 dimensions
    glm::mat4 scale = glm::scale(glm::vec3(0.5f, 0.5f, 0.5f));

    //Rotate shape on the z axis
    glm::mat4 rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));

    //translate on y axis
    glm::mat4 translation = glm::translate(glm::vec3(1.5f, 0.0f, 0.0f));

    glm::mat4 model = translation * rotation * scale;



    // camera/view transformation
    glm::mat4 view = gCamera.GetViewMatrix();

    glm::mat4 projection = glm::perspective(45.0f, static_cast<GLfloat>(WINDOW_WIDTH) / static_cast<GLfloat>(WINDOW_HEIGHT), 0.1f, 100.0f);

    if (gIsPerspective)
    {
        // Perspective projection
        projection = glm::perspective(glm::radians(gCamera.Zoom), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 100.0f);
    }
    else
    {
        // Orthographic projection
        float orthoSize = 5.0f; // Adjust this value based on your scene's scale
        projection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 100.0f);
    }
    glUseProgram(gProgramId);

    GLint modelLoc = glGetUniformLocation(gProgramId, "model");
    GLint viewLoc = glGetUniformLocation(gProgramId, "view");
    GLint projLoc = glGetUniformLocation(gProgramId, "projection");
    GLint textureLoc = glGetUniformLocation(gProgramId, "textureSampler");

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

    glBindTexture(GL_TEXTURE_2D, gTextureId);
    glUniform1i(textureLoc, 0); // Set texture unit 0

    glBindVertexArray(gMesh.vao);
    glDrawElements(GL_TRIANGLES, gMesh.nIndices, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);

    // Render the sphere mesh
    glm::mat4 sphereModel = glm::mat4(1.0f);
    glm::mat4 sphereTranslation = glm::translate(glm::vec3(1.5f, 0.10f, 0.0f));
    sphereModel = sphereTranslation * sphereModel;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sphereModel));

    glBindVertexArray(gSphereMesh.vao);
    glDrawElements(GL_TRIANGLES, gSphereMesh.nIndices, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);


    // Render the plane mesh
    scale = glm::scale(glm::vec3(4.0f, 2.0f, 2.0f));
    rotation = glm::rotate(0.0f, glm::vec3(1.0, 1.0f, 1.0f));
    translation = glm::translate(glm::vec3(0.0f, -0.25f, 0.0f));
    model = translation * rotation * scale;

    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glBindVertexArray(gPlaneMesh.vao);
    glDrawElements(GL_TRIANGLES, gPlaneMesh.nIndices, GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);

    glfwSwapBuffers(gWindow);
}

//implements the UCreateMesh function
void UCreateMesh(GLMesh& mesh) {
    const float radius = 0.5f;
    const float height = 1.0f;
    const int sectors = 36;
    const int circleSegments = 36;
    const float M_PI = 3.14159265358979323846f;

    std::vector<GLfloat> vertices;
    std::vector<GLushort> indices;

    // Create cylinder vertices
    float sectorStep = 2 * M_PI / sectors;
    for (int i = 0; i <= sectors; ++i) {
        float angle = i * sectorStep;
        float x = radius * cos(angle);
        float y = -height / 2.0f;
        float z = radius * sin(angle);
        // Texture coordinate in s direction
        float s = static_cast<float>(i) / sectors;
        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);
        // Texture coordinate s
        vertices.push_back(s);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);

        vertices.push_back(x);
        vertices.push_back(y + height);
        vertices.push_back(z);
        // Texture coordinate s
        vertices.push_back(s);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back(1.0f);
    }

    // Create circle vertices at the bottom
    float circleStep = 2 * M_PI / circleSegments;
    for (int i = 0; i < circleSegments; ++i) {
        float angle = i * circleStep;
        float x = radius * cos(angle);
        float y = -height / 2.0f;
        float z = radius * sin(angle);

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(z);
        // Texture coordinate s
        vertices.push_back(0.5f);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
    }

    // Create indices for the cylinder sides
    for (int i = 0; i < sectors; ++i) {
        indices.push_back(i * 2);
        indices.push_back(i * 2 + 1);
        indices.push_back((i * 2 + 2) % (sectors * 2));

        indices.push_back((i * 2 + 2) % (sectors * 2));
        indices.push_back(i * 2 + 1);
        indices.push_back((i * 2 + 3) % (sectors * 2));
    }

    // Create indices for the circle at the bottom
    int baseVertexIndex = sectors * 2;
    for (int i = 0; i < circleSegments - 1; ++i) {
        indices.push_back(baseVertexIndex);
        indices.push_back(baseVertexIndex + i + 1);
        indices.push_back(baseVertexIndex + i + 2);
    }
    indices.push_back(baseVertexIndex);
    indices.push_back(baseVertexIndex + circleSegments);
    indices.push_back(baseVertexIndex + 1);

    const GLuint floatsPerVertex = 7;

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(3, mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    mesh.nIndices = indices.size();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.vbo[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indices.size(), indices.data(), GL_STATIC_DRAW);

    GLint stride = sizeof(GLfloat) * floatsPerVertex;

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(GLfloat) * 3));
    glEnableVertexAttribArray(1);
}

void UDestroyMesh(GLMesh& mesh) {
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(2, mesh.vbo);
}

void UCreateSphereMesh(GLMesh& mesh) {
    const float radius = 0.25f;
    const int latitudeDivisions = 36;
    const int longitudeDivisions = 36;

    std::vector<GLfloat> vertices;
    std::vector<GLushort> indices;

    // Create vertices and texture coordinates for the sphere
    for (int lat = 0; lat <= latitudeDivisions; ++lat) {
        float theta = lat * glm::pi<float>() / latitudeDivisions;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int lon = 0; lon <= longitudeDivisions; ++lon) {
            float phi = lon * 2.0f * glm::pi<float>() / longitudeDivisions;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            float u = 1.0f - static_cast<float>(lon) / longitudeDivisions;
            float v = 1.0f - static_cast<float>(lat) / latitudeDivisions;

            vertices.push_back(radius * x);
            vertices.push_back(radius * y);
            vertices.push_back(radius * z);
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }

    // Create indices for the sphere
    for (int lat = 0; lat < latitudeDivisions; ++lat) {
        for (int lon = 0; lon < longitudeDivisions; ++lon) {
            int first = lat * (longitudeDivisions + 1) + lon;
            int second = first + longitudeDivisions + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    const GLuint floatsPerVertex = 5;

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(2, mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    mesh.nIndices = indices.size();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.vbo[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indices.size(), indices.data(), GL_STATIC_DRAW);

    GLint stride = sizeof(GLfloat) * floatsPerVertex;

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(GLfloat) * 3));
    glEnableVertexAttribArray(1);
}


void UCreatePlaneMesh(GLMesh& mesh) {
    // Define the vertices of the plane
    std::vector<GLfloat> vertices = {
        // Positions         // Texture coordinates
        -0.5f, 0.0f, -0.5f, 0.0f, 0.0f,
         0.5f, 0.0f, -0.5f, 1.0f, 0.0f,
        -0.5f, 0.0f,  0.5f, 0.0f, 1.0f,
         0.5f, 0.0f,  0.5f, 1.0f, 1.0f
    };

    // Define the indices of the plane
    std::vector<GLushort> indices = {
        0, 1, 2,
        2, 1, 3
    };

    const GLuint floatsPerVertex = 5;

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(2, mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    mesh.nIndices = indices.size();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.vbo[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indices.size(), indices.data(), GL_STATIC_DRAW);

    GLint stride = sizeof(GLfloat) * floatsPerVertex;

    // Specify the vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(GLfloat) * 3));
    glEnableVertexAttribArray(1);
}

void UDestroyPlaneMesh(GLMesh& mesh) {
    glDeleteVertexArrays(1, &mesh.vao);
    glDeleteBuffers(2, mesh.vbo);
}

bool UCreateShaderProgram(const char* vtxShaderSource, const char* fragShaderSource, GLuint& programId) {
    // Create and compile vertex shader
    GLuint vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShaderId, 1, &vtxShaderSource, nullptr);
    glCompileShader(vertexShaderId);

    // Check vertex shader compilation status
    GLint vertexShaderStatus;
    glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &vertexShaderStatus);
    if (vertexShaderStatus == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(vertexShaderId, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(vertexShaderId, maxLength, &maxLength, &errorLog[0]);
        glDeleteShader(vertexShaderId);
        std::cerr << "Vertex shader compilation failed: " << &errorLog[0] << std::endl;
        return false;
    }

    // Create and compile fragment shader
    GLuint fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShaderId, 1, &fragShaderSource, nullptr);
    glCompileShader(fragmentShaderId);

    // Check fragment shader compilation status
    GLint fragmentShaderStatus;
    glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &fragmentShaderStatus);
    if (fragmentShaderStatus == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(fragmentShaderId, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(fragmentShaderId, maxLength, &maxLength, &errorLog[0]);
        glDeleteShader(fragmentShaderId);
        glDeleteShader(vertexShaderId);
        std::cerr << "Fragment shader compilation failed: " << &errorLog[0] << std::endl;
        return false;
    }

    // Create and link the shader program
    programId = glCreateProgram();
    glAttachShader(programId, vertexShaderId);
    glAttachShader(programId, fragmentShaderId);
    glLinkProgram(programId);

    // Check shader program linking status
    GLint programLinkStatus;
    glGetProgramiv(programId, GL_LINK_STATUS, &programLinkStatus);
    if (programLinkStatus == GL_FALSE) {
        GLint maxLength = 0;
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> errorLog(maxLength);
        glGetProgramInfoLog(programId, maxLength, &maxLength, &errorLog[0]);
        glDeleteProgram(programId);
        glDeleteShader(vertexShaderId);
        glDeleteShader(fragmentShaderId);
        std::cerr << "Shader program linking failed: " << &errorLog[0] << std::endl;
        return false;
    }

    // Clean up
    glDetachShader(programId, vertexShaderId);
    glDetachShader(programId, fragmentShaderId);
    glDeleteShader(vertexShaderId);
    glDeleteShader(fragmentShaderId);

    return true;
}

void UDestroyShaderProgram(GLuint programId) {
    glDeleteProgram(programId);
}