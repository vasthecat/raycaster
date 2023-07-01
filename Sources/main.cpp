#include <raylib-ext.hpp>
#include <algorithm>
#include <vector>
#include <iostream>
#include <cmath>

#define RUN_RAYCASTER
#define RAYCAST_TEXTURES

const int screenWidth = 640;
const int screenHeight = 640;

const int board_w = 8;
const int board_h = 8;

const int cell_size = screenWidth / board_w;

int board[board_w][board_h] = {
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 2, 0, 0, 0, 1, 1 },
    { 1, 2, 2, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 1, 0, 0, 0, 0, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
};
Color wall_colors[] = {
    BLANK,
    RED,
    BLUE,
};

struct player_t {
    Vector2 pos;
    float rotation;
    float speed;
};

struct object_t {
    Vector2 pos;
    Image image;
};

bool
check_collision(Vector2 position, float radius)
{
    for (float angle = -PI; angle < PI; angle += PI / 4)
    {
        Vector2 check = position + Vector2Rotate({ radius, 0 }, angle);
        int cell_x = check.x / cell_size;
        int cell_y = check.y / cell_size;
        if (board[cell_x][cell_y] != 0)
            return true;
    }
    return false;
}

void
DrawTopDownView(const player_t &player)
{
    for (int row = 0; row < board_h; ++row) {
        for (int col = 0; col < board_w; ++col) {
            if (board[col][row] != 0) {
                DrawRectangle(col * cell_size, row * cell_size,
                    cell_size, cell_size, BLACK);
            }
            else
            {
                DrawRectangle(col * cell_size, row * cell_size,
                    cell_size, cell_size, WHITE);
            }
        }
    }

    for (int x = cell_size; x < screenWidth; x += cell_size) {
        DrawLine(x, 0, x, screenHeight, GRAY);
    }
    for (int y = cell_size; y < screenHeight; y += cell_size) {
        DrawLine(0, y, screenWidth, y, GRAY);
    }

    DrawCircleV(player.pos, 14, RED);
    DrawLineEx(player.pos, player.pos + Vector2Rotate({ 1,0 }, player.rotation) * 25, 5, BLUE);
}

#ifdef RUN_RAYCASTER
Image images[] = {
    LoadImage("./Assets/textures/TECH_1A.png"), // NULL
    LoadImage("./Assets/textures/TECH_1A16.png"),
    LoadImage("./Assets/textures/SUPPORT_3.png"),
};

struct hit_t {
    Vector2 pos;
    struct { int x, y; } cell_pos;
    bool is_horizontal;
    float angle;
};

inline bool
correct_cell(int x, int y)
{
    return (x >= 0 && x < board_w) && (y >= 0 && y < board_h);
}

inline float
fix_angle(float angle)
{
    while (angle > PI)  angle -= 2 * PI;
    while (angle < -PI) angle += 2 * PI;
    return angle;
}

hit_t
cast_ray(Vector2 pos, float dir)
{
    dir = fix_angle(dir);

    int cell_x = pos.x / cell_size;
    int cell_y = pos.y / cell_size;

    hit_t hit_data_v, hit_data_h;

    // Vertical hit
    for (int k = 0; ; ++k) {
        int shift;
        int k_dir;
        if (dir > -PI / 2 && dir < PI / 2) {
            shift = 1;
            k_dir = 1;
        }
        else {
            shift = 0;
            k_dir = -1;
        }

        float dx = (cell_x + shift + k * k_dir) * cell_size - pos.x;
        float dy = dx * tan(dir);
        Vector2 d = { dx, dy };
        Vector2 hit = d + pos;

        int cell_hit_x = int(hit.x / cell_size) + shift - 1;
        int cell_hit_y = int(hit.y / cell_size);

        hit_data_v.pos = hit;
        hit_data_v.cell_pos = { cell_hit_x, cell_hit_y };
        hit_data_v.is_horizontal = false;
        hit_data_v.angle = dir;

        if (!correct_cell(cell_hit_x, cell_hit_y))
            break;
        if (board[cell_hit_x][cell_hit_y] != 0)
            break;
    }

    // Horizontal hit
    for (int k = 0; ; ++k) {
        int shift;
        int k_dir;
        if (dir > -PI && dir < 0) {
            shift = 0;
            k_dir = -1;
        }
        else {
            shift = 1;
            k_dir = 1;
        }

        float dy = (cell_y + shift + k * k_dir) * cell_size - pos.y;
        float dx = dy / tan(dir);
        Vector2 d = { dx, dy };
        Vector2 hit = d + pos;

        int cell_hit_x = int(hit.x / cell_size);
        int cell_hit_y = int(hit.y / cell_size) + shift - 1;

        hit_data_h.pos = hit;
        hit_data_h.cell_pos = { cell_hit_x, cell_hit_y };
        hit_data_h.is_horizontal = true;
        hit_data_h.angle = dir;

        if (!correct_cell(cell_hit_x, cell_hit_y))
            break;
        if (board[cell_hit_x][cell_hit_y] != 0)
            break;
    }

    if (Vector2Length(hit_data_h.pos - pos) < Vector2Length(hit_data_v.pos - pos)) {
        return hit_data_h;
    }
    else {
        return hit_data_v;
    }
}

