#include <stdio.h>
#include <windows.h>
#include <map>
#include "vcpkg\\installed\\x64-windows\\include\\gl\\glew.h"
#include "vcpkg\\installed\\x64-windows\\include\\glm\\glm.hpp"
#include "vcpkg\\installed\\x64-windows\\include\\glm\\gtc\\matrix_transform.hpp"

#include "vcpkg\\installed\\x64-windows\\include\\detours\\detours.h"

static void (WINAPI * trueGlViewport)(GLint, GLint, GLsizei, GLsizei) = glViewport;
static void (WINAPI * trueGlScissor)(GLint, GLint, GLsizei, GLsizei) = glScissor;
//static void (WINAPI * trueGlMatrixMode)(GLenum) = glMatrixMode;
static void (WINAPI * trueGlDrawArrays)(GLenum mode, GLint first, GLsizei count) = glDrawArrays;
static void (WINAPI * trueGlDrawElements)(GLenum mode, GLsizei count, GLenum type, const void * indices) = glDrawElements;
static void (WINAPI * trueGlDrawElementsBaseVertex)(GLenum mode, GLsizei count, GLenum type, const void * indices, GLint baseVertex) = NULL;
static void (WINAPI * trueGlLinkProgram)(GLuint program) = NULL;
static void (WINAPI * trueGlUseProgram)(GLuint program) = NULL;

static GLenum (WINAPI * trueGlewInit)(VOID) = glewInit;

typedef struct Viewport {
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
} Viewport;

typedef struct Scissor {
    GLint x;
    GLint y;
    GLsizei width;
    GLsizei height;
} Scissor;

static std::map<GLuint, GLint> programEyeMatrixLocations;

static Viewport currentViewport;

static Scissor currentScissor;

static GLuint currentProgram;

void transposeTranslate(GLfloat x, GLfloat y, GLfloat z) {
    GLfloat T[16] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    x, y, z, 1 };

    glMultTransposeMatrixf(T);
}

void leftSide(void) {
    static glm::mat4 leftTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(0.03, 0, 0));

    trueGlViewport(currentViewport.x, currentViewport.y, currentViewport.width/2, currentViewport.height);
    trueGlScissor(currentScissor.x, currentScissor.y, currentScissor.width/2, currentScissor.height);

    auto uniformSearch = programEyeMatrixLocations.find(currentProgram);
    if(uniformSearch != programEyeMatrixLocations.end()) {
        GLint uniform = uniformSearch->second;
        glUniformMatrix4fv(uniform, 1, GL_FALSE, &leftTranslate[0][0]);
    }
   
}

void rightSide(void) {
    static glm::mat4 rightTranslate = glm::translate(glm::mat4(1.0f), glm::vec3(-0.03, 0, 0));

    trueGlViewport(currentViewport.x + currentViewport.width/2, currentViewport.y, currentViewport.width/2, currentViewport.height);
    trueGlScissor(currentScissor.x + currentScissor.width/2, currentScissor.y, currentScissor.width/2, currentScissor.height);

    auto uniformSearch = programEyeMatrixLocations.find(currentProgram);
    if(uniformSearch != programEyeMatrixLocations.end()) {
        GLint uniform = uniformSearch->second;
        glUniformMatrix4fv(uniform, 1, GL_FALSE, &rightTranslate[0][0]);
    }
}


void neutral(void) {
    //trueGlMatrixMode(currentMatrixMode);
    trueGlViewport(currentViewport.x, currentViewport.y, currentViewport.width, currentViewport.height);
    trueGlScissor(currentScissor.x, currentScissor.y, currentScissor.width, currentScissor.height);
}

void WINAPI hookedGlViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    currentViewport.x = x;
    currentViewport.y = y;
    currentViewport.width = width;
    currentViewport.height = height;
    trueGlViewport(x, y, width, height);
}

void WINAPI hookedGlScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    currentScissor.x = x;
    currentScissor.y = y;
    currentScissor.width = width;
    currentScissor.height = height;
    trueGlScissor(x, y, width, height);
}

void WINAPI hookedGlMatrixMode(GLenum mode) {
    //currentMatrixMode = mode;
    //trueGlMatrixMode(mode);
}

void WINAPI hookedGlDrawArrays(GLenum mode, GLint first, GLsizei count) {
    leftSide();
    trueGlDrawArrays(mode, first, count);
    rightSide();
    trueGlDrawArrays(mode, first, count);
    neutral();
}

void WINAPI hookedGlDrawElements(GLenum mode, GLsizei count, GLenum type, const void * indices) {
    leftSide();
    trueGlDrawElements(mode, count, type, indices);
    rightSide();
    trueGlDrawElements(mode, count, type, indices);
    neutral();
}

void WINAPI hookedGlDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void * indices, GLint baseVertex) {
    //return;
    leftSide();
    trueGlDrawElementsBaseVertex(mode, count, type, indices, baseVertex);
    rightSide();
    trueGlDrawElementsBaseVertex(mode, count, type, indices, baseVertex);
    neutral();
}

void WINAPI hookedGlLinkProgram(GLuint program) {
    trueGlLinkProgram(program);
    GLint uniformLocation = glGetUniformLocation(program, "sgEyeMatrix");
    if(uniformLocation != -1) {
        programEyeMatrixLocations.try_emplace(program, uniformLocation);
    }
}

void WINAPI hookedGlUseProgram(GLuint program) {
    currentProgram = program;
    trueGlUseProgram(program);
}

GLenum WINAPI hookedGlewInit(VOID) {
    GLenum result = trueGlewInit();
    
    trueGlDrawElementsBaseVertex = (void (WINAPI* )(GLenum, GLsizei, GLenum, const void *, GLint))wglGetProcAddress("glDrawElementsBaseVertex");
    trueGlLinkProgram = (void (WINAPI*)(GLuint))wglGetProcAddress("glLinkProgram");
    trueGlUseProgram = (void (WINAPI*)(GLuint))wglGetProcAddress("glUseProgram");
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)trueGlDrawElementsBaseVertex, hookedGlDrawElementsBaseVertex);
    DetourAttach(&(PVOID&)trueGlLinkProgram, hookedGlLinkProgram);
    DetourAttach(&(PVOID&)trueGlUseProgram, hookedGlUseProgram);
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

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)trueGlViewport, hookedGlViewport);
        DetourAttach(&(PVOID&)trueGlScissor, hookedGlScissor);
        //DetourAttach(&(PVOID&)trueGlMatrixMode, hookedGlMatrixMode);
        DetourAttach(&(PVOID&)trueGlDrawArrays, hookedGlDrawArrays);
        DetourAttach(&(PVOID&)trueGlDrawElements, hookedGlDrawElements);
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
        DetourDetach(&(PVOID&)trueGlViewport, hookedGlViewport);
        DetourDetach(&(PVOID&)trueGlScissor, hookedGlScissor);
        //DetourDetach(&(PVOID&)trueGlMatrixMode, hookedGlMatrixMode);
        DetourDetach(&(PVOID&)trueGlDrawArrays, hookedGlDrawArrays);
        DetourDetach(&(PVOID&)trueGlDrawElements, hookedGlDrawElements);
        DetourDetach(&(PVOID&)trueGlewInit, hookedGlewInit);
        error = DetourTransactionCommit();

        printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
               " Removed detour glFinish() (result=%d)\n", error);
        fflush(stdout);
    }

    return TRUE;
}