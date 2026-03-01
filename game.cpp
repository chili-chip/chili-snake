#include "game.hpp"

using namespace blit;

#define SCALE_FACTOR 4
#define X_LIMIT 128/SCALE_FACTOR
#define Y_LIMIT 128/SCALE_FACTOR
#define SAVE_FILE "snek_save_data.bin"

Point segments[1024] = {Point(16,16)};
int snake_size = 1;
Point food_position = Point(10,10);
int highscore = 0;

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
bool highscore_saved = false;

int get_score(){
    return (snake_size - 1) * 100;
}

void load_highscore(){
    File highscore_file(SAVE_FILE, OpenMode::read);
    if (highscore_file.is_open()) {
        char buffer[4];
        highscore_file.read(0, 4, buffer);
        highscore = *((int*)buffer);
        highscore_file.close();
    } else {
        highscore = 0;
    }
}

void save_highscore(){
    File highscore_file(SAVE_FILE, OpenMode::write);
    if (highscore_file.is_open()) {
        int score_val = highscore;
        highscore_file.write(0, 4, (char*)&score_val);
        highscore_file.close();
    }
}

void reset_game(){
    segments[0] = Point(16,16);
    snake_size = 1;
    food_position = Point(10,10);
    current_direction = UP;
    timer = 0;
    game_over = false;
    highscore_saved = false;
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

void play_eating_sound(){
    channels[0].waveforms   = Waveform::SQUARE;
    channels[0].frequency   = 1047;
    channels[0].attack_ms   = 10;
    channels[0].decay_ms    = 150;
    channels[0].sustain     = 80;
    channels[0].release_ms  = 50;
    channels[0].volume      = 12000;
    channels[0].trigger_attack();
}

void check_food(){
    if (segments[0] == food_position){
        play_eating_sound();
        grow_snake();
        spawn_food();
    }
}

void check_game_over(){
    for (int i = 1; i < snake_size; i++){
        if (segments[0] == segments[i]){
            game_over = true;
            // Save highscore if current score is higher
            int current_score = get_score();
            if (current_score > highscore) {
                highscore = current_score;
                save_highscore();
            }
            highscore_saved = true;
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

void draw_score(){
    int score = (snake_size - 1) * 100;
    screen.pen = Pen(255,255,255);
    screen.text(std::to_string(score), minimal_font, Point(128, 0), false, TextAlign::top_right);
}

void draw_game_over(){
    screen.pen = Pen(255,255,255);
    screen.text("Game Over", minimal_font, Point(64, 60), true, TextAlign::center_center);
    int score = get_score();
    screen.text("Score: " + std::to_string(score), minimal_font, Point(64, 75), true, TextAlign::center_center);
    screen.text("High: " + std::to_string(highscore), minimal_font, Point(64, 85), true, TextAlign::center_center);
}

void init() {
    set_screen_mode(ScreenMode::hires);
    load_highscore();
}

void render(uint32_t time) {
    screen.pen = Pen(0,0,0);
    screen.clear();
    if (game_over) {
        draw_game_over();
        return;
    }
    draw_food();
    draw_snake();
    draw_score();
}

void update(uint32_t time) {
    get_input();
    if (time - timer > 200){
        update_direction();
        move_snake();
        check_game_over();
        check_food();
        timer = time;
    }
}