struct RaycastConfig
{
    float fov;
    int rays_count;
    float delta_angle;
    float rect_w;
};

Vector2
slerp(Vector2 a, Vector2 b, float t)
{
    float omega = std::acos(Vector2DotProduct(Vector2Normalize(a), Vector2Normalize(b)));
    float k1 = std::sin((1 - t) * omega) / std::sin(omega);
    float k2 = std::sin(t * omega) / std::sin(omega);
    return a * k1 + b * k2;
}

void
DrawRaycastView(const player_t &player, const std::vector<object_t> &objects,
                const RaycastConfig &config)
{
    std::vector<hit_t> hits;
    for (float angle = -config.fov / 2; angle < config.fov / 2; angle += config.delta_angle) {
        hit_t hit = cast_ray(player.pos, player.rotation + angle);
        DrawLineEx(player.pos, hit.pos, 2, BLUE);
        hits.push_back(hit);
    }

    float rect_x = 0;
    for (hit_t& hit : hits)
    {
        Vector2 hit_delta = hit.pos - player.pos;
        float dist = hit_delta.x * cos(player.rotation) +
            hit_delta.y * sin(player.rotation);

        float rect_h = (cell_size * screenHeight) / dist;
        float rect_y = (screenHeight - rect_h) / 2;

        int image_idx = board[hit.cell_pos.x][hit.cell_pos.y];
#ifdef RAYCAST_TEXTURES
        Vector2 pos_in_cell = {
            hit.pos.x - hit.cell_pos.x * cell_size,
            hit.pos.y - hit.cell_pos.y * cell_size,
        };
        Image cell_image = images[image_idx];
        Vector2 column = pos_in_cell / cell_size * cell_image.width;
        int col = column.y;
        if (hit.is_horizontal)
            col = column.x;

        float pix_h = rect_h / cell_image.height;

        for (int i = 0; i < cell_image.height; ++i)
        {
            Color* color_data = (Color*)cell_image.data;
            Color pixel = color_data[i * cell_image.width + col];

            DrawRectangle(
                screenWidth + rect_x, rect_y + pix_h * i,
                config.rect_w + 1, pix_h + 1, pixel
            );
        }
#else
        Color wall_color = wall_colors[image_idx];
        if (hit.is_horizontal)
            wall_color *= 0.8f;

        DrawRectangle(
            screenWidth + rect_x, rect_y, config.rect_w + 1, rect_h + 1, wall_color
        );
#endif

        rect_x += config.rect_w;
    }

    for (auto &object : objects)
    {
        // std::cout << "=== START === " << std::endl;
        Vector2 player_to_object = object.pos - player.pos;
        Vector2 anti_normal = Vector2Normalize(Vector2Rotate(player_to_object, 90 * DEG2RAD));

        Vector2 a = object.pos - anti_normal * cell_size / 2;
        Vector2 b = object.pos + anti_normal * cell_size / 2;
        DrawLineEx(a, b, 5, RED);

        Vector2 dir = Vector2Rotate({ 1, 0 }, player.rotation);

        float start_angle = fix_angle(Vector2Angle({1, 0}, a - player.pos));
        float end_angle = fix_angle(Vector2Angle({1, 0}, b - player.pos));

        const float fov = std::abs(end_angle - start_angle);
        const int rays_count = fov / config.fov * config.rays_count;

        for (int ray_i = 0; ray_i < rays_count; ray_i++)
        {
            Vector2 point = player.pos + slerp(a - player.pos, b - player.pos, ray_i * 1.0f / rays_count);
            hit_t hit = cast_ray(player.pos, player.rotation - Vector2Angle(point - player.pos, dir));

            auto point_delta = point - player.pos;
            float dist = Vector2Length(point_delta);
            if (dist < Vector2Length(hit.pos - player.pos))
            {
                // DrawLineEx(player.pos, point, 2, GREEN);
                float rect_h = (cell_size * screenHeight) / dist;
                float rect_y = (screenHeight - rect_h) / 2;

                float rect_xa = fix_angle(Vector2Angle(Vector2Rotate(dir, -config.fov / 2), a - player.pos)) / config.fov * screenWidth;
                float rect_xb = fix_angle(Vector2Angle(Vector2Rotate(dir, -config.fov / 2), b - player.pos)) / config.fov * screenWidth;
                float rect_w = std::abs(rect_xb - rect_xa) / rays_count;

                rect_x = fix_angle(Vector2Angle(Vector2Rotate(dir, -config.fov / 2), point - player.pos)) / config.fov * screenWidth;
                if (rect_x + rect_w < 0) continue;
                if (rect_x < 0) rect_x = 0;

                float pix_h = rect_h / object.image.height;
                float column = (ray_i * 1.0f / rays_count) * object.image.width;
                int col = (int) column;

                for (int i = 0; i < object.image.height; ++i)
                {
                    Color* color_data = (Color*)object.image.data;
                    Color pixel = color_data[i * object.image.width + col];

                    DrawRectangle(
                        screenWidth + rect_x, rect_y + pix_h * i,
                        rect_w + 1, pix_h + 1, pixel
                    );
                }
            }
            // else
            // {
            //     DrawLineEx(player.pos, hit.pos, 2, MAGENTA);
            // }
        }
        // DrawLineEx(player.pos, a, 5, BLACK);
        // DrawLineEx(player.pos, b, 5, PURPLE);
        // std::cout << "=== END === " << std::endl;
    }
}
#endif

