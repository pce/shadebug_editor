#include <iostream>
#include <string>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <lunasvg.h>  // LunaSVG library header

// Window resize callback
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

// Function to load and render SVG
bool RenderSVG(const std::string& svg_data, float width, float height, ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 canvas_p1) {
    using namespace lunasvg;

    // Load the SVG document
    auto document = Document::loadFromData(svg_data);
    if (!document) {
        std::cerr << "Failed to load SVG document" << std::endl;
        return false;
    }

    // Render the SVG document to a bitmap
    auto bitmap = document->renderToBitmap(width, height);
    
    // Check if the bitmap is valid by checking its dimensions
    if (bitmap.width() == 0 || bitmap.height() == 0) {
        std::cerr << "Failed to render SVG to a valid bitmap" << std::endl;
        return false;
    }

    // Render the bitmap (SVG image) onto the ImGui canvas
    // Cast the bitmap data to ImTextureID
    draw_list->AddImage((ImTextureID)(uintptr_t)bitmap.data(), canvas_p0, canvas_p1);
    return true;
}

int main() {
    std::cout << "Document Editor Initialized!" << std::endl;

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Set OpenGL version (macOS requires >= 3.2)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on macOS
#endif

    // Create a window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Document Editor", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    // Set up ImGui for GLFW and OpenGL
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // SVG data (for testing, use a simple SVG string)
    static std::string svg_data = "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='100'>"
                                  "<rect x='10' y='10' width='80' height='80' fill='blue'/></svg>";

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui content
        ImGui::Begin("Document Editor");
        ImGui::Text("Hello, world!");
        ImGui::End();

        // Page Preview panel
        static float scale = 1.0f;  // Scaling factor
        static int page_width = 210; // DIN A4 width in mm
        static int page_height = 297; // DIN A4 height in mm

        ImGui::Begin("Page Preview");
        ImGui::SliderFloat("Scale", &scale, 0.1f, 2.0f, "Scale = %.1f");
        ImGui::SliderInt("Width (mm)", &page_width, 100, 300);
        ImGui::SliderInt("Height (mm)", &page_height, 100, 400);
        ImGui::Text("Preview Area");

        // Calculate the canvas position and size
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
        ImVec2 canvas_size(page_width * scale, page_height * scale);
        ImVec2 canvas_p1(canvas_p0.x + canvas_size.x, canvas_p0.y + canvas_size.y);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255), 0.0f, ImDrawFlags_None, 2.0f);

        // Render SVG within the page
        if (!RenderSVG(svg_data, canvas_size.x, canvas_size.y, draw_list, canvas_p0, canvas_p1)) {
            std::cerr << "Failed to render SVG." << std::endl;
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
