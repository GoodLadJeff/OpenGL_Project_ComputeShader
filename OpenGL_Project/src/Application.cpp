#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "Renderer.h"

#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Shader.h"
#include "Texture.h"

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 640

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    glfwSwapInterval(1);

    //calling glew init after the context of glfw
    if (glewInit() != GLEW_OK)
        std::cout << "Error!" << std::endl;
    //printing gl version
    std::cout << glGetString(GL_VERSION) << std::endl;

    //setting a new scope bc glGetError gives errors even if glfw terminated and so doesn't stop debugging when we close the app
    {
        float positions[] = {
            //vec pos     text pos
            -1.0f, -1.0f, 0.0f, 0.0f, // 0
             1.0f, -1.0f, 1.0f, 0.0f, // 1
             1.0f,  1.0f, 1.0f, 1.0f, // 2
            -1.0f,  1.0f, 0.0f, 1.0f  // 3
        };

        //index buffer (using the same vertices to draw an other triangle
        unsigned int indices[] =
        {
            0, 1, 2,
            2, 3, 0
        };

        GLCall(glEnable(GL_BLEND));
        GLCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

        //creating vao, vertex array object
        unsigned int vao;
        GLCall(glGenVertexArrays(1, &vao));
        GLCall(glBindVertexArray(vao));

        VertexArray va;
        VertexBuffer vb(positions, 4 * 4 * sizeof(float));

        VertexBufferLayout layout;
        layout.Push(GL_FLOAT, 2);
        layout.Push(GL_FLOAT, 2);
        va.AddBuffer(vb, layout);

        IndexBuffer ib(indices, 6);

        Shader shader("res/shaders/Basic.shader");
        shader.Bind();
        //shader.SetUniform4f("u_Color", 0.6f, 0.0f, 0.5f, 1.0f);

        //I have a texture class but it doesn't work with code generated images like this yet
        GLuint screenTex;
        glGenTextures(1, &screenTex);
        glBindTexture(GL_TEXTURE_2D, screenTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, SCREEN_WIDTH, SCREEN_HEIGHT);
        glBindImageTexture(0, screenTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        
        shader.SetUniform1i("u_Texture", 0);

        //COMPUTE SHADER
        //this is a ray tracer code, I can't say I could do this code alone, but I have at least understood it
        const char* computeShaderSource = R"( 
        #version 460 core
        layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
        layout(rgba32f, binding = 0) uniform image2D screen;

        void main()
        {
            vec4 pixel = vec4(0.075, 0.133, 0.173, 1.0);
            ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);

            ivec2 dims = imageSize(screen);
            float x = -(float(pixel_coords.x * 2 - dims.x) / dims.x); // transforms to [-1.0, 1.0]
            float y = (float(pixel_coords.y * 2 - dims.y) / dims.y);   // transforms to [-1.0, 1.0]

            float fov = 90.0;
            vec3 cam_o = vec3(0.0, 0.0, -tan(fov / 2.0));
            vec3 ray_o = vec3(x, y, 0.0);
            vec3 ray_d = normalize(ray_o - cam_o);

            vec3 sphere_c = vec3(0.0, 0.0, -5.0);
            float sphere_r = 1.0;

            vec3 o_c = ray_o - sphere_c;
            float b = dot(ray_d, o_c);
            float c = dot(o_c, o_c) - sphere_r * sphere_r;
            float intersectionState = b * b - c;
            vec3 intersection = ray_o + ray_d * (-b + sqrt(b * b - c));

            if (intersectionState >= 0.0)
            {
                pixel = vec4((normalize(intersection - sphere_c) + 1.0) / 2.0, 1.0);
            }
            imageStore(screen, pixel_coords, pixel);
        }

        )";

        GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(computeShader, 1, &computeShaderSource, NULL);
        glCompileShader(computeShader);

        GLuint computeProgram = glCreateProgram();
        glAttachShader(computeProgram, computeShader);
        glLinkProgram(computeProgram);

        va.Unbind();
        vb.Unbind();
        ib.Unbind();
        shader.Unbind();

        Renderer renderer;

        float r = 0.0f;
        float increment = 0.05f;

        while (!glfwWindowShouldClose(window))
        {
            renderer.Clear();

            //using the compute shader
            glUseProgram(computeProgram);
            // for performance purposes we divide all this the same manner as this layout(local_size_x = 8, local_size_y = 4, local_size_z = 1) in;
            glDispatchCompute(ceil(SCREEN_WIDTH / 8), ceil(SCREEN_HEIGHT / 4), 1); 
            glMemoryBarrier(GL_ALL_BARRIER_BITS);

            shader.Bind();

            renderer.Draw(va, ib, shader);

            if (r > 1.0f || r < 0.0f)
                increment *= -1;
            r += increment;

            /* Swap front and back buffers */
            glfwSwapBuffers(window);
            /* Poll for and process events */
            glfwPollEvents();
        }
    }

    glfwTerminate();
    return 0;
}