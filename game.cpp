#include "game.hpp"

using namespace blit;

#define SCALE_FACTOR 4
#define X_LIMIT 128/SCALE_FACTOR
#define Y_LIMIT 128/SCALE_FACTOR

Point segments[1024] = {Point(16,16)};
int size = 1;
Point food_position = Point(10,10);

enum direction{
    UP,
    DOWN,
    LEFT,
    RIGHT,
};

direction current_direction = UP;

uint32_t timer = 0;

void draw_snake(){
    screen.pen = Pen(0,255,0);
    for (int i = 0; i < size; i++){
        screen.rectangle(Rect(segments[i].x*SCALE_FACTOR, segments[i].y*SCALE_FACTOR, SCALE_FACTOR, SCALE_FACTOR));
    }
}

void grow_snake(){
    if(size < 1024){
        segments[size] = segments[size-1];
        size++;
    }
}

void draw_food(){
    screen.pen = Pen(255,0,0);
    screen.rectangle(Rect(food_position.x*SCALE_FACTOR, food_position.y*SCALE_FACTOR, SCALE_FACTOR, SCALE_FACTOR));
}

void spawn_food(){
    Point new_position = Point(rand() % X_LIMIT, rand() % Y_LIMIT);
    for (int i = 0; i < size; i++){
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

void move_snake(){
    for (int i = size-1; i > 0; i--){
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
        current_direction = UP;
    }
    else if (buttons & Button::DPAD_DOWN){
        current_direction = DOWN;
    }
    else if (buttons & Button::DPAD_RIGHT){
        current_direction = RIGHT;
    }
    else if (buttons & Button::DPAD_LEFT){
        current_direction = LEFT;
    }
}

void init() {
    set_screen_mode(ScreenMode::hires);
}

void render(uint32_t time) {
    screen.pen = Pen(0,0,0);
    screen.clear();
    draw_food();
    draw_snake();
}

void update(uint32_t time) {

    get_input();
    
    if (time - timer > 400){
        check_food();
        move_snake();
        timer = time;
    }

}