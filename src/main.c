#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define STATIC_ASSERT(condition) _Static_assert(condition, "!(" #condition ")")

#define GL_GLEXT_PROTOTYPES

#include <GLFW/glfw3.h>

typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef float    f32;
typedef double   f64;

STATIC_ASSERT(sizeof(f32) == sizeof(u32));
STATIC_ASSERT(sizeof(f64) == sizeof(u64));
STATIC_ASSERT(sizeof(void*) == sizeof(u64));

typedef struct stat FileStat;

typedef struct timespec Time;

typedef enum {
    FALSE = 0,
    TRUE,
} Bool;

typedef struct {
    f32 x, y;
} Vec2f;

typedef struct {
    f64 x, y;
} Vec2d;

typedef struct {
    f32 x, y, z, w;
} Vec4f;

typedef struct {
    f32 column_row[4][4];
} Mat4;

typedef struct {
    Vec2f translate;
    Vec2f scale;
    Vec4f color;
    f32   rotate_radians;
} Geom;

typedef struct {
    Vec2f translate;
    Vec4f color;
} Point;

typedef struct {
    Point points[3];
} Triangle;

typedef struct {
    Vec2f points[4];
} Quad;

#define NANOS_PER_SECOND 1000000000

#define PI  ((f32)M_PI)
#define TAU (PI * 2.0f)

#define EPSILON 0.00001f

#if 0
    #define WINDOW_WIDTH    2500
    #define WINDOW_HEIGHT   1150
    #define WINDOW_DIAGONAL 2752
#else
    #define WINDOW_WIDTH    1536
    #define WINDOW_HEIGHT   768
    #define WINDOW_DIAGONAL 1718
#endif

#define MULTISAMPLES_WINDOW  16
#define MULTISAMPLES_TEXTURE 16
#define MULTISAMPLES_SHADOW  16

#define VIEW_NEAR -1.0f
#define VIEW_FAR  1.0f

#if 1
    #define VIEW_TRANSLATE      ((Vec2f){WINDOW_WIDTH, WINDOW_HEIGHT})
    #define VIEW_ROTATE_RADIANS ((180.0f * PI) / 180.0f)
#else
    #define VIEW_TRANSLATE      ((Vec2f){0})
    #define VIEW_ROTATE_RADIANS 0
#endif

#define LINE_WIDTH 1.825f

#define COLOR_BACKGROUND ((Vec4f){0})

#if 0
    #define COLOR_TRIANGLE_0 ((Vec4f){0.3f, 0.25f, 0.375f, 0.1f})
    #define COLOR_TRIANGLE_1 ((Vec4f){1.0f, 0.375f, 0.3f, 0.75f})
    #define COLOR_TRIANGLE_2 ((Vec4f){0.375f, 0.3f, 1.0f, 0.75f})
#else
    #define COLOR_TRIANGLE_0 ((Vec4f){1.0f, 1.0f, 1.0f, 1.0f})
    #define COLOR_TRIANGLE_1 COLOR_TRIANGLE_0
    #define COLOR_TRIANGLE_2 COLOR_TRIANGLE_0
#endif

#define COLOR_OBJECT ((Vec4f){0.25f, 0.25f, 0.25f, 1.0f})
#define COLOR_WORLD  ((Vec4f){0.0f, 1.0f, 1.0f, 1.0f})

#define COLOR_LINE_0 ((Vec4f){0.625f, 0.625f, 0.625f, 0.9f})
#define COLOR_LINE_1 ((Vec4f){0.5f, 0.5f, 0.5f, 0.275f})

#define CAP_LINES     (1 << 6)
#define CAP_QUADS     (1 << 4)
#define CAP_TRIANGLES (1 << 7)
#define CAP_POINTS    (1 << 7)

#define CAP_VAO          4
#define CAP_VBO          4
#define CAP_INSTANCE_VBO 2
#define CAP_FBO          2
#define CAP_TEXTURES     2

#define PATH_GEOM_VERT "src/geom_vert.glsl"
#define PATH_GEOM_FRAG "src/geom_frag.glsl"

