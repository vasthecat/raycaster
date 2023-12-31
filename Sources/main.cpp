#include <raylib-ext.hpp>
#include <algorithm>
#include <raylib.h>
#include <raymath.h>
#include <vector>
#include <iostream>
#include <cmath>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

#define DRAW_VIEW_RAYS
#define DRAW_COLLISIONS
// #define USE_SHADING
// #define DRAW_RAYS_TO_OBJECTS

const int screen_width = 1024;
const int screen_height = 768;
const float mouse_sensetivity = 3;

const int board_w = 8;
const int board_h = 9;

const int cell_size = 80;

int board[board_w][board_h] = {
    { 1, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 2, 0, 0, 0, 1, 0, 1 },
    { 1, 2, 2, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 1, 0, 0, 0, 0, 0, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};

Image images[] = {
    LoadImage("./Assets/textures/TECH_1A.png"), // NULL
    LoadImage("./Assets/textures/TECH_1A.png"),
    LoadImage("./Assets/textures/SUPPORT_3.png"),
};

Image floor_img = LoadImage("./Assets/textures/FLOOR_1A.png");
Image ceiling_img = LoadImage("./Assets/textures/LIGHT_1C.png");

struct Player {
    Vector2 pos;
    float rotation;
    float speed;
};

struct Object {
    size_t id;
    Vector2 pos;
    Image image;

    Object()
    {
        static size_t counter = 0;
        id = counter++;
    }
};

struct CellPos {
    int x, y;
    CellPos() : x(0), y(0) {};
    CellPos(int x, int y) : x(x), y(y) {};
    CellPos(Vector2 v) : x(int(v.x)), y(int(v.y)) {};
    bool operator==(const CellPos &p) const
    {
        return p.x == x && p.y == y;
    }
    bool operator!=(const CellPos &p) const
    {
        return p.x != x || p.y != y;
    }

};

std::ostream &operator<<(std::ostream &stream, const CellPos &c)
{
    stream << c.x << ' ' << c.y;
    return stream;
}

struct hash_fn
{
    std::size_t operator() (const CellPos &p) const
    {
        std::size_t h1 = std::hash<int>()(p.x);
        std::size_t h2 = std::hash<int>()(p.y);
        return h1 ^ h2;
    }
};

struct RayHit {
    Vector2 pos;
    CellPos cell_pos;
    bool is_horizontal;
    float angle;
};

struct Collision {
    Vector2 pos;
    CellPos cell;
};

struct RaycastConfig
{
    float fov;
    int rays_count;
    float delta_angle;
    float rect_w;
    RenderTexture minimap;
    bool draw_map;
};

RaycastConfig config;

inline Vector2
get_cell(const Vector2 &pos)
{
    return Vector2 {
        std::floor(pos.x / cell_size),
        std::floor(pos.y / cell_size),
    };
}

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

std::vector<Collision>
find_collisions(const Vector2 &pos, float radius)
{
    float radius_sqr = radius * radius;
    std::vector<Collision> collisions;
    CellPos cell = CellPos(get_cell(pos));

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 && j == 0) continue;
            CellPos neighbour(cell.x + i, cell.y + j);
            if (!correct_cell(neighbour)) continue;
            if (board[neighbour.x][neighbour.y] == 0) continue;

            Vector2 collision = pos;
            if (neighbour.x < cell.x)
                collision.x = cell.x * 1.0f * cell_size;
            else if (neighbour.x > cell.x)
                collision.x = cell.x * 1.0f * cell_size + cell_size;

            if (neighbour.y < cell.y)
                collision.y = cell.y * 1.0f * cell_size;
            else if (neighbour.y > cell.y)
                collision.y = cell.y * 1.0f * cell_size + cell_size;

            if (Vector2LengthSqr(collision - pos) <= radius_sqr)
            {
                collisions.push_back(Collision {
                    collision,
                    neighbour,
                });
            }
        }
    }

    return collisions;
}

