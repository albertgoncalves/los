#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define GL_GLEXT_PROTOTYPES

#include <GLFW/glfw3.h>

typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef float    f32;
typedef double   f64;

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
} Geom;

typedef struct {
    Vec2f translate;
    Vec4f color;
} Point;

typedef struct {
    Point points[3];
} Triangle;

typedef struct {
    const char* buffer;
    u32         len;
} String;

typedef struct {
    void* address;
    u32   len;
} MemMap;

typedef struct {
    Mat4 projection;
} Uniforms;

#define OK    0
#define ERROR 1

#define NANOS_PER_SECOND 1000000000

#define ATTRIBUTE(x) __attribute__((x))

#define EXIT()                                                       \
    do {                                                             \
        fprintf(stderr, "%s:%s:%d\n", __FILE__, __func__, __LINE__); \
        _exit(ERROR);                                                \
    } while (FALSE)

#define EXIT_WITH(x)                                                         \
    do {                                                                     \
        fprintf(stderr, "%s:%s:%d `%s`\n", __FILE__, __func__, __LINE__, x); \
        _exit(ERROR);                                                        \
    } while (FALSE)

#define EXIT_IF(condition)         \
    do {                           \
        if (condition) {           \
            EXIT_WITH(#condition); \
        }                          \
    } while (FALSE)

#define EXIT_IF_GL_ERROR()                                 \
    do {                                                   \
        switch (glGetError()) {                            \
        case GL_INVALID_ENUM: {                            \
            EXIT_WITH("GL_INVALID_ENUM");                  \
        }                                                  \
        case GL_INVALID_VALUE: {                           \
            EXIT_WITH("GL_INVALID_VALUE");                 \
        }                                                  \
        case GL_INVALID_OPERATION: {                       \
            EXIT_WITH("GL_INVALID_OPERATION");             \
        }                                                  \
        case GL_INVALID_FRAMEBUFFER_OPERATION: {           \
            EXIT_WITH("GL_INVALID_FRAMEBUFFER_OPERATION"); \
        }                                                  \
        case GL_OUT_OF_MEMORY: {                           \
            EXIT_WITH("GL_OUT_OF_MEMORY");                 \
        }                                                  \
        case GL_NO_ERROR: {                                \
            break;                                         \
        }                                                  \
        default: {                                         \
            EXIT();                                        \
        }                                                  \
        }                                                  \
    } while (FALSE)

#define PI 3.14159274f

#define EPSILON 0.00001f

#if 0
    #define WINDOW_WIDTH    1024
    #define WINDOW_HEIGHT   768
    #define WINDOW_DIAGONAL 1324
#else
    #define WINDOW_WIDTH    1536
    #define WINDOW_HEIGHT   768
    #define WINDOW_DIAGONAL 1718
#endif

#define VIEW_NEAR -1.0f
#define VIEW_FAR  1.0f

#define LINE_WIDTH 1.825f

#define COLOR_BACKGROUND ((Vec4f){0.1f, 0.1f, 0.1f, 1.0f})

#if 0
    #define COLOR_TRIANGLE_0 ((Vec4f){0.3f, 0.25f, 0.375f, 0.1f})
    #define COLOR_TRIANGLE_1 ((Vec4f){1.0f, 0.375f, 0.3f, 0.75f})
    #define COLOR_TRIANGLE_2 ((Vec4f){0.375f, 0.3f, 1.0f, 0.75f})
#else
    #define COLOR_TRIANGLE_0 ((Vec4f){1.0f, 1.0f, 1.0f, 1.0f})
    #define COLOR_TRIANGLE_1 COLOR_TRIANGLE_0
    #define COLOR_TRIANGLE_2 COLOR_TRIANGLE_0
#endif

#define COLOR_QUAD ((Vec4f){0.25f, 0.25f, 0.25f, 1.0f})

#define COLOR_LINE_0 ((Vec4f){0.625f, 0.625f, 0.625f, 0.9f})
#define COLOR_LINE_1 ((Vec4f){0.5f, 0.5f, 0.5f, 0.275f})

#define CAP_BUFFER (1 << 12)

#define CAP_LINES     (1 << 7)
#define CAP_TRIANGLES (1 << 7)
#define CAP_POINTS    (1 << 7)

#define CAP_VAO          3
#define CAP_VBO          3
#define CAP_INSTANCE_VBO 2

#define PATH_GEOM_VERT "src/geom_vert.glsl"
#define PATH_GEOM_FRAG "src/geom_frag.glsl"

#define PATH_TRIANGLE_VERT "src/triangle_vert.glsl"
#define PATH_TRIANGLE_FRAG "src/triangle_frag.glsl"

static char BUFFER[CAP_BUFFER];
static u32  LEN_BUFFER = 0;

#define BIND_BUFFER(object, data, size, target, usage) \
    do {                                               \
        glBindBuffer(target, object);                  \
        glBufferData(target, size, data, usage);       \
        EXIT_IF_GL_ERROR();                            \
    } while (FALSE)

#define SET_VERTEX_ATTRIB(program, label, size, stride, offset)     \
    do {                                                            \
        const u32 index = (u32)glGetAttribLocation(program, label); \
        glEnableVertexAttribArray(index);                           \
        glVertexAttribPointer(index,                                \
                              size,                                 \
                              GL_FLOAT,                             \
                              FALSE,                                \
                              stride,                               \
                              (void*)(offset));                     \
        EXIT_IF_GL_ERROR();                                         \
    } while (FALSE)

#define SET_VERTEX_ATTRIB_DIV(program, label, size, stride, offset) \
    do {                                                            \
        const u32 index = (u32)glGetAttribLocation(program, label); \
        glEnableVertexAttribArray(index);                           \
        glVertexAttribPointer(index,                                \
                              size,                                 \
                              GL_FLOAT,                             \
                              FALSE,                                \
                              stride,                               \
                              (void*)(offset));                     \
        glVertexAttribDivisor(index, 1);                            \
        EXIT_IF_GL_ERROR();                                         \
    } while (FALSE)

static u64 now(void) {
    Time time;
    EXIT_IF(clock_gettime(CLOCK_MONOTONIC, &time));
    return ((u64)time.tv_sec * NANOS_PER_SECOND) + (u64)time.tv_nsec;
}

static MemMap string_open(const char* path) {
    EXIT_IF(!path);
    const i32 file = open(path, O_RDONLY);
    EXIT_IF(file < 0);
    FileStat stat;
    EXIT_IF(fstat(file, &stat) < 0);
    const MemMap map = {
        .address =
            mmap(NULL, (u32)stat.st_size, PROT_READ, MAP_SHARED, file, 0),
        .len = (u32)stat.st_size,
    };
    EXIT_IF(map.address == MAP_FAILED);
    close(file);
    return map;
}

static const char* string_copy(MemMap map) {
    const String string = {
        .buffer = (const char*)map.address,
        .len = map.len,
    };
    EXIT_IF(CAP_BUFFER < (LEN_BUFFER + string.len + 1));
    char* copy = &BUFFER[LEN_BUFFER];
    memcpy(copy, string.buffer, string.len);
    LEN_BUFFER += string.len;
    BUFFER[LEN_BUFFER++] = '\0';
    return copy;
}

static f32 epsilon(f32 x) {
    return x == 0.0f ? EPSILON : x;
}

static f32 polar_degrees(Vec2f point) {
    const f32 degrees =
        (atanf(epsilon(point.y) / epsilon(point.x)) / PI) * 180.0f;
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

static Mat4 orthographic(f32 left,
                         f32 right,
                         f32 bottom,
                         f32 top,
                         f32 near,
                         f32 far) {
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

ATTRIBUTE(noreturn) static void callback_error(i32 code, const char* error) {
    printf("%d: %s\n", code, error);
    EXIT();
}

static void callback_key(GLFWwindow* window, i32 key, i32, i32 action, i32) {
    if (action != GLFW_PRESS) {
        return;
    }
    switch (key) {
    case GLFW_KEY_ESCAPE: {
        glfwSetWindowShouldClose(window, TRUE);
        break;
    }
    }
}

static void compile_shader(const char* path, u32 shader) {
    const MemMap map = string_open(path);
    const char*  source = string_copy(map);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    {
        i32 status = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status) {
            glGetShaderInfoLog(shader,
                               (i32)(CAP_BUFFER - LEN_BUFFER),
                               NULL,
                               &BUFFER[LEN_BUFFER]);
            printf("%s", &BUFFER[LEN_BUFFER]);
            EXIT();
        }
    }
    EXIT_IF(munmap(map.address, map.len));
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
            glGetProgramInfoLog(program,
                                (i32)(CAP_BUFFER - LEN_BUFFER),
                                NULL,
                                &BUFFER[LEN_BUFFER]);
            printf("%s", &BUFFER[LEN_BUFFER]);
            EXIT();
        }
    }
    glDeleteShader(shader_vert);
    glDeleteShader(shader_frag);
    EXIT_IF_GL_ERROR();
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
                      const Mat4*  projection) {
    glUseProgram(program);
    glBindVertexArray(vao);

    BIND_BUFFER(vbo, vertices, size_vertices, GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    SET_VERTEX_ATTRIB(program,
                      "VERT_IN_POSITION",
                      2,
                      sizeof(Vec2f),
                      offsetof(Vec2f, x));
    BIND_BUFFER(instance_vbo,
                geoms,
                size_geoms,
                GL_ARRAY_BUFFER,
                GL_DYNAMIC_DRAW);

    SET_VERTEX_ATTRIB_DIV(program,
                          "VERT_IN_TRANSLATE",
                          2,
                          sizeof(Geom),
                          offsetof(Geom, translate));
    SET_VERTEX_ATTRIB_DIV(program,
                          "VERT_IN_SCALE",
                          2,
                          sizeof(Geom),
                          offsetof(Geom, scale));
    SET_VERTEX_ATTRIB_DIV(program,
                          "VERT_IN_COLOR",
                          4,
                          sizeof(Geom),
                          offsetof(Geom, color));

    glUniformMatrix4fv(glGetUniformLocation(program, "PROJECTION"),
                       1,
                       FALSE,
                       &projection->column_row[0][0]);
    EXIT_IF_GL_ERROR();
}

i32 main(void) {
    EXIT_IF(!glfwInit());
    glfwSetErrorCallback(callback_error);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, FALSE);
    glfwWindowHint(GLFW_SAMPLES, 16);
    GLFWwindow* window =
        glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, __FILE__, NULL, NULL);
    EXIT_IF(!window);

    glfwSetKeyCallback(window, callback_key);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glClearColor(COLOR_BACKGROUND.x,
                 COLOR_BACKGROUND.y,
                 COLOR_BACKGROUND.z,
                 COLOR_BACKGROUND.w);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE);
    EXIT_IF_GL_ERROR();

    const Mat4 projection =
        orthographic(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, VIEW_NEAR, VIEW_FAR);

    u32 vao[CAP_VAO];
    glGenVertexArrays(CAP_VAO, &vao[0]);

    u32 vbo[CAP_VBO];
    glGenBuffers(CAP_VBO, &vbo[0]);

    u32 instance_vbo[CAP_INSTANCE_VBO];
    glGenBuffers(CAP_INSTANCE_VBO, &instance_vbo[0]);

    const Vec2f vertices_line[] = {{0.0f, 0.0f}, {1.0f, 1.0f}};

    Geom lines[CAP_LINES] = {
        {{0}, {WINDOW_WIDTH, 0.0f}, COLOR_LINE_0},
        {{WINDOW_WIDTH, 0.0f}, {0.0f, WINDOW_HEIGHT}, COLOR_LINE_0},
        {{WINDOW_WIDTH, WINDOW_HEIGHT}, {-WINDOW_WIDTH, 0.0f}, COLOR_LINE_0},
        {{0.0f, WINDOW_HEIGHT}, {0.0f, -WINDOW_HEIGHT}, COLOR_LINE_0},
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
              &projection);
    glLineWidth(LINE_WIDTH);
    glEnable(GL_LINE_SMOOTH);

    const Vec2f vertices_quad[] = {
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {0.0f, 0.0f},
    };

    Geom quads[] = {
        {{400.0f, 400.0f}, {25.0f, 100.0f}, COLOR_QUAD},
        {{600.0f, 250.0f}, {10.0f, 150.0f}, COLOR_QUAD},
        {{850.0f, 400.0f}, {5.0f, 300.0f}, COLOR_QUAD},
        {{850.0f, 300.0f}, {100.0f, 5.0f}, COLOR_QUAD},
        {{1200.0f, 150.0f}, {5.0f, 50.0f}, COLOR_QUAD},
        {{1150.0f, 225.0f}, {25.0f, 25.0f}, COLOR_QUAD},
        {{1175.0f, 650.0f}, {5.0f, 75.0f}, COLOR_QUAD},
    };
