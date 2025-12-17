#include "ButtonManager.h"

void handleLongClick(Button2& b) {
    // Can use b to map the buttons, since I know the order they begin it will be :
    // 0: left, 1: middle, 2: right
    printf("Button Long Clicked down!");
    printf("Button ID: %d\n", b.getID());
}
void handleShortClick(Button2& b) {
    // Can use b to map the buttons, since I know the order they begin it will be :
    // 0: left, 1: middle, 2: right
    printf("Button Short Clicked down!");
    printf("Button ID: %d\n", b.getID());
}