void
draw_top_down_view(const Player &player,
                   const std::vector<RayHit> &hits,
                   const std::vector<Object> &objects)
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

    Vector2 player_cell = get_cell(player.pos);
    DrawRectangleV(player_cell * cell_size, {cell_size, cell_size}, PURPLE);
#ifdef DRAW_VIEW_RAYS
    for (auto &hit : hits)
        DrawLineEx(player.pos, hit.pos, 2, BLUE);
#endif
    for (auto &object : objects)
    {
        Vector2 player_to_object = object.pos - player.pos;
        Vector2 anti_normal = Vector2Normalize(Vector2Rotate(player_to_object, 90 * DEG2RAD));

        Vector2 a = object.pos - anti_normal * cell_size / 2;
        Vector2 b = object.pos + anti_normal * cell_size / 2;
        DrawLineEx(a, b, 5, RED);
    }

    DrawCircleV(player.pos, 10, RED);
    DrawLineEx(player.pos, player.pos + Vector2Rotate({ 1,0 }, player.rotation) * 25, 3, RED);
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

std::vector<size_t>
get_render_order(const Player &player, const std::vector<Object> &objects)
{
    std::vector<std::pair<float, size_t>> distances;
    for (size_t i = 0; i < objects.size(); i++)
    {
        float dist = Vector2LengthSqr(objects[i].pos - player.pos);
        distances.emplace_back(dist, i);
    }
    std::sort(distances.begin(), distances.end(), std::greater<>());

    std::vector<size_t> order;
    for (auto &p : distances)
        order.push_back(p.second);
    return order;
}

Color
blend_colors(Color c1, Color c2, float val)
{
    Color blend = (c1 * (1.0f - val) + c2 * val);
    blend.a = c1.a;
    return blend;
}

