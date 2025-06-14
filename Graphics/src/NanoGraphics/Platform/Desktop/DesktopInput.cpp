#include "ngpch.h"
#include "DesktopInput.hpp"

#include "NanoGraphics/Core/Logging.hpp"
#include "NanoGraphics/Utils/Profiler.hpp"

namespace Nano::Graphics::Internal
{

#if defined(NG_PLATFORM_DESKTOP)
    ////////////////////////////////////////////////////////////////////////////////////
    // Methods
    ////////////////////////////////////////////////////////////////////////////////////
    bool DesktopInput::IsKeyPressed(Key keycode) const
    {
        int state = glfwGetKey(static_cast<GLFWwindow*>(m_NativeWindow), static_cast<int>(keycode));
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool DesktopInput::IsMousePressed(MouseButton button) const
    {
        int state = glfwGetMouseButton(static_cast<GLFWwindow*>(m_NativeWindow), static_cast<int>(button));
        return state == GLFW_PRESS;
    }

    Nano::Graphics::Maths::Vec2<double> DesktopInput::GetCursorPosition() const
    {
        double xPos, yPos;
        glfwGetCursorPos(static_cast<GLFWwindow*>(m_NativeWindow), &xPos, &yPos);

        return { xPos, yPos };
    }

    void DesktopInput::SetCursorPosition(Nano::Graphics::Maths::Vec2<double> position) const
    {
        glfwSetCursorPos(static_cast<GLFWwindow*>(m_NativeWindow), position.x, position.y);
    }

    void DesktopInput::SetCursorMode(CursorMode mode) const
    {
        glfwSetInputMode(static_cast<GLFWwindow*>(m_NativeWindow), GLFW_CURSOR, static_cast<int>(mode));
    }

    std::string DesktopInput::GetClipboard() const
    {
        const char* cStr = glfwGetClipboardString(static_cast<GLFWwindow*>(m_NativeWindow));
        if (cStr)
            return std::string(cStr);

        return {};
    }

    void DesktopInput::SetClipboard(const std::string& input) const
    {
        glfwSetClipboardString(static_cast<GLFWwindow*>(m_NativeWindow), input.c_str());
    }
#endif

}