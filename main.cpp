#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <stdio.h>
#include <assert.h>
#include <dlfcn.h>
#include <sys/stat.h>

static void error_callback(int _, const char* desc)
{
    fprintf(stderr, "ERROR: %s\n", desc);
}

int main()
{
    glfwInit();
    glfwSetErrorCallback(error_callback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
//    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    const int screenScale = 40, RES_X = 16*screenScale, RES_Y = 9*screenScale;
    GLFWwindow *window1;
    { // window1
        int screenWidth, screenHeight;
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        glfwGetMonitorWorkarea(primaryMonitor, NULL, NULL, &screenWidth, &screenHeight);

        window1 = glfwCreateWindow(RES_X, RES_Y, "#1", NULL, NULL);
        glfwMakeContextCurrent(window1);
        glfwSetWindowPos(window1, screenWidth-RES_X, 0);
        glfwSetWindowAttrib(window1, GLFW_FLOATING, GLFW_TRUE);
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    }

    int loadShader1(GLuint prog, const char *filename);
    int loadShader3(GLuint prog, const char *filename);
    int loadShader3x(long *, GLuint prog, const char *filename);

    GLuint prog1 = glCreateProgram();
    assert(loadShader1(prog1, "../Voxel/base.frag") == 0);

    GLuint prog2 = glCreateProgram();
    GLuint prog3 = glCreateProgram();
    long lastModTime2;
    long lastModTime3;

    GLuint tex1;
    glGenTextures(1, &tex1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, RES_X, RES_Y);

    GLuint tex2;
    const int Size = 64;
    glGenTextures(1, &tex2);
    glBindTexture(GL_TEXTURE_3D, tex2);
    glTexStorage3D(GL_TEXTURE_3D, 5, GL_R8, Size,Size,Size);

    int counter = 0;
    GLuint abo;
    glGenBuffers(1, &abo);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, abo);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof counter, &counter, GL_STATIC_DRAW);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, abo);

//    glfwSwapInterval(0); // vsync
    while (!glfwWindowShouldClose(window1))
    {
        bool dirty3 = loadShader3x(&lastModTime3, prog3, "../Voxel/genVoxel.glsl");
        loadShader3x(&lastModTime2, prog2, "../Voxel/renderVoxel.glsl");

        if (dirty3)
        {
            glBindImageTexture(0, tex2, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R8);
            glUseProgram(prog3);
            glDispatchCompute(Size/8,Size/8,Size/8);
            glGenerateMipmap(GL_TEXTURE_3D);

            glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof counter, &counter);
            printf("counter : %d\n", counter);
        }

        static float t0 = 0;
        static uint32_t frame = 0;
        ++frame;
        float t1 = glfwGetTime();
        float dt = t1 - t0; t0 = t1;
        float fps;
        if ((frame & 0xf) == 0)
        {
            fps = 1./dt;
        }
        char title[32];
        sprintf(title, "%4.2f\t\t%.1f fps\t\t%d x %d", t1, fps, RES_X, RES_Y);
        glfwSetWindowTitle(window1, title);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_3D, tex2);
        glBindImageTexture(0, tex1, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
        glUseProgram(prog2);
        glProgramUniform1f(prog2, 0, t1);
        glDispatchCompute(screenScale,screenScale,1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex1);
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog1);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glfwSwapBuffers(window1);
        glfwPollEvents();
    }

    int err = glGetError();
    if (err) fprintf(stderr, "ERROR: %x\n", err);
    glfwDestroyWindow(window1);
    glfwTerminate();
}

static void detachShaders(GLuint prog)
{
    GLsizei numShaders;
    GLuint shaders[5];
    glGetAttachedShaders(prog, 5, &numShaders, shaders);
    for (int i=0; i<numShaders; i++)
    {
        glDetachShader(prog, shaders[i]);
    }
}

int loadShader1(GLuint prog, const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "ERROR: file %s not found.", filename);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    rewind(f);
    char version[32];
    fgets(version, sizeof(version), f);
    length -= ftell(f);
    char source1[length+1]; source1[length] = 0; // set null terminator
    fread(source1, length, 1, f);
    fclose(f);

    detachShaders(prog);
    {
        const char *string[] = { version, source1 };
        const GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, sizeof string/sizeof *string, string, NULL);
        glCompileShader(fs);
        int success;
        glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            int length;
            glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &length);
            char message[length];
            glGetShaderInfoLog(fs, length, &length, message);
            fprintf(stderr, "ERROR: fail to compile fragment shader. file %s\n%s\n", filename, message);
            return 2;
        }
        glAttachShader(prog, fs);
        glDeleteShader(fs);
    }
    {
        const char vsSource[] = R"(
        precision mediump float;
        void main() {
            vec2 UV = vec2(gl_VertexID%2, gl_VertexID/2)*2.-1.;
            gl_Position = vec4(UV, 0, 1);
        }
        )";

        const char *string[] = { version, vsSource };
        const GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 2, string, NULL);
        glCompileShader(vs);
        int success;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
        assert(success);
        glAttachShader(prog, vs);
        glDeleteShader(vs);
    }

    glLinkProgram(prog);
    glValidateProgram(prog);
    return 0;
}

int loadShader3(GLuint prog, const char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f)
    {
        fprintf(stderr, "ERROR: file %s not found.", filename);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    rewind(f);
    char version[32];
    fgets(version, sizeof(version), f);
    length -= ftell(f);
    char source1[length+1]; source1[length] = 0; // set null terminator
    fread(source1, length, 1, f);
    fclose(f);

    detachShaders(prog);
    {
        const char *string[] = { version, source1 };
        const GLuint sha = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(sha, sizeof string/sizeof *string, string, NULL);
        glCompileShader(sha);
        int success;
        glGetShaderiv(sha, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            int length;
            glGetShaderiv(sha, GL_INFO_LOG_LENGTH, &length);
            char message[length];
            glGetShaderInfoLog(sha, length, &length, message);
            fprintf(stderr, "ERROR: fail to compile compute shader. file %s\n%s\n", filename, message);
            return 2;
        }
        glAttachShader(prog, sha);
        glDeleteShader(sha);
    }
    glLinkProgram(prog);
    glValidateProgram(prog);
    return 0;
}

int loadShader3x(long *lastModTime, GLuint prog, const char *filename)
{
    struct stat libStat;
    int err = stat(filename, &libStat);
    if (err == 0 && *lastModTime != libStat.st_mtime)
    {
        err = loadShader3(prog, filename);
        if (err != 1)
        {
            printf("INFO: reloading file %s\n", filename);
            *lastModTime = libStat.st_mtime;
            return 1;
        }
    }
    return 0;
}