void
draw_raycast_view(const Player &player,
                  const std::vector<RayHit> &hits,
                  const std::vector<Object> &objects,
                  const RaycastConfig &config)
{
    int horizon = screen_height / 2.0f;
    float cam_height = 0.5f * screen_height;
    float light_dist = 200.0f;

    float rect_x = 0;
    for (auto &hit : hits)
    {
        float floor_pix_h = 3;
        Vector2 hit_delta = hit.pos - player.pos;
        float dist =
            hit_delta.x * cos(player.rotation) +
            hit_delta.y * sin(player.rotation);

        // floor and ceiling
        Vector2 ray0 = Vector2Normalize(hit_delta);
        float angle = fix_angle(hit.angle - player.rotation + config.fov / 2.0);
        int x = int(angle / config.fov * screen_width);
        float floor_dist = dist / Vector2Length(hit_delta);
        for(int y = 0; y < horizon; y += floor_pix_h)
        {
            float row_dist = cam_height / (horizon - y) / floor_dist;
            Vector2 floor_pos = player.pos / cell_size + ray0 * row_dist;
            Vector2 cell = Vector2 {
                std::floor(floor_pos.x),
                std::floor(floor_pos.y),
            };
            Vector2 uv = floor_pos - cell;

            Color floor_pix = sample_uv(floor_img, uv);
            Color ceiling_pix = sample_uv(ceiling_img, uv);

#ifdef USE_SHADING
            float blend = row_dist * cell_size / light_dist;
            blend *= blend;
            if (blend > 1.0f) blend = 1.0f;
            floor_pix = blend_colors(floor_pix, BLACK, blend);
            ceiling_pix = blend_colors(ceiling_pix, BLACK, blend);
#endif

            DrawRectangle(
                x, screen_height - y,
                config.rect_w + 1, floor_pix_h,
                floor_pix
            );
            DrawRectangle(
                x, y,
                config.rect_w + 1, floor_pix_h,
                ceiling_pix
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

        float wall_pix_h = rect_h / cell_image.height;
        for (int i = 0; i < cell_image.height; ++i)
        {
            Color* color_data = (Color*)cell_image.data;
            Color pixel = color_data[i * cell_image.width + col];

#ifdef USE_SHADING
            float blend = dist / light_dist;
            blend *= blend;
            if (blend > 1.0f) blend = 1.0f;
            pixel = blend_colors(pixel, BLACK, blend);
#endif

            DrawRectangle(
                rect_x - 1, rect_y + wall_pix_h * i,
                config.rect_w + 2, std::ceil(wall_pix_h),
                pixel
            );
        }

        rect_x += config.rect_w;
    }

    std::vector<size_t> render_order = get_render_order(player, objects);
    for (size_t i : render_order)
    {
        const Object &object = objects[i];
        Vector2 player_to_object = object.pos - player.pos;
        Vector2 anti_normal = Vector2Normalize(Vector2Rotate(player_to_object, 90 * DEG2RAD));

        Vector2 a = object.pos - anti_normal * cell_size / 2;
        Vector2 b = object.pos + anti_normal * cell_size / 2;

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
#ifdef DRAW_RAYS_TO_OBJECTS
                BeginTextureMode(config.minimap);
                DrawLineV(player.pos, point, GREEN);
                EndTextureMode();
#endif

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

#ifdef USE_SHADING
                    float blend = dist / light_dist;
                    blend *= blend;
                    if (blend > 1.0f) blend = 1.0f;
                    pixel = blend_colors(pixel, BLACK, blend);
#endif

                    DrawRectangle(
                        rect_x, rect_y + pix_h * i,
                        rect_w + 1, std::ceil(pix_h),
                        pixel
                    );
                }
            }
#ifdef DRAW_RAYS_TO_OBJECTS
            else
            {
                BeginTextureMode(config.minimap);
                DrawLineEx(player.pos, hit.pos, 2, MAGENTA);
                EndTextureMode();
            }
#endif
        }
#ifdef DRAW_RAYS_TO_OBJECTS
        BeginTextureMode(config.minimap);
        DrawLineEx(player.pos, a, 5, BLACK);
        DrawLineEx(player.pos, b, 5, PURPLE);
        EndTextureMode();
#endif
    }
}

void
fix_collisions(Player &player, const Vector2 &move_dir, float dt)
{
    const int collision_radius = 25;
    Vector2 move = move_dir * (player.speed * dt);
    std::vector<Collision> collisions = find_collisions(player.pos + move, collision_radius);
    player.pos += move;
    if (!collisions.empty())
    {
        for (auto &collision : collisions)
        {
            if (Vector2Length(collision.pos - player.pos) >= collision_radius)
                continue;
            Vector2 fix = Vector2Normalize(player.pos - collision.pos) * collision_radius + collision.pos;
            player.pos += fix - player.pos;
#ifdef DRAW_COLLISIONS
            BeginTextureMode(config.minimap);
                DrawRectangle(
                    collision.cell.x * cell_size,
                    collision.cell.y * cell_size,
                    cell_size, cell_size, GREEN
                );
                DrawCircleV(collision.pos, 4, MAGENTA);
                DrawCircleV(fix, 4, MAGENTA);
            EndTextureMode();
#endif
        }
    }
}