int main()
{
#ifdef RUN_RAYCASTER
    InitWindow(screenWidth * 2, screenHeight, "GDSC: Creative Coding");
#else
    InitWindow(screenWidth, screenHeight, "GDSC: Creative Coding");
#endif
    SetTargetFPS(60);

    player_t player;
    player.pos = { screenWidth / 2., screenHeight / 2. };
    player.speed = 100;
    player.rotation = 0;

    std::vector<object_t> objects;

    object_t barrel;
    barrel.pos = { 3 * cell_size, 5 * cell_size };
    barrel.image = LoadImage("./Assets/textures/barrel.png");
    objects.push_back(barrel);

    object_t barrel2;
    barrel2.pos = { 3 * cell_size, 5 * cell_size };
    barrel2.image = LoadImage("./Assets/textures/barrel.png");
    objects.push_back(barrel2);

#ifdef RUN_RAYCASTER
    RaycastConfig config;
    config.fov = 60 * DEG2RAD;
    config.rays_count = 200;
    config.delta_angle = config.fov / config.rays_count;
    config.rect_w = (screenWidth / config.fov) * config.delta_angle;
#endif

    float t = 0;
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        t += dt;

        objects[1].pos.x = 3 * cell_size + 100 * sin(t);

        Vector2 move = { 0, 0 };
#ifdef RUN_RAYCASTER
        DisableCursor();
        float delta = GetMouseDelta().x;
        player.rotation += delta * dt * 0.1;
        Vector2 dir = Vector2Rotate({ 1, 0 }, player.rotation);
        Vector2 forward = dir * (player.speed * dt);
        Vector2 right = Vector2Rotate(forward, PI / 2);
        if (IsKeyDown(KEY_W))
            move = forward;
        if (IsKeyDown(KEY_S))
            move = -forward;
        if (IsKeyDown(KEY_A))
            move = -right;
        if (IsKeyDown(KEY_D))
            move = right;
#else
        if (IsKeyDown(KEY_W))
            move.y -= player.speed * dt;
        if (IsKeyDown(KEY_S))
            move.y += player.speed * dt;
        if (IsKeyDown(KEY_A))
            move.x -= player.speed * dt;
        if (IsKeyDown(KEY_D))
            move.x += player.speed * dt;

        Vector2 mp = {
            GetMouseX() - player.pos.x,
            GetMouseY() - player.pos.y
        };
        player.rotation = Vector2Angle({ 1, 0 }, mp);
#endif
        player.pos += move;
        if (check_collision(player.pos, 15))
            player.pos -= move;


        BeginDrawing();
        {
            ClearBackground(DARKGRAY);
            DrawRectangle(screenWidth, screenHeight / 2, screenWidth, screenHeight / 2, GRAY);
            DrawTopDownView(player);
#ifdef RUN_RAYCASTER
            DrawRaycastView(player, objects, config);
#endif
        }
        EndDrawing();
    }
    CloseWindow();

    return 0;
}