#define PATH_TRIANGLE_VERT "src/triangle_vert.glsl"
#define PATH_TRIANGLE_FRAG "src/triangle_frag.glsl"

#define PATH_SHADOW_VERT "src/shadow_vert.glsl"
#define PATH_SHADOW_FRAG "src/shadow_frag.glsl"

#define BIND_BUFFER(object, data, size, target, usage) \
    do {                                               \
        glBindBuffer(target, object);                  \
        glBufferData(target, size, data, usage);       \
    } while (FALSE)

#define SET_VERTEX_ATTRIB(program, label, size, stride, offset)                       \
    do {                                                                              \
        const u32 index = (u32)glGetAttribLocation(program, label);                   \
        glEnableVertexAttribArray(index);                                             \
        glVertexAttribPointer(index, size, GL_FLOAT, FALSE, stride, (void*)(offset)); \
    } while (FALSE)

#define SET_VERTEX_ATTRIB_DIV(program, label, size, stride, offset)                   \
    do {                                                                              \
        const u32 index = (u32)glGetAttribLocation(program, label);                   \
        glEnableVertexAttribArray(index);                                             \
        glVertexAttribPointer(index, size, GL_FLOAT, FALSE, stride, (void*)(offset)); \
        glVertexAttribDivisor(index, 1);                                              \
    } while (FALSE)

static u64 now(void) {
    Time time;
    assert(clock_gettime(CLOCK_MONOTONIC, &time) == 0);
    return ((u64)time.tv_sec * NANOS_PER_SECOND) + (u64)time.tv_nsec;
}

static f32 epsilon(f32 x) {
    return x == 0.0f ? EPSILON : x;
}

static f32 polar_degrees(Vec2f point) {
    const f32 degrees = (atanf(epsilon(point.y) / epsilon(point.x)) / PI) * 180.0f;
    if (point.x < 0.0f) {
        return 180.0f + degrees;
    }
    if (point.y < 0.0f) {
        return 360.0f + degrees;
    }
    return degrees;
}

static Vec2f turn(Vec2f a, Vec2f b, f32 radians) {
    const f32 x = b.x - a.x;
    const f32 y = b.y - a.y;
    const f32 s = sinf(radians);
    const f32 c = cosf(radians);
    return (Vec2f){
        a.x + (x * c) + (y * s),
        a.y + (x * -s) + (y * c),
    };
}

static Quad geom_to_quad(Geom geom) {
    const Vec2f vertices[4] = {
        {0},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
    };

    const f32 w = geom.scale.x / 2.0f;
    const f32 h = geom.scale.y / 2.0f;

    Quad quad = {0};
    for (u32 i = 0; i < 4; ++i) {
        quad.points[i].x = (vertices[i].x * geom.scale.x) - w;
        quad.points[i].y = (vertices[i].y * geom.scale.y) - h;
        quad.points[i] = turn((Vec2f){0}, quad.points[i], geom.rotate_radians);
        quad.points[i].x += w + geom.translate.x;
        quad.points[i].y += h + geom.translate.y;
    }
    return quad;
}

static Vec2f normalize(Vec2f v) {
    const f32 l = epsilon(sqrtf((v.x * v.x) + (v.y * v.y)));
    return (Vec2f){
        .x = v.x / l,
        .y = v.y / l,
    };
}

static Vec2f extend(Vec2f a, Vec2f b, f32 length) {
    const Vec2f c = normalize((Vec2f){
        b.x - a.x,
        b.y - a.y,
    });
    return (Vec2f){
        a.x + (c.x * length),
        a.y + (c.y * length),
    };
}

static Mat4 translate_rotate(Vec2f translate, f32 rotate_radians) {
    const f32 s = sinf(rotate_radians);
    const f32 c = cosf(rotate_radians);
    return (Mat4){
        .column_row[0][0] = c,
        .column_row[0][1] = s,
        .column_row[1][0] = -s,
        .column_row[1][1] = c,
        .column_row[2][2] = 1.0f,
        .column_row[3][0] = translate.x,
        .column_row[3][1] = translate.y,
        .column_row[3][3] = 1.0f,
    };
}