std::vector<CellPos>
find_path(Vector2 from, Vector2 to)
{
    CellPos cell0(from / cell_size);
    CellPos cell_fin(to / cell_size);

    std::unordered_set<CellPos, hash_fn> visited;
    std::unordered_map<CellPos, CellPos, hash_fn> trace;
    std::unordered_set<CellPos, hash_fn> front = { cell0 };

    CellPos prev = cell0;
    bool found = false;
    while (!found)
    {
        CellPos cur;
        int heuristic = INT_MAX;
        for (auto &cell : front)
        {
            int h = std::abs(cell.x - cell_fin.x) + std::abs(cell.y - cell_fin.y);
            if (h < heuristic)
            {
                heuristic = h;
                cur = cell;
            }
        }

        front.erase(cur);
        visited.insert(cur);
        prev = cur;

        std::vector<CellPos> neighbours;
        neighbours.emplace_back(cur.x - 1, cur.y);
        neighbours.emplace_back(cur.x + 1, cur.y);
        neighbours.emplace_back(cur.x, cur.y - 1);
        neighbours.emplace_back(cur.x, cur.y + 1);

        for (auto &neighbour : neighbours)
        {
            if (neighbour == cell_fin)
            {
                trace[neighbour] = cur;
                found = true;
                break;
            }
            else if (correct_cell(neighbour) &&
                     board[neighbour.x][neighbour.y] == 0 &&
                     visited.find(neighbour) == visited.end())
            {
                front.insert(neighbour);
                trace[neighbour] = cur;
            }
        }
    }

    // TODO: check if there is no path
    std::vector<CellPos> path;
    CellPos tracer = cell_fin;
    while (tracer != cell0)
    {
        path.push_back(tracer);
        tracer = trace.at(tracer);
    }
    path.push_back(cell0);

    BeginTextureMode(config.minimap);
    for (int i = 0; i < int(path.size()) - 1; i++)
    {
        CellPos c0 = path[i];
        CellPos c1 = path[i + 1];
        Vector2 p0 = {
            c0.x * 1.0f * cell_size + cell_size / 2.0f,
            c0.y * 1.0f * cell_size + cell_size / 2.0f,
        };
        Vector2 p1 = {
            c1.x * 1.0f * cell_size + cell_size / 2.0f,
            c1.y * 1.0f * cell_size + cell_size / 2.0f,
        };
        DrawLineEx(p0, p1, 10, MAGENTA);
    }
    EndTextureMode();
    std::reverse(path.begin(), path.end());
    return path;
}

void
shoot(const Player &player, std::vector<Object> &objects)
{
    size_t to_destroy;
    bool found = false;
    for (auto &object : objects)
    {
        Vector2 player_to_object = object.pos - player.pos;
        Vector2 anti_normal = Vector2Normalize(Vector2Rotate(player_to_object, 90 * DEG2RAD));

        Vector2 a = object.pos - anti_normal * cell_size / 2;
        Vector2 b = object.pos + anti_normal * cell_size / 2;

        float start_angle = fix_angle(Vector2Angle({1, 0}, a - player.pos));
        float end_angle = fix_angle(Vector2Angle({1, 0}, b - player.pos));
        float angle = fix_angle(player.rotation);

        // TODO: this is not really working
        if (start_angle < angle && angle < end_angle)
        {
            to_destroy = object.id;
            found = true;
            break;
        }
    }
    if (found)
    {
        objects.erase(
            std::remove_if(
                objects.begin(),
                objects.end(),
                [to_destroy](Object &c) {
                    return c.id == to_destroy;
                }
            ),
            objects.end()
        );
    }
}

void
draw_hands(Texture2D &hands)
{
    float scale = 3;
    Vector2 position = {
        (screen_width - hands.width * scale) / 2.0f,
        screen_height - hands.height * scale,
    };
    DrawTextureEx(hands, position, 0, scale, WHITE);
}

void
draw_crosshair()
{
    float size = 20;
    Vector2 center = { screen_width / 2.0f, screen_height / 2.0f };
    DrawLineEx(
        center - Vector2 { size, 0 },
        center + Vector2 { size, 0 },
        3, Color { 255, 255, 255, 128 }
    );
    DrawLineEx(
        center - Vector2 { 0, size },
        center + Vector2 { 0, size },
        3, Color { 255, 255, 255, 128 }
    );
}

