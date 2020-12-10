#include <stdio.h>
#include <windows.h>
#include <GL/gl.h>

#include "vcpkg\\installed\\x64-windows\\include\\detours\\detours.h"

static void (WINAPI * trueGlViewport)(GLint, GLint, GLsizei, GLsizei) = glViewport;
static void (WINAPI * trueGlScissor)(GLint, GLint, GLsizei, GLsizei) = glScissor;
static void (WINAPI * trueGlMatrixMode)(GLenum) = glMatrixMode;
static void (WINAPI * trueGlDrawArrays)(GLenum mode, GLint first, GLsizei count) = glDrawArrays;
static void (WINAPI * trueGlDrawElements)(GLenum mode, GLsizei count, GLenum type, const void * indices) = glDrawElements;

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

static Viewport currentViewport;

static Scissor currentScissor;

static GLenum currentMatrixMode;

void leftSide(void) {
    trueGlViewport(currentViewport.x, currentViewport.y, currentViewport.width/2, currentViewport.height);
    trueGlScissor(currentScissor.x, currentScissor.y, currentScissor.width/2, currentScissor.height);
    trueGlMatrixMode(GL_MODELVIEW_MATRIX);
    glPushMatrix();
    glTranslatef(-0.03, 0, 0);
}

void rightSide(void) {
    glPopMatrix();
    trueGlViewport(currentViewport.x + currentViewport.width/2, currentViewport.y, currentViewport.width/2, currentViewport.height);
    trueGlScissor(currentScissor.x + currentScissor.width/2, currentScissor.y, currentScissor.width/2, currentScissor.height);
    glPushMatrix();
    glTranslatef(0.03, 0, 0);
}


void neutral(void) {
    glPopMatrix();
    trueGlMatrixMode(currentMatrixMode);
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
    currentMatrixMode = mode;
    trueGlMatrixMode(mode);
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
        DetourAttach(&(PVOID&)trueGlMatrixMode, hookedGlMatrixMode);
        DetourAttach(&(PVOID&)trueGlDrawArrays, hookedGlDrawArrays);
        DetourAttach(&(PVOID&)trueGlDrawElements, hookedGlDrawElements);
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
        DetourDetach(&(PVOID&)trueGlMatrixMode, hookedGlMatrixMode);
        DetourDetach(&(PVOID&)trueGlDrawArrays, hookedGlDrawArrays);
        DetourDetach(&(PVOID&)trueGlDrawElements, hookedGlDrawElements);
        error = DetourTransactionCommit();

        printf("ogldet" DETOURS_STRINGIFY(DETOURS_BITS) ".dll:"
               " Removed detour glFinish() (result=%d)\n", error);
        fflush(stdout);
    }

    return TRUE;
}