#include <stdio.h>
#include <windows.h>
//#include <GL/gl.h>

#include "vcpkg\\installed\\x64-windows\\include\\detours\\detours.h"
#include "vcpkg\\installed\\x64-windows\\include\\gl\\glew.h"


static GLuint (WINAPI * trueGlCreateProgram)(void) = NULL;
static int (WINAPI * trueEntry)(VOID) = NULL;
static GLenum (WINAPI * trueGlewInit)(VOID) = glewInit;

const char* GEOMETRY_SHADER = R"glsl(
    #version 330 core
    layout(triangles) in;
    layout(triangle_strip, max_vertices=6) out;

    void main() {
        for(int i = 0; i < 3; i++) {
            gl_Position = gl_in[i].gl_Position;
            gl_Position.x = gl_Position.x * 0.5 - gl_Position.w * 0.5;
            //gl_Position.x -= 0.06 * gl_Position.w;
            EmitVertex();
        }
        EndPrimitive();
        for(int i = 0; i < 3; i++) {
            gl_Position = gl_in[i].gl_Position;
            gl_Position.x = gl_Position.x * 0.5 + gl_Position.w * 0.5;
            //gl_Position.x += 0.06 * gl_Position.w;
            EmitVertex();
        }
        EndPrimitive();
    }


)glsl";

GLuint WINAPI hookedGlCreateProgram() {
    //MessageBoxW(NULL, L"Hooked Link Program", L"Hook", 0);
    GLuint result = trueGlCreateProgram();
    GLuint shader = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(shader, 1, &GEOMETRY_SHADER, NULL);
    glCompileShader(shader);
    glAttachShader(result, shader);
    return result;
    
}



GLenum WINAPI hookedGlewInit(VOID) {
    //MessageBoxW(NULL, L"Hooked entry", L"Hook", 0);
    GLenum result = trueGlewInit();
    
    trueGlCreateProgram = (GLuint (WINAPI* )())wglGetProcAddress("glCreateProgram");
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)trueGlCreateProgram, hookedGlCreateProgram);
    DetourTransactionCommit();
    return result;
}





BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved)
{
    LONG error;
    (void)hinst;
    (void)reserved;

    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        DetourRestoreAfterWith();

        printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
               " Starting.\n");
        fflush(stdout);

        trueEntry = (int (WINAPI *)())DetourGetEntryPoint(NULL);

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)trueGlewInit, hookedGlewInit);
        error = DetourTransactionCommit();

        if (error == NO_ERROR) {
            printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
                   " Detoured glFinish().\n");
        }
        else {
            printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
                   " Error detouring glFinish(): %d\n", error);
        }
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
       // DetourDetach(&(PVOID&)trueGlLinkProgram, hookedGlLinkProgram);
        error = DetourTransactionCommit();

        printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
               " Removed detour glFinish() (result=%d)\n", error);
        fflush(stdout);
    }

    return TRUE;
}