#include <raylib-ext.hpp>
#include <algorithm>
#include <raymath.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <chrono>

const int screen_width = 640;
const int screen_height = 640;

const int board_w = 8;
const int board_h = 8;

const int cell_size = screen_width / board_w;

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

struct Player {
    Vector2 pos;
    float rotation;
    float speed;
};

struct Object {
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
draw_top_down_view(const Player &player)
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

    for (int x = cell_size; x < screen_width; x += cell_size) {
        DrawLine(x, 0, x, screen_height, GRAY);
    }
    for (int y = cell_size; y < screen_height; y += cell_size) {
        DrawLine(0, y, screen_width, y, GRAY);
    }

    DrawCircleV(player.pos, 14, RED);
    DrawLineEx(player.pos, player.pos + Vector2Rotate({ 1,0 }, player.rotation) * 25, 5, BLUE);
}

Image images[] = {
    LoadImage("./Assets/textures/TECH_1A.png"), // NULL
    LoadImage("./Assets/textures/TECH_1A16.png"),
    LoadImage("./Assets/textures/SUPPORT_3.png"),
};

Image floor_img = LoadImage("./Assets/textures/FLOOR_1A.png");
Image ceiling_img = LoadImage("./Assets/textures/LIGHT_1C.png");

struct CellPos {
    int x, y;
    CellPos() : x(0), y(0) {};
    CellPos(int x, int y) : x(x), y(y) {};
    CellPos(Vector2 v) : x(int(v.x)), y(int(v.y)) {};
};

struct RayHit {
    Vector2 pos;
    CellPos cell_pos;
    bool is_horizontal;
    float angle;
};

inline bool
correct_cell(int x, int y)
{
    return (x >= 0 && x < board_w) && (y >= 0 && y < board_h);
}

inline bool
correct_cell(CellPos pos)
{
    return correct_cell(pos.x, pos.y);
}

inline float
fix_angle(float angle)
{
    while (angle > PI)  angle -= 2 * PI;
    while (angle < -PI) angle += 2 * PI;
    return angle;
}