#define LEN_QUADS (sizeof(quads) / sizeof(quads[0]))

    const u32 program_quad = compile_program(PATH_GEOM_VERT, PATH_GEOM_FRAG);
    init_geom(program_quad,
              vao[1],
              vbo[1],
              instance_vbo[1],
              vertices_quad,
              sizeof(vertices_quad),
              quads,
              sizeof(quads),
              &projection);

    Triangle triangles[CAP_TRIANGLES];

    const u32 program_triangles =
        compile_program(PATH_TRIANGLE_VERT, PATH_TRIANGLE_FRAG);
    glUseProgram(program_triangles);
    glBindVertexArray(vao[2]);

    BIND_BUFFER(vbo[2],
                triangles,
                sizeof(triangles),
                GL_ARRAY_BUFFER,
                GL_DYNAMIC_DRAW);

    SET_VERTEX_ATTRIB(program_triangles,
                      "VERT_IN_POSITION",
                      2,
                      sizeof(Point),
                      offsetof(Point, translate));
    SET_VERTEX_ATTRIB(program_triangles,
                      "VERT_IN_COLOR",
                      4,
                      sizeof(Point),
                      offsetof(Point, color));

    glUniformMatrix4fv(glGetUniformLocation(program_triangles, "PROJECTION"),
                       1,
                       FALSE,
                       &projection.column_row[0][0]);
    EXIT_IF_GL_ERROR();

    Vec2f position = {WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f};
    Vec2f speed = {0};

    u64 time[2] = {now()};
    u64 frames = 0;

    printf("\n\n");
    while (!glfwWindowShouldClose(window)) {
        {
            time[1] = now();
            const u64 elapsed = time[1] - time[0];
            if (NANOS_PER_SECOND <= elapsed) {
                const f64 nanoseconds_per_frame =
                    ((f64)elapsed) / ((f64)frames);
                printf("\033[2A"
                       "%9.0f ns/f\n"
                       "%9lu frames\n",
                       nanoseconds_per_frame,
                       frames);
                time[0] = time[1];
                frames = 0;
            }
        }

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
        move = normalize(move);

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
        const Vec2f look_to = (Vec2f){
            .x = (f32)cursor.x,
            .y = (f32)cursor.y,
        };

        const Vec2f look_from = extend(position, look_to, 25.0f);

#define FOV (PI / 4.0f)
        Vec2f target[2] = {
            extend(look_from,
                   turn(look_from, look_to, -(FOV / 2.0f)),
                   WINDOW_DIAGONAL),
            extend(look_from,
                   turn(look_from, look_to, FOV / 2.0f),
                   WINDOW_DIAGONAL),
        };
#undef FOV

        u32 len_lines = 6;
        EXIT_IF(CAP_LINES < len_lines);
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

#define LINE_BETWEEN(x0, y0)                                            \
    do {                                                                \
        f32 degrees =                                                   \
            polar_degrees((Vec2f){(x0)-look_from.x, (y0)-look_from.y}); \
        Bool inside = FALSE;                                            \
        if ((fov[0] <= degrees) && (degrees <= fov[1])) {               \
            inside |= TRUE;                                             \
        }                                                               \
        degrees -= 360.0f;                                              \
        if ((fov[0] <= degrees) && (degrees <= fov[1])) {               \
            inside |= TRUE;                                             \
        }                                                               \
        if (!inside) {                                                  \
            break;                                                      \
        }                                                               \
        EXIT_IF(CAP_LINES <= len_lines);                                \
        lines[len_lines++] = (Geom){                                    \
            {x0, y0},                                                   \
            {look_from.x - (x0), look_from.y - (y0)},                   \
            COLOR_LINE_1,                                               \
        };                                                              \
    } while (FALSE)

        for (u32 i = 0; i < 4; ++i) {
            LINE_BETWEEN(lines[i].translate.x, lines[i].translate.y);
        }
        for (u32 i = 0; i < LEN_QUADS; ++i) {
            const f32 w = quads[i].scale.x;
            const f32 h = quads[i].scale.y;

            LINE_BETWEEN(quads[i].translate.x, quads[i].translate.y);
            LINE_BETWEEN(quads[i].translate.x + w, quads[i].translate.y);
            LINE_BETWEEN(quads[i].translate.x + w, quads[i].translate.y + h);
            LINE_BETWEEN(quads[i].translate.x, quads[i].translate.y + h);
        }

        Vec2f points[CAP_POINTS];

        u32 len_points = 0;
        for (u32 i = 4; i < len_lines; ++i) {
            EXIT_IF(CAP_POINTS <= (len_points + 2));
            points[len_points++] = lines[i].translate;
            points[len_points++] =
                extend(look_from,
                       turn(look_from, lines[i].translate, -EPSILON),
                       WINDOW_DIAGONAL);
            points[len_points++] =
                extend(look_from,
                       turn(look_from, lines[i].translate, EPSILON),
                       WINDOW_DIAGONAL);
        }

        for (u32 i = 0; i < len_points; ++i) {
            Vec2f a[2] = {look_from, points[i]};
            for (u32 j = 0; j < LEN_QUADS; ++j) {
                const f32 w = quads[j].scale.x;
                const f32 h = quads[j].scale.y;

                Vec2f b[2] = {
                    quads[j].translate,
                    {quads[j].translate.x + w, quads[j].translate.y},
                };
                intersect(a, b, &points[i]);

                a[1] = points[i];
                b[0] = (Vec2f){
                    quads[j].translate.x + w,
                    quads[j].translate.y + h,
                };
                intersect(a, b, &points[i]);

                a[1] = points[i];
                b[1] = (Vec2f){
                    quads[j].translate.x,
                    quads[j].translate.y + h,
                };
                intersect(a, b, &points[i]);

                a[1] = points[i];
                b[0] = quads[j].translate;
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
                    polar_degrees((Vec2f){points[j].x - look_from.x,
                                          points[j].y - look_from.y}) -
                    polar_degrees((Vec2f){points[j - 1].x - look_from.x,
                                          points[j - 1].y - look_from.y});
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

        u32 len_triangles = 0;
        for (u32 i = 1; i < len_points; ++i) {
            EXIT_IF(CAP_TRIANGLES <= len_triangles);
            triangles[len_triangles++] = (Triangle){{
                {look_from, COLOR_TRIANGLE_0},
                {points[i - 1], COLOR_TRIANGLE_1},
                {points[i], COLOR_TRIANGLE_2},
            }};
        }
        for (u32 i = 0; i < len_triangles; ++i) {
            for (u32 j = 1; j < 3; ++j) {
                const f32 x = triangles[i].points[j].translate.x -
                              triangles[i].points[0].translate.x;
                const f32 y = triangles[i].points[j].translate.y -
                              triangles[i].points[0].translate.y;
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

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program_quad);
        glBindVertexArray(vao[1]);
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo[1]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quads), &quads[0]);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP,
                              0,
                              sizeof(vertices_quad) / sizeof(vertices_quad[0]),
                              (i32)LEN_QUADS);

        glUseProgram(program_triangles);
        glBindVertexArray(vao[2]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(triangles), &triangles[0]);
        glDrawArrays(GL_TRIANGLES, 0, (i32)(len_triangles * 3));

#if 0
        glUseProgram(program_line);
        glBindVertexArray(vao[0]);
        glBindBuffer(GL_ARRAY_BUFFER, instance_vbo[0]);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lines), &lines[0]);
        glDrawArraysInstanced(GL_LINES,
                              0,
                              sizeof(vertices_line) / sizeof(vertices_line[0]),
                              (i32)len_lines);
#endif

        glfwSwapBuffers(window);

        ++frames;
    }

    glDeleteVertexArrays(CAP_VAO, &vao[0]);
    glDeleteBuffers(CAP_VBO, &vbo[0]);
    glDeleteBuffers(CAP_INSTANCE_VBO, &instance_vbo[0]);

    glDeleteProgram(program_line);
    glDeleteProgram(program_quad);
    glDeleteProgram(program_triangles);

    return OK;
}
