#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

typedef enum Result { untouched = 0, win = 1, lost = 2, visited = 3 } Result;

typedef enum Action {
    waiting = 0,
    bypass = 1,
    finished = 2,
    move_first = 8,
    move_last = 15 /* ,move_1_left = 8, move_2_left = 9, move_1_up = 10, ...
                      move_1_down = 14, move_2_down = 15  */
} Action;

typedef struct State {
    unsigned short blockmap;  // 障碍物位图
    unsigned char me : 4;     // 本方位置
    unsigned char enemy : 4;  // 敌方位置
    unsigned char action : 4;
    enum Result result : 2;
    unsigned char loop_found : 1;
    unsigned char is_sibling : 1;
} State;

struct State Stack[1 << (16 + 4 + 4 - 3)];
unsigned char ResultMap[1 << (16 + 4 + 4 - 2)];
const State invalid = {.blockmap = 0, .me = 0, .enemy = 0};

bool IsValid(State s) { return s.me != s.enemy; }

int action2direction(unsigned char action) {
    unsigned char action_direction = (action & 0x6) >> 1;
    return action_direction == 0x0   ? -1
           : action_direction == 0x1 ? 4
           : action_direction == 0x2 ? 1
                                     : -4;
}

int action2steps(unsigned char action) { return 1 + (action & 0x1); }

unsigned short reverseBits(unsigned short x) {
    unsigned short s = sizeof(x) * 8;
    unsigned short mask = 0xFFFF;
    while ((s >>= 1) > 0) {
        mask ^= mask << s;
        x = ((x >> s) & mask) | ((x << s) & ~mask);
    }
    return x;
}

int NewPos(int pos, int direction, int count) {
    assert(direction == 1 || direction == -1 || direction == 4 ||
           direction == -4);
    assert(count == 1 || count == 2);
    int newpos = pos + direction * count;
    if (newpos < 0 || newpos > 0xF) return -1;
    if ((newpos >> 2) != (pos >> 2) && (newpos & 0x3) != (pos & 0x3)) return -1;
    return newpos;
}

State Advance(State s, int direction, int count) {
    const int offset = direction * count;
    int currpos = s.me;
    int newpos = NewPos(currpos, direction, count);
    if (newpos == -1) return invalid;

    do {
        currpos += direction;
        if (currpos == s.enemy || s.blockmap & (1 << currpos)) return invalid;
    } while (currpos != newpos);

    s.me = newpos;
    int futurePos = NewPos(newpos, direction, 1);
    if (futurePos == -1 || futurePos == s.enemy) return s;
    if (!(s.blockmap & (1 << futurePos))) {
        s.blockmap |= (1 << futurePos);
        return s;
    }
    if (count == 2) return s;
    for (;;) {
        int morefuture = NewPos(futurePos, direction, 1);
        if (morefuture == -1 || morefuture == s.enemy) {
            for (int cleanpos = s.me + direction;
                 cleanpos != futurePos + direction; cleanpos += direction) {
                s.blockmap ^= (1 << cleanpos);
            }
            return s;
        }
        if (!(s.blockmap & (1 << morefuture))) {
            s.blockmap ^= (1 << futurePos);
            s.blockmap ^= (1 << morefuture);
            return s;
        }
        futurePos = morefuture;
    }

    // unreachable here;
}

State TurnOver(State s) {
    State turned = {};
    turned.blockmap = reverseBits(s.blockmap);
    turned.enemy = 0x0F - s.me;
    turned.me = 0x0F - s.enemy;
    return turned;
}

int CalcNextState(State s, State post_states[8]) {
    int result_idx = 0;
    int directions[4] = {-4, -1, 1, 4};
    for (int idx = 0; idx < 4; ++idx) {
        const int direction = directions[idx];
        State next = Advance(s, direction, 1);
        if (IsValid(next)) {
            post_states[result_idx++] = next;
            State next = Advance(s, direction, 2);
            if (IsValid(next)) {
                post_states[result_idx++] = next;
            }
        }
    }
    if (result_idx == 0) {
        post_states[result_idx++] = s;
    }
    return result_idx;
}

Result FetchResult(State s) {
    bool turned = false;
    if (s.me > s.enemy) {
        s = TurnOver(s);
        turned = 1;
    }
    int offset = (s.blockmap << 6) | (s.me << 2) | (s.enemy >> 2);
    int shift = (s.enemy & 0x3) << 1;
    unsigned char mask = 0x3 << shift;
    Result r = (ResultMap[offset] & mask) >> shift;
    return turned ? ((r & 0x1) << 1) | (r >> 1) : r;
}

