//
// Created by cfans on 2018/3/3.
//

#include "GLUtil.h"

GLuint compileShaderCode(const char * shaderCode, GLenum shaderType){

    GLuint shaderHandle = glCreateShader(shaderType);
    glShaderSource(shaderHandle, 1, &shaderCode, NULL);
    glCompileShader(shaderHandle);
    GLint compileSuccess;
    glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &compileSuccess);
    if (compileSuccess == GL_FALSE) {
        GLchar messages[256];
        glGetShaderInfoLog(shaderHandle, sizeof(messages), 0, &messages[0]);
        LOGE("glGetShaderiv ShaderIngoLog: %s", messages);
    }
    return shaderHandle;
}