static Mat4 orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    return (Mat4){
        .column_row[0][0] = 2.0f / (right - left),
        .column_row[1][1] = 2.0f / (top - bottom),
        .column_row[2][2] = 2.0f / (near - far),
        .column_row[3][3] = 1.0f,
        .column_row[3][0] = (left + right) / (left - right),
        .column_row[3][1] = (bottom + top) / (bottom - top),
        .column_row[3][2] = (near + far) / (near - far),
    };
}

static void intersect(const Vec2f a[2], const Vec2f b[2], Vec2f* point) {
    const f32 x0 = a[0].x - a[1].x;
    const f32 y0 = a[0].y - a[1].y;

    const f32 x1 = a[0].x - b[0].x;
    const f32 y1 = a[0].y - b[0].y;

    const f32 x2 = b[0].x - b[1].x;
    const f32 y2 = b[0].y - b[1].y;

    const f32 denominator = (x0 * y2) - (y0 * x2);
    if (denominator == 0.0f) {
        return;
    }
    const f32 t = ((x1 * y2) - (y1 * x2)) / denominator;
    const f32 u = -((x0 * y1) - (y0 * x1)) / denominator;
    if ((t < 0.0f) || (1.0f < t) || (u < 0.0f) || (1.0f < u)) {
        return;
    }
    point->x = a[0].x + (t * (a[1].x - a[0].x));
    point->y = a[0].y + (t * (a[1].y - a[0].y));
}

__attribute__((noreturn)) static void callback_glfw_error(i32 code, const char* error) {
    fflush(stdout);
    fflush(stderr);
    fprintf(stderr, "%d", code);
    if (error) {
        fprintf(stderr, ": %s", error);
    }
    fputc('\n', stderr);
    assert(0);
}

static void callback_glfw_key(GLFWwindow* window, i32 key, i32, i32 action, i32) {
    if (action != GLFW_PRESS) {
        return;
    }
    switch (key) {
    case GLFW_KEY_ESCAPE: {
        glfwSetWindowShouldClose(window, TRUE);
        break;
    }
    default: {
    }
    }
}

__attribute__((noreturn)) static void callback_gl_debug(u32,
                                                        u32,
                                                        u32,
                                                        u32,
                                                        i32         length,
                                                        const char* message,
                                                        const void*) {
    fflush(stdout);
    fflush(stderr);
    if (0 < length) {
        fprintf(stderr, "%.*s", length, message);
    } else {
        fprintf(stderr, "%s", message);
    }
    assert(0);
}

static void compile_shader(const char* path, u32 shader) {
    assert(path);
    const i32 file = open(path, O_RDONLY);
    assert(0 <= file);

    FileStat stat;
    assert(0 <= fstat(file, &stat));
    const u32 len = (u32)stat.st_size;

    void* address = mmap(NULL, len, PROT_READ, MAP_SHARED, file, 0);
    close(file);
    assert(address != MAP_FAILED);

    {
#define CAP_BUFFER (1 << 10)
        assert(len <= CAP_BUFFER);
        char buffer[CAP_BUFFER];
        memcpy(buffer, address, len);

        const char* buffers[1] = {buffer};
        const i32   lens[1] = {(i32)len};
        glShaderSource(shader, 1, buffers, lens);
#undef CAP_BUFFER
    }

    glCompileShader(shader);
    {
        i32 status = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status) {
#define CAP_BUFFER (1 << 8)
            char buffer[CAP_BUFFER];
            glGetShaderInfoLog(shader, CAP_BUFFER, NULL, &buffer[0]);
            printf("%s", &buffer[0]);
            assert(0);
#undef CAP_BUFFER
        }
    }
    assert(munmap(address, len) == 0);
}

