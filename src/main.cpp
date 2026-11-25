#include <limits>
#include <vector>

#include <Application.inl>

int main()
{
    Application app{1280, 720};
    const int result = app.Init("Chaos Tank Arena");
    if (result != 0)
        return result;

    app.Run();
    return 0;
}

void Application::OnInit()
{
    AppendCamera(glm::vec3{0.0f, 0.0f, 3.0f});
}

void Application::Update()
{
}

void Application::Clear()
{
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Application::Render(const glm::mat4&, const glm::mat4&, int)
{
}
