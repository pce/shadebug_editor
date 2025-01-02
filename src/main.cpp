#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thorvg.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <string>
#include <iostream>

// Function to initialize ThorVG
bool initializeThorVG()
{
    // Initialize ThorVG with software engine and 0 threads
    if (tvg::Initializer::init(0, tvg::CanvasEngine::Sw) != tvg::Result::Success) {
        return false;
    }
    return true;
}

// Function to create and render an SVG string
void renderSVG(tvg::Canvas* canvas, const std::string& svgData, float scale, int page_width, int page_height)
{
    // Create a ThorVG Picture
    auto picture = tvg::Picture::gen();
    if (picture->load(svgData.c_str(), svgData.size(), "image/svg+xml") == tvg::Result::Success) {
        // Scale the picture
        picture->scale(scale);
        picture->size(page_width * scale, page_height * scale);
        // Add the picture to the canvas
        canvas->push(std::move(picture));
    }
}

// Function to create a texture from the buffer
GLuint createTextureFromBuffer(uint32_t* buffer, int width, int height)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return textureID;
}

int main()
{
    // Initialize ThorVG
    if (!initializeThorVG()) {
        return -1;
    }

    // Initialize GLFW
    if (!glfwInit()) {
        return -1;
    }

    // Configure GLFW to use OpenGL 3.2 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create a GLFW window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Document Editor", NULL, NULL);
    if (window == NULL) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader (GLAD)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150"); // Update GLSL version

    // Get framebuffer size and set target for ThorVG
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    uint32_t *buffer = new uint32_t[fbWidth * fbHeight];

    // SVG string to render
    const std::string svgData = R"(
        <svg width="100" height="100">
            <circle cx="50" cy="50" r="40" stroke="black" stroke-width="3" fill="red" />
        </svg>
    )";

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        glfwPollEvents();

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Page Preview panel
        static float scale = 1.0f;  // Scaling factor
        static int page_width = 210; // DIN A4 width in mm
        static int page_height = 297; // DIN A4 height in mm

        ImGui::Begin("Page Preview");
        ImGui::SliderFloat("Scale", &scale, 0.1f, 2.0f, "Scale = %.1f");
        ImGui::InputInt("Width (mm)", &page_width);
        ImGui::InputInt("Height (mm)", &page_height);

        ImGui::Text("Preview Area");

        // Calculate the canvas position and size
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size(page_width * scale, page_height * scale);
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_size.x, canvas_p0.y + canvas_size.y);

        ImDrawList *draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(128, 128, 128, 255), 0.0f, ImDrawFlags_None, 5.0f);

        // Create a ThorVG Canvas
        auto canvas = tvg::SwCanvas::gen();
        canvas->target(buffer, fbWidth * 4, fbWidth, fbHeight, tvg::ColorSpace::ABGR8888);

        // Render the SVG string
        renderSVG(canvas, svgData, scale, page_width, page_height);

        // Draw ThorVG canvas
        canvas->draw();
        canvas->sync();

        // Create a texture from the buffer
        GLuint textureID = createTextureFromBuffer(buffer, fbWidth, fbHeight);

        // Render the texture
        glBindTexture(GL_TEXTURE_2D, textureID);
        draw_list->AddImage((ImTextureID)(intptr_t)textureID, canvas_p0, canvas_p1);
        glDeleteTextures(1, &textureID);

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    delete[] buffer;
    glfwDestroyWindow(window);
    glfwTerminate();

    // Terminate ThorVG
    tvg::Initializer::term();

    return 0;
}