RayHit
cast_ray(Vector2 pos, Vector2 dir)
{
    float angle = Vector2Angle({1, 0}, dir);
    Vector2 cell = pos / cell_size;

    RayHit hit_h;
    hit_h.angle = angle;
    hit_h.is_horizontal = true;

    float hor_mag = (std::round((dir.y < 0 ? -0.5 : 0.5) + cell.y) - cell.y) / dir.y * cell_size;
    float hor_move = std::abs(cell_size / dir.y);
    hit_h.pos = pos + dir * hor_mag;

    for (int k = 0; ; ++k) {
        hit_h.cell_pos.x = int(hit_h.pos.x / cell_size);
        hit_h.cell_pos.y = int((hit_h.pos.y + dir.y * 0.5f * cell_size) / cell_size);
        bool correct = correct_cell(hit_h.cell_pos);
        if (!correct) break;
        bool is_wall = board[hit_h.cell_pos.x][hit_h.cell_pos.y] != 0;
        if (correct && is_wall) break;
        hit_h.pos += dir * hor_move;
    }

    RayHit hit_v;
    hit_v.angle = angle;
    hit_v.is_horizontal = false;

    float vert_mag = (std::round((dir.x < 0 ? -0.5 : 0.5) + cell.x) - cell.x) / dir.x * cell_size;
    float vert_move = std::abs(cell_size / dir.x);
    hit_v.pos = pos + dir * vert_mag;

    for (int k = 0; ; ++k) {
        hit_v.cell_pos.x = int((hit_v.pos.x + dir.x * 0.5f * cell_size) / cell_size);
        hit_v.cell_pos.y = int(hit_v.pos.y / cell_size);
        bool correct = correct_cell(hit_v.cell_pos);
        if (!correct) break;
        bool is_wall = board[hit_v.cell_pos.x][hit_v.cell_pos.y] != 0;
        if (correct && is_wall) break;
        hit_v.pos += dir * vert_move;
    }

    if (Vector2LengthSqr(hit_h.pos - pos) < Vector2LengthSqr(hit_v.pos - pos))
        return hit_h;
    else
        return hit_v;
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

Color
sample_uv(const Image &img, const Vector2 uv)
{
    int tx = int(img.width * uv.x);
    int ty = int(img.height * uv.y);
    Color *color_data = (Color *) img.data;
    return color_data[img.width * ty + tx];
}

void
draw_raycast_view(const Player &player, const std::vector<Object> &objects,
                  const RaycastConfig &config)
{
    std::vector<RayHit> hits;
    for (float angle = -config.fov / 2; angle < config.fov / 2; angle += config.delta_angle) {
        Vector2 d = {
            cos(player.rotation + angle),
            sin(player.rotation + angle),
        };
        RayHit hit = cast_ray(player.pos, d);
        DrawLineEx(player.pos, hit.pos, 2, BLUE);
        hits.push_back(hit);
    }

    int horizon = screen_height / 2;
    float cam_height = 0.5 * screen_height;

    float rect_x = 0;
    for (RayHit& hit : hits)
    {
        Vector2 hit_delta = hit.pos - player.pos;
        float dist =
            hit_delta.x * cos(player.rotation) +
            hit_delta.y * sin(player.rotation);

        // floor and ceiling
        Vector2 ray0 = Vector2Normalize(hit_delta);
        float angle = fix_angle(hit.angle - player.rotation + config.fov / 2.0);
        int x = int(angle / config.fov * screen_width);
        float floor_dist = dist / Vector2Length(hit_delta);
        for(int y = 0; y < horizon; y++)
        {
            float row_dist = cam_height / (horizon - y) / floor_dist;
            Vector2 floor_pos = player.pos / cell_size + ray0 * row_dist;
            Vector2 cell = Vector2 {
                std::floor(floor_pos.x),
                std::floor(floor_pos.y),
            };
            Vector2 uv = floor_pos - cell;

            DrawRectangle(
                screen_width + x, screen_height - y,
                config.rect_w + 1, 1,
                sample_uv(floor_img, uv)
            );
            DrawRectangle(
                screen_width + x, y,
                config.rect_w + 1, 1,
                sample_uv(ceiling_img, uv)
            );
        }

        // walls
        float rect_h = (cell_size * screen_height) / dist;
        float rect_y = (screen_height - rect_h) / 2;

        int image_idx = board[hit.cell_pos.x][hit.cell_pos.y];

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

        // TODO: fix if img.height is greater than rect_h
        for (int i = 0; i < cell_image.height; ++i)
        {
            Color* color_data = (Color*)cell_image.data;
            Color pixel = color_data[i * cell_image.width + col];

            DrawRectangle(
                screen_width + rect_x - 1, rect_y + pix_h * i,
                config.rect_w + 2, pix_h + 2, pixel
            );
        }

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
            RayHit hit = cast_ray(player.pos, Vector2Normalize(point - player.pos));

            auto point_delta = point - player.pos;
            float dist = Vector2Length(point_delta);
            if (dist < Vector2Length(hit.pos - player.pos))
            {
                // DrawLineEx(player.pos, point, 2, GREEN);
                float rect_h = (cell_size * screen_height) / dist;
                float rect_y = (screen_height - rect_h) / 2;

                float rect_xa = fix_angle(Vector2Angle(Vector2Rotate(dir, -config.fov / 2), a - player.pos)) / config.fov * screen_width;
                float rect_xb = fix_angle(Vector2Angle(Vector2Rotate(dir, -config.fov / 2), b - player.pos)) / config.fov * screen_width;
                float rect_w = std::abs(rect_xb - rect_xa) / rays_count;

                rect_x = fix_angle(Vector2Angle(Vector2Rotate(dir, -config.fov / 2), point - player.pos)) / config.fov * screen_width;
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
                        screen_width + rect_x, rect_y + pix_h * i,
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

int main()
{
    InitWindow(screen_width * 2, screen_height, "Raycaster");
    SetTargetFPS(60);

    Player player;
    player.pos = { screen_width / 2., screen_height / 2. };
    player.speed = 100;
    player.rotation = 0;

    std::vector<Object> objects;

    Object barrel;
    barrel.pos = { 3 * cell_size, 5 * cell_size };
    barrel.image = LoadImage("./Assets/textures/barrel.png");
    objects.push_back(barrel);

    Object barrel2;
    barrel2.pos = { 3 * cell_size, 5 * cell_size };
    barrel2.image = LoadImage("./Assets/textures/barrel.png");
    objects.push_back(barrel2);

    RaycastConfig config;
    config.fov = 60 * DEG2RAD;
    config.rays_count = screen_width / 2;
    config.delta_angle = config.fov / config.rays_count;
    config.rect_w = (screen_width / config.fov) * config.delta_angle;

    float t = 0;
    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        t += dt;

        objects[1].pos.x = 3 * cell_size + 100 * sin(t);

        Vector2 move = { 0, 0 };
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

        player.pos += move;
        if (check_collision(player.pos, 15))
            player.pos -= move;

        BeginDrawing();
        {
            ClearBackground(BLACK);
            draw_top_down_view(player);
            draw_raycast_view(player, objects, config);
        }
        EndDrawing();
    }
    CloseWindow();

    return 0;
}