static u32 compile_program(const char* source_vert, const char* source_frag) {
    const u32 program = glCreateProgram();
    const u32 shader_vert = glCreateShader(GL_VERTEX_SHADER);
    const u32 shader_frag = glCreateShader(GL_FRAGMENT_SHADER);
    compile_shader(source_vert, shader_vert);
    compile_shader(source_frag, shader_frag);
    glAttachShader(program, shader_vert);
    glAttachShader(program, shader_frag);
    glLinkProgram(program);
    {
        i32 status = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (!status) {
#define CAP_BUFFER (1 << 8)
            char buffer[CAP_BUFFER];
            glGetProgramInfoLog(program, CAP_BUFFER, NULL, &buffer[0]);
            printf("%s", &buffer[0]);
            assert(0);
#undef CAP_BUFFER
        }
    }
    glDeleteShader(shader_vert);
    glDeleteShader(shader_frag);
    return program;
}

static void init_geom(u32          program,
                      u32          vao,
                      u32          vbo,
                      u32          instance_vbo,
                      const Vec2f* vertices,
                      u32          size_vertices,
                      const Geom*  geoms,
                      u32          size_geoms,
                      const Mat4*  projection,
                      const Mat4*  view) {
    glUseProgram(program);
    glBindVertexArray(vao);

    BIND_BUFFER(vbo, vertices, size_vertices, GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    SET_VERTEX_ATTRIB(program, "VERT_IN_POSITION", 2, sizeof(Vec2f), offsetof(Vec2f, x));
    BIND_BUFFER(instance_vbo, geoms, size_geoms, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

    SET_VERTEX_ATTRIB_DIV(program, "VERT_IN_TRANSLATE", 2, sizeof(Geom), offsetof(Geom, translate));
    SET_VERTEX_ATTRIB_DIV(program, "VERT_IN_SCALE", 2, sizeof(Geom), offsetof(Geom, scale));
    SET_VERTEX_ATTRIB_DIV(program, "VERT_IN_COLOR", 4, sizeof(Geom), offsetof(Geom, color));
    SET_VERTEX_ATTRIB_DIV(program,
                          "VERT_IN_ROTATE_RADIANS",
                          1,
                          sizeof(Geom),
                          offsetof(Geom, rotate_radians));

    glUniformMatrix4fv(glGetUniformLocation(program, "PROJECTION"),
                       1,
                       FALSE,
                       &projection->column_row[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program, "VIEW"), 1, FALSE, &view->column_row[0][0]);
}

i32 main(void) {
    glfwSetErrorCallback(callback_glfw_error);

    assert(glfwInit());

    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, FALSE);
    glfwWindowHint(GLFW_SAMPLES, MULTISAMPLES_WINDOW);
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, __FILE__, NULL, NULL);
    assert(window);

    glfwSetKeyCallback(window, callback_glfw_key);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(callback_gl_debug, NULL);

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glClearColor(COLOR_BACKGROUND.x, COLOR_BACKGROUND.y, COLOR_BACKGROUND.z, COLOR_BACKGROUND.w);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);

    const Mat4 projection = orthographic(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, VIEW_NEAR, VIEW_FAR);
    const Mat4 view = translate_rotate(VIEW_TRANSLATE, VIEW_ROTATE_RADIANS);

    u32 vao[CAP_VAO];
    glGenVertexArrays(CAP_VAO, &vao[0]);

    u32 vbo[CAP_VBO];
    glGenBuffers(CAP_VBO, &vbo[0]);

    u32 instance_vbo[CAP_INSTANCE_VBO];
    glGenBuffers(CAP_INSTANCE_VBO, &instance_vbo[0]);

    const Vec2f vertices_line[] = {{0.0f, 0.0f}, {1.0f, 1.0f}};

    Geom lines[CAP_LINES] = {
        {{0}, {WINDOW_WIDTH, 0.0f}, COLOR_LINE_0, 0.0f},
        {{WINDOW_WIDTH, 0.0f}, {0.0f, WINDOW_HEIGHT}, COLOR_LINE_0, 0.0f},
        {{WINDOW_WIDTH, WINDOW_HEIGHT}, {-WINDOW_WIDTH, 0.0f}, COLOR_LINE_0, 0.0f},
        {{0.0f, WINDOW_HEIGHT}, {0.0f, -WINDOW_HEIGHT}, COLOR_LINE_0, 0.0f},
    };

    const u32 program_line = compile_program(PATH_GEOM_VERT, PATH_GEOM_FRAG);
    init_geom(program_line,
              vao[0],
              vbo[0],
              instance_vbo[0],
              vertices_line,
              sizeof(vertices_line),
              lines,
              sizeof(lines),
              &projection,
              &view);
    glLineWidth(LINE_WIDTH);
    glEnable(GL_LINE_SMOOTH);

    const Vec2f vertices_quad[4] = {
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f},
    };

    Geom quads[CAP_QUADS] = {
        {{0}, {WINDOW_WIDTH, WINDOW_HEIGHT}, COLOR_WORLD, 0.0f},
        {{400.0f, 400.0f}, {25.0f, 100.0f}, COLOR_OBJECT, 0.0f},
        {{600.0f, 250.0f}, {10.0f, 150.0f}, COLOR_OBJECT, 0.0f},
        {{850.0f, 400.0f}, {5.0f, 300.0f}, COLOR_OBJECT, 0.0f},
        {{850.0f, 300.0f}, {100.0f, 5.0f}, COLOR_OBJECT, 0.0f},
        {{1200.0f, 150.0f}, {5.0f, 50.0f}, COLOR_OBJECT, 0.0f},
        {{1150.0f, 225.0f}, {25.0f, 25.0f}, COLOR_OBJECT, 0.0f},
        {{1175.0f, 650.0f}, {5.0f, 75.0f}, COLOR_OBJECT, 0.0f},
    };

    const u32 program_quad = compile_program(PATH_GEOM_VERT, PATH_GEOM_FRAG);
    init_geom(program_quad,
              vao[1],
              vbo[1],
              instance_vbo[1],
              vertices_quad,
              sizeof(vertices_quad),
              quads,
              sizeof(quads),
              &projection,
              &view);

    Triangle triangles[CAP_TRIANGLES];

    const u32 program_triangles = compile_program(PATH_TRIANGLE_VERT, PATH_TRIANGLE_FRAG);
    glUseProgram(program_triangles);
    glBindVertexArray(vao[2]);

    BIND_BUFFER(vbo[2], triangles, sizeof(triangles), GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);

    SET_VERTEX_ATTRIB(program_triangles,
                      "VERT_IN_POSITION",
                      2,
                      sizeof(Point),
                      offsetof(Point, translate));
    SET_VERTEX_ATTRIB(program_triangles, "VERT_IN_COLOR", 4, sizeof(Point), offsetof(Point, color));

    glUniformMatrix4fv(glGetUniformLocation(program_triangles, "PROJECTION"),
                       1,
                       FALSE,
                       &projection.column_row[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program_triangles, "VIEW"),
                       1,
                       FALSE,
                       &view.column_row[0][0]);

    const Vec2f shadow[] = {
        {WINDOW_WIDTH, WINDOW_HEIGHT},
        {WINDOW_WIDTH, 0.0f},
        {0.0f, WINDOW_HEIGHT},
        {0.0f, 0.0f},
    };