void SaveResult(State s, Result result) {
    if (s.me > s.enemy) {
        s = TurnOver(s);
        result = ((result & 0x1) << 1) | (result >> 1);
    }
    int offset = (s.blockmap << 6) | (s.me << 2) | (s.enemy >> 2);
    int shift = (s.enemy & 0x3) << 1;
    unsigned char mask = 0x3 << shift;
    ResultMap[offset] = (ResultMap[offset] & ~mask) | (result << shift);
}

bool valid(State s) {
    if (s.action == bypass || s.action == finished) return true;
    int direction = action2direction(s.action);
    int step = action2steps(s.action);
    int pos = s.me;
    for (; step > 0; --step) {
        pos = NewPos(pos, direction, 1);
        if (pos == -1 || pos == s.enemy || (s.blockmap & (1 << pos)) != 0)
            return false;
    }
    return true;
}
bool unmovable(State s) {
    State test_s = s;
    test_s.action = move_first;
    do {
        if (valid(test_s)) {
            return false;
        }
        ++test_s.action;
    } while (test_s.action != move_last);
    return true;
}

State next_try(State s) {
    if (s.action == bypass || s.action == move_last) {
        s.action = finished;
        return s;
    }
    if (unmovable(s)) {
        s.action = bypass;
        return s;
    }
    if (s.action == waiting) {
        s.action = move_first;
        return s;
    }
    s.action = s.action + 1;
    return s;
}

State next_action(State s) {
    s.is_sibling = s.action == waiting ? 0 : 1;
    while (s.action != finished) {
        s = next_try(s);
        if (valid(s)) {
            break;
        }
    }
    return s;
}

State ExecuteAction(State s) {
    int direction = action2direction(s.action);
    int count = action2steps(s.action);
    return Advance(s, direction, count);
}
State Transfer(State s, Result lastResult) {
    if (s.me == 0xF) {
        s.action = finished;
        s.result = win;
        return s;
    } else if (s.enemy == 0x0) {
        s.action = finished;
        s.result = lost;
        return s;
    }
    if (s.action == waiting) {
        Result cached = FetchResult(s);
        if (cached != untouched) {
            s.result = cached;
            s.action = finished;
            return s;
        }
        SaveResult(s, visited);
        s = next_action(s);
        s.result = untouched;
        return s;
    }
    if (lastResult == untouched) {  // untouched == from parent or sibling
        if (s.action == bypass) {
            return TurnOver(s);
        }
        int direction = action2direction(s.action);
        int count = action2steps(s.action);
        State executed = Advance(s, direction, count);
        if (executed.me == 0xF) {
            s.result = win;
            s.action = finished;
            SaveResult(s, win);
            return s;
        }
        return TurnOver(executed);
    }
    if (lastResult == lost) {
        s.result = win;
        s.action = finished;
        SaveResult(s, win);
        return s;
    }
    if (lastResult == visited) {
        s.loop_found = 1;
    }
    s = next_action(s);
    if (s.action != finished) {
        return s;
    }
    s.result = s.loop_found ? visited : lost;
    SaveResult(s, s.loop_found ? untouched : lost);
    return s;
}

void dump(State s, int depth) {
    printf(
        "depth:%d,s={blockmap:%u,me:%u,enemy:%u,action:%u,result:%d,loop_found:"
        "%d,is_sibling:%d}\n",
        depth, s.blockmap, s.me, s.enemy, s.action, s.result, s.loop_found,
        s.is_sibling);
    if (depth & 0x1 == 0x1) s = TurnOver(s);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            if (j == 0)
                for (int k = 0; k < (4 * depth % 27); ++k) printf(" ");
            int pos = i * 4 + j;
            if (s.me == pos)
                printf("A");
            else if (s.enemy == pos)
                printf("B");
            else if ((s.blockmap & (1 << pos)) != 0)
                printf("X");
            else
                printf(" ");
            if (j == 3) printf("\n");
        }
}
Result search(State s) {
    int depth = 0;
    const int MAX_STEP = 1000000000;
    int step = MAX_STEP;
    Stack[depth] = s;
    Result lastResult = untouched;
    do {
        State prevS = Stack[depth];
        //dump(prevS, depth);
        State s = Transfer(prevS, lastResult);
        lastResult = s.result;
        if (s.action == finished) {
            lastResult = s.result;
            --depth;
        } else if (s.action == waiting) {
            Stack[++depth] = s;
            lastResult = untouched;
        } else {
            Stack[depth] = s;
            lastResult = untouched;
        }
    } while (depth >= 0 && --step > 0);
    printf("search step = %d\n", MAX_STEP - step);
    return lastResult;
}

int main() {
    State s = {};
    s.blockmap = 0x0000;
    s.me = 0x0;
    s.enemy = 0xf;
    Result r = search(s);
    printf("result = %d\n", r);
    return 0;
}
