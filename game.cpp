#include "game.hpp"

using namespace blit;

#define SCALE_FACTOR 4
#define X_LIMIT 128/SCALE_FACTOR
#define Y_LIMIT 128/SCALE_FACTOR

Point segments[1024] = {Point(16,16)};
int snake_size = 1;
Point food_position = Point(10,10);

enum direction{
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

direction current_direction = UP;
direction input_direction = UP;

uint32_t timer = 0;

bool game_over = false;

void reset_game(){
    segments[0] = Point(16,16);
    snake_size = 1;
    food_position = Point(10,10);
    current_direction = UP;
    timer = 0;
    game_over = false;
}

void draw_snake(){
    screen.pen = Pen(0,255,0);
    for (int i = 0; i < snake_size; i++){
        screen.rectangle(Rect(segments[i].x*SCALE_FACTOR, segments[i].y*SCALE_FACTOR, SCALE_FACTOR, SCALE_FACTOR));
    }
}

void grow_snake(){
    if(snake_size < 1024){
        segments[snake_size] = segments[snake_size-1];
        snake_size++;
    }
}

void draw_food(){
    screen.pen = Pen(255,0,0);
    screen.rectangle(Rect(food_position.x*SCALE_FACTOR, food_position.y*SCALE_FACTOR, SCALE_FACTOR, SCALE_FACTOR));
}

void spawn_food(){
    Point new_position = Point(rand() % X_LIMIT, rand() % Y_LIMIT);
    for (int i = 0; i < snake_size; i++){
        if (segments[i] == new_position){
            spawn_food();
            return;
        }
    }
    food_position = new_position;
}

void check_food(){
    if (segments[0] == food_position){
        grow_snake();
        spawn_food();
    }
}

void check_game_over(){
    for (int i = 1; i < snake_size; i++){
        if (segments[0] == segments[i]){
            game_over = true;
            return;
        }
    }
}

void move_snake(){
    for (int i = snake_size-1; i > 0; i--){
        segments[i] = segments[i-1];
    }

    Point new_head = segments[0];
    switch (current_direction)
    {
    case UP:
        new_head.y--;
        break;
    case DOWN:
        new_head.y++;
        break;
    case RIGHT:
        new_head.x++;
        break;
    case LEFT:
        new_head.x--;
        break;
    default:
        break;
    }

    // wrap around screen bounds
    if(new_head.x < 0) new_head.x = X_LIMIT - 1;
    if(new_head.x >= X_LIMIT) new_head.x = 0;
    if(new_head.y < 0) new_head.y = Y_LIMIT - 1;
    if(new_head.y >= Y_LIMIT) new_head.y = 0;

    segments[0] = new_head;
}

void get_input(){
    if (buttons & Button::DPAD_UP){
        if (current_direction != DOWN){
            input_direction = UP;
        }
    }
    else if (buttons & Button::DPAD_DOWN){
        if (current_direction != UP){
            input_direction = DOWN;
        }
    }
    else if (buttons & Button::DPAD_RIGHT){
        if (current_direction != LEFT){
            input_direction = RIGHT;
        }
    }
    else if (buttons & Button::DPAD_LEFT){
        if (current_direction != RIGHT){
            input_direction = LEFT;
        }
    }

    if (buttons & Button::A){
        if (game_over){
            reset_game();
        }
    }
}

void update_direction(){
    current_direction = input_direction;
}

void init() {
    set_screen_mode(ScreenMode::hires);
}

void render(uint32_t time) {
    screen.pen = Pen(0,0,0);
    screen.clear();
    if (game_over) {
        screen.pen = Pen(255,255,255);
        screen.text("Game Over", minimal_font, Point(64, 64), true, TextAlign::center_center);
        return;
    }
    draw_food();
    draw_snake();
}

void update(uint32_t time) {
    get_input();
    if (time - timer > 400){
        update_direction();
        check_food();
        move_snake();
        check_game_over();
        timer = time;
    }
}