#define LEN_SHADOWS (sizeof(shadow) / sizeof(shadow[0]))

    const u32 program_shadow = compile_program(PATH_SHADOW_VERT, PATH_SHADOW_FRAG);
    glUseProgram(program_shadow);
    glBindVertexArray(vao[3]);
    BIND_BUFFER(vbo[3], shadow, sizeof(shadow), GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW);
    SET_VERTEX_ATTRIB(program_shadow, "VERT_IN_POSITION", 2, sizeof(Vec2f), offsetof(Vec2f, x));
    glUniformMatrix4fv(glGetUniformLocation(program_shadow, "PROJECTION"),
                       1,
                       FALSE,
                       &projection.column_row[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(program_shadow, "VIEW"),
                       1,
                       FALSE,
                       &view.column_row[0][0]);

    u32 textures[CAP_TEXTURES];
    glGenTextures(CAP_TEXTURES, &textures[0]);
    for (u32 i = 0; i < CAP_TEXTURES; ++i) {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textures[i]);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                                MULTISAMPLES_TEXTURE,
                                GL_RGBA8,
                                WINDOW_WIDTH,
                                WINDOW_HEIGHT,
                                FALSE);
    }

    u32 fbo[CAP_FBO];
    glGenFramebuffers(CAP_FBO, &fbo[0]);
    for (u32 i = 0; i < CAP_FBO; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER,
                               GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D_MULTISAMPLE,
                               textures[i],
                               0);
    }

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textures[0]);
    glUniform1i(glGetUniformLocation(program_shadow, "TEXTURE"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, textures[1]);
    glUniform1i(glGetUniformLocation(program_shadow, "MASK"), 1);

    const i32 uniform_blend = glGetUniformLocation(program_shadow, "BLEND");
    glUniform1i(glGetUniformLocation(program_shadow, "MULTISAMPLES_SHADOW"), MULTISAMPLES_SHADOW);

    Vec2f position = {WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f};
    Vec2f speed = {0};

    u64 prev = now();
    u64 elapsed = 0;
    u64 frames = 0;

    u32 len_lines = 0;
    u32 len_quads = 0;
    u32 len_points = 0;
    u32 len_triangles = 0;

    printf("\n\n\n\n\n\n");
    while (!glfwWindowShouldClose(window)) {
        {
            const u64 next = now();
            elapsed += next - prev;
            prev = next;
            if (NANOS_PER_SECOND <= elapsed) {
                const f64 nanoseconds_per_frame = ((f64)elapsed) / ((f64)frames);
                printf("\033[6A"
                       "%9.0f ns/f\n"
                       "%9lu frames\n"
                       "%9u len_lines\n"
                       "%9u len_quads\n"
                       "%9u len_points\n"
                       "%9u len_triangles\n",
                       nanoseconds_per_frame,
                       frames,
                       len_lines,
                       len_quads,
                       len_points,
                       len_triangles);
                elapsed = 0;
                frames = 0;
            }
        }

        ++frames;

        glfwPollEvents();

        Vec2f move = {0};
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            move.y -= 1.0f;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            move.y += 1.0f;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            move.x -= 1.0f;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            move.x += 1.0f;
        }
        move = normalize(turn((Vec2f){0}, move, VIEW_ROTATE_RADIANS));

#define RUN 3.75f
        speed.x += move.x * RUN;
        speed.y += move.y * RUN;
#undef RUN
#define FRICTION 0.7875f
        speed.x *= FRICTION;
        speed.y *= FRICTION;
#undef FRICTION
        position.x += speed.x;
        position.y += speed.y;

        Vec2d cursor;
        glfwGetCursorPos(window, &cursor.x, &cursor.y);

        Vec2f look_to = (Vec2f){(f32)cursor.x, (f32)cursor.y};
        look_to.x -= VIEW_TRANSLATE.x;
        look_to.y -= VIEW_TRANSLATE.y;
        look_to = turn((Vec2f){0}, look_to, VIEW_ROTATE_RADIANS);

#define LOOK_FROM_OFFSET 15.0f
        const Vec2f look_from = extend(position, look_to, LOOK_FROM_OFFSET);
#undef LOOK_FROM_OFFSET

        len_quads = 8;
        for (u32 i = 1; i < len_quads; ++i) {
            quads[i].rotate_radians += 0.001f;
            if (TAU <= quads[i].rotate_radians) {
                quads[i].rotate_radians -= TAU;
            }
        }
        {
#define PLAYER_WIDTH  24.0f
#define PLAYER_HEIGHT 16.0f
            assert(len_quads < CAP_QUADS);

            quads[len_quads++] = (Geom){
                {
                    position.x - (PLAYER_WIDTH / 2.0f),
                    position.y - (PLAYER_HEIGHT / 2.0f),
                },
                {PLAYER_WIDTH, PLAYER_HEIGHT},
                COLOR_OBJECT,
                (VIEW_ROTATE_RADIANS -
                 ((polar_degrees((Vec2f){look_to.x - look_from.x, look_to.y - look_from.y}) * PI) /
                  180.0f)) +
                    (PI / 2.0f),
            };
#undef PLAYER_WIDTH
#undef PLAYER_HEIGHT
        }

        Quad rotated_quads[CAP_QUADS];
        for (u32 i = 0; i < len_quads; ++i) {
            rotated_quads[i] = geom_to_quad(quads[i]);
        }

        f32 blend = look_from.x / WINDOW_WIDTH;
        if (blend < 0.0f) {
            blend = 0.0f;
        }
        if (1.0f < blend) {
            blend = 1.0f;
        }

#define FOV_RADIANS ((70.0f * PI) / 180.0f)
        Vec2f target[2] = {
            extend(look_from, turn(look_from, look_to, -(FOV_RADIANS / 2.0f)), WINDOW_DIAGONAL),
            extend(look_from, turn(look_from, look_to, FOV_RADIANS / 2.0f), WINDOW_DIAGONAL),
        };
#undef FOV_RADIANS

        len_lines = 6;
        assert(len_lines <= CAP_LINES);
        lines[4].translate = target[0];
        lines[5].translate = target[1];
        for (u32 i = 4; i < 6; ++i) {
            lines[i].scale.x = look_from.x - lines[i].translate.x;
            lines[i].scale.y = look_from.y - lines[i].translate.y;
        }

        f32 fov[2] = {
            polar_degrees((Vec2f){
                target[0].x - look_from.x,
                target[0].y - look_from.y,
            }),
            polar_degrees((Vec2f){
                target[1].x - look_from.x,
                target[1].y - look_from.y,
            }),
        };
        {
            f32 angle = fov[1] - fov[0];
            if (180.0f < angle) {
                angle -= 360.0f;
            }
            if (angle < -180.0f) {
                angle += 360.0f;
            }
            if (angle < 0.0f) {
                const f32 degrees = fov[1];
                fov[1] = fov[0];
                fov[0] = degrees;
            }
            if (fov[1] < fov[0]) {
                fov[0] -= 360.0f;
            }
        }

#define LINE_BETWEEN(point)                                                                      \
    do {                                                                                         \
        f32  degrees = polar_degrees((Vec2f){(point).x - look_from.x, (point).y - look_from.y}); \
        Bool inside = FALSE;                                                                     \
        if ((fov[0] <= degrees) && (degrees <= fov[1])) {                                        \
            inside |= TRUE;                                                                      \
        }                                                                                        \
        degrees -= 360.0f;                                                                       \
        if ((fov[0] <= degrees) && (degrees <= fov[1])) {                                        \
            inside |= TRUE;                                                                      \
        }                                                                                        \
        if (!inside) {                                                                           \
            break;                                                                               \
        }                                                                                        \
        assert(len_lines < CAP_LINES);                                                           \
        lines[len_lines++] = (Geom){                                                             \
            (point),                                                                             \
            {look_from.x - ((point).x), look_from.y - (point).y},                                \
            COLOR_LINE_1,                                                                        \
            0.0f,                                                                                \
        };                                                                                       \
    } while (FALSE)

        for (u32 i = 0; i < 4; ++i) {
            LINE_BETWEEN(lines[i].translate);
        }
        for (u32 i = 1; i < len_quads; ++i) {
            for (u32 j = 0; j < 4; ++j) {
                LINE_BETWEEN(rotated_quads[i].points[j]);
            }
        }
#undef LINE_BETWEEN

        Vec2f points[CAP_POINTS];

        len_points = 0;
        for (u32 i = 4; i < len_lines; ++i) {
            assert((len_points + 2) < CAP_POINTS);
            points[len_points++] = lines[i].translate;
            points[len_points++] =
                extend(look_from, turn(look_from, lines[i].translate, -EPSILON), WINDOW_DIAGONAL);
            points[len_points++] =
                extend(look_from, turn(look_from, lines[i].translate, EPSILON), WINDOW_DIAGONAL);
        }

        for (u32 i = 0; i < len_points; ++i) {
            Vec2f a[2] = {look_from, points[i]};
            for (u32 j = 1; j < len_quads; ++j) {
                Vec2f b[2] = {
                    rotated_quads[j].points[0],
                    rotated_quads[j].points[1],
                };
                intersect(a, b, &points[i]);

                a[1] = points[i];
                b[0] = rotated_quads[j].points[2];
                intersect(a, b, &points[i]);

                a[1] = points[i];
                b[1] = rotated_quads[j].points[3];
                intersect(a, b, &points[i]);

                a[1] = points[i];
                b[0] = rotated_quads[j].points[0];
                intersect(a, b, &points[i]);
            }
            for (u32 j = 0; j < 4; ++j) {
                a[1] = points[i];
                const Vec2f b[2] = {
                    lines[j].translate,
                    {
                        lines[j].translate.x + lines[j].scale.x,
                        lines[j].translate.y + lines[j].scale.y,
                    },
                };
                intersect(a, b, &points[i]);
            }
        }

        for (u32 i = 1; i < len_points; ++i) {
            for (u32 j = i; 0 < j; --j) {
                f32 angle =
                    polar_degrees((Vec2f){points[j].x - look_from.x, points[j].y - look_from.y}) -
                    polar_degrees(
                        (Vec2f){points[j - 1].x - look_from.x, points[j - 1].y - look_from.y});
                if (angle < -180.0f) {
                    angle += 360.0f;
                }
                if (180.0f < angle) {
                    angle -= 360.0f;
                }

                if (angle < 0.0f) {
                    break;
                }
                const Vec2f point = points[j - 1];
                points[j - 1] = points[j];
                points[j] = point;
            }
        }

        len_triangles = 0;
        for (u32 i = 1; i < len_points; ++i) {
            assert(len_triangles < CAP_TRIANGLES);
            triangles[len_triangles++] = (Triangle){{
                {look_from, COLOR_TRIANGLE_0},
                {points[i - 1], COLOR_TRIANGLE_1},
                {points[i], COLOR_TRIANGLE_2},
            }};
        }
        for (u32 i = 0; i < len_triangles; ++i) {
            for (u32 j = 1; j < 3; ++j) {
                const f32 x =
                    triangles[i].points[j].translate.x - triangles[i].points[0].translate.x;
                const f32 y =
                    triangles[i].points[j].translate.y - triangles[i].points[0].translate.y;
                f32 t = sqrtf((x * x) + (y * y)) / WINDOW_DIAGONAL;
                if (1.0f < t) {
                    t = 1.0f;
                } else if (t < 0.0f) {
                    t = 0.0f;
                }
                const f32 alpha = 1.0f + -t;
                triangles[i].points[j].color.w = alpha;
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program_quad);
        glBindVertexArray(vao[1]);
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quads[0]) * len_quads, &quads[0]);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, (i32)len_quads);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program_triangles);
        glBindVertexArray(vao[2]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(triangles), &triangles[0]);
        glDrawArrays(GL_TRIANGLES, 0, (i32)(len_triangles * 3));

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClear(GL_COLOR_BUFFER_BIT);

#if 0
        glUseProgram(program_line);
        glBindVertexArray(vao[0]);
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lines), &lines[0]);
        glDrawArraysInstanced(GL_LINES, 0, 2, (i32)len_lines);
#endif

        glUseProgram(program_shadow);
        glBindVertexArray(vao[3]);
        glUniform1f(uniform_blend, blend);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, LEN_SHADOWS);
#undef LEN_SHADOWS

        glfwSwapBuffers(window);
    }

    glDeleteTextures(CAP_TEXTURES, &textures[0]);
    glDeleteFramebuffers(CAP_FBO, &fbo[0]);
    glDeleteBuffers(CAP_INSTANCE_VBO, &instance_vbo[0]);
    glDeleteBuffers(CAP_VBO, &vbo[0]);
    glDeleteVertexArrays(CAP_VAO, &vao[0]);

    glDeleteProgram(program_line);
    glDeleteProgram(program_quad);
    glDeleteProgram(program_triangles);
    glDeleteProgram(program_shadow);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