int main()
{
    InitWindow(screen_width, screen_height, "Raycaster");
    // SetTargetFPS(60);
    Texture2D hands = LoadTexture("./Assets/textures/hands.png");

    Player player;
    player.pos = { 5.0f * cell_size, 5.0f * cell_size };
    player.speed = 150;
    player.rotation = 0;

    std::vector<Object> objects;

    Object barrel;
    barrel.pos = { 3 * cell_size, 5 * cell_size };
    barrel.image = LoadImage("./Assets/textures/barrel.png");
    objects.push_back(barrel);

    Object barrel2;
    barrel2.pos = { 3 * cell_size, 4 * cell_size };
    barrel2.image = LoadImage("./Assets/textures/enemy1.png");
    objects.push_back(barrel2);

    Object barrel3;
    barrel3.pos = { 2 * cell_size, 2 * cell_size };
    barrel3.image = LoadImage("./Assets/textures/michael.png");
    objects.push_back(barrel3);

    config.fov = 75 * DEG2RAD;
    config.rays_count = screen_width / 4;
    config.delta_angle = config.fov / config.rays_count;
    config.rect_w = (screen_width / config.fov) * config.delta_angle;
    config.minimap = LoadRenderTexture(screen_width, screen_height);
    config.draw_map = false;

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        Vector2 move_dir = { 0, 0 };
        DisableCursor();
        float delta = GetMouseDelta().x;
        player.rotation += delta * 1e-3f * mouse_sensetivity;
        Vector2 dir = Vector2Rotate({ 1, 0 }, player.rotation);
        Vector2 forward = dir;
        Vector2 right = Vector2Rotate(forward, PI / 2);
        if (IsKeyDown(KEY_W))
            move_dir += forward;
        if (IsKeyDown(KEY_S))
            move_dir += -forward;
        if (IsKeyDown(KEY_A))
            move_dir += -right;
        if (IsKeyDown(KEY_D))
            move_dir += right;
        move_dir = Vector2Normalize(move_dir);
        
        if (IsKeyPressed(KEY_T))
            config.draw_map = !config.draw_map;

        if (IsKeyPressed(KEY_SPACE))
            shoot(player, objects);

        std::vector<RayHit> hits;
        for (float angle = -config.fov / 2; angle < config.fov / 2; angle += config.delta_angle) {
            Vector2 d = {
                cos(player.rotation + angle),
                sin(player.rotation + angle),
            };
            RayHit hit = cast_ray(player.pos, d);
            hits.push_back(hit);
        }

        BeginTextureMode(config.minimap);
        draw_top_down_view(player, hits, objects);
        EndTextureMode();

        if (objects.size() > 1)
        {
            std::vector<CellPos> path = find_path(objects[1].pos, player.pos);
            if (path.size() > 1)
            {
                CellPos c0 = path[1];
                Vector2 p0 = {
                    c0.x * 1.0f * cell_size + cell_size / 2.0f,
                    c0.y * 1.0f * cell_size + cell_size / 2.0f,
                };

                Vector2 move = Vector2Normalize(p0 - objects[1].pos) * dt * 30;
                objects[1].pos += move;
            }
        }

        if (objects.size() > 2)
        {
            std::vector<CellPos> path = find_path(objects[2].pos, player.pos);
            if (path.size() > 1)
            {
                CellPos c0 = path[1];
                Vector2 p0 = {
                    c0.x * 1.0f * cell_size + cell_size / 2.0f,
                    c0.y * 1.0f * cell_size + cell_size / 2.0f,
                };

                Vector2 move = Vector2Normalize(p0 - objects[2].pos) * dt * 40;
                objects[2].pos += move;
            }
        }


        BeginDrawing();
        {
            ClearBackground(BLACK);
            draw_raycast_view(player, hits, objects, config);
            fix_collisions(player, move_dir, dt);
            draw_hands(hands);
            draw_crosshair();
        }
        if (config.draw_map)
            DrawTexture(config.minimap.texture, 0, 0, WHITE);
        DrawFPS(10, 10);
        EndDrawing();
    }
    CloseWindow();

    return 0;